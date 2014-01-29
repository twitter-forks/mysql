/* Copyright (c) 2013, Twitter, Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "sql_class.h"
#include "sql_show.h"
#include "my_atomic.h"
#include "query_stats.h"

/* global query type stats hash table */
HASH query_stats_cache;

/* global query type stats cache limit */
uint query_stats_cache_count;

/* lightweight mutex latch for query stats reader */
int query_stats_reader;

#define ER_QUERY_STATS_PULL_ABORTED_MSG \
  "Operation to read query stats was aborted due to a conflicting lock"

#define SQL_TERM '\0'

#define IS_SPACE(c) \
  (c == ' ' || c == '\t' || c == '\n' || c == '\r')

#define IS_NUMERIC(c) \
  (c >= '0' && c <= '9')

#define IS_DELIMITER(c) \
  (c == ',')

#define IS_LEFT_PAREN(c) \
  (c == '(')

#define IS_RELATION_PRED_SIGN(c) \
  (c == '=' || c == '>' || c == '<' )

static inline int is_in_list(const char *sql, char *nextc)
{
  if ((sql[0] == 'i' || sql[0] == 'I') &&
      (sql[1] == 'n' || sql[1] == 'N'))
  {
    int i = 2;
    while (IS_SPACE(sql[i]))
      ++i;
    if (sql[i] == '(') {
      *nextc = ')';
      return i+1;
    }
  }
  return 0;
}

static inline int is_values_list(const char *sql, char *nextc)
{
  if ((sql[0] == 'v' || sql[0] == 'V') &&
      (sql[1] == 'a' || sql[1] == 'A') &&
      (sql[2] == 'l' || sql[2] == 'L') &&
      (sql[3] == 'u' || sql[3] == 'U') &&
      (sql[4] == 'e' || sql[4] == 'E') &&
      (sql[5] == 's' || sql[5] == 'S'))
  {
    int i = 6;
    while (IS_SPACE(sql[i]))
      ++i;
    if (sql[i] == '(') {
      *nextc = ')';
      return i+1;
    }
  }
  return 0;
}

static inline int is_like_expr(const char *sql, char *nextc)
{
  if ((sql[0] == 'l' || sql[0] == 'L') &&
      (sql[1] == 'i' || sql[1] == 'I') &&
      (sql[2] == 'k' || sql[2] == 'K') &&
      (sql[3] == 'e' || sql[3] == 'E'))
  {
    int i = 4;
    while (IS_SPACE(sql[i]))
      ++i;
    if (sql[i] == '\'') {
      *nextc = '\'';
      return i+1;
    }
  }
  return 0;
}

static inline int is_comments(const char *sql, char *nextc)
{
  if (sql[0] == '/' && sql[1] == '*')
  {
    int i = 2;
    *nextc = '*';
    return i;
  }
  return 0;
}

/* Format is "client_id" : "X" */
static inline int is_client_id(const char *sql, char *nextc)
{
  if( (sql[0] == 'c' || sql[0] == 'C') &&
      (sql[1] == 'l' || sql[1] == 'L') &&
      (sql[2] == 'i' || sql[2] == 'I') &&
      (sql[3] == 'e' || sql[3] == 'E') &&
      (sql[4] == 'n' || sql[4] == 'N') &&
      (sql[5] == 't' || sql[5] == 'T') &&
      (sql[6] == '_') &&
      (sql[7] == 'i' || sql[7] == 'I') &&
      (sql[8] == 'd' || sql[8] == 'D') )
  {
    int i = 9;
    while (IS_SPACE(sql[i]))
      ++i;
    if (sql[i] == '\"')
    {
      while (sql[i] != ':')
        ++i;
      while (sql[i] != '\"')
        ++i;
      *nextc = '\"';
      return i+1;
    }
  }
  return 0;
}

static int retrieve_client_id(const char *sql, char *client_id)
{
  int rsl = 0;
  char expect = '\0';
  const char *s = sql;
  DBUG_ASSERT(client_id != NULL);
  while (!(*s == '*' && *(s+1) == '/'))
  {
    if ((rsl = is_client_id(s, &expect)))
    {
      int len = 0;
      s += rsl;
      while (*s != expect)
      {
        if (len++ <= QUERY_ID_MAX_LENGTH)
          *client_id++ = *s;
        ++s;
      }
      *client_id = '\0';
    }
    else
      ++s;
  }
  return s - sql + 2;
}

static inline int is_negative_numeric(const char *sql)
{
  if (*sql == '-' && IS_NUMERIC(*(sql+1)) &&
      IS_SPACE(*(sql-1)))
  {
    return 1;
  }
  return 0;
}

static inline int is_numerics(const char *sql)
{
  if (IS_NUMERIC(*sql) &&
      (IS_RELATION_PRED_SIGN(*(sql-1)) || IS_SPACE(*(sql-1)) ||
       IS_DELIMITER(*(sql-1)) || IS_LEFT_PAREN(*(sql-1))))
  {
    int i = 1;
    while (IS_NUMERIC(sql[i]))
      ++i;
    return i;
  }
  return 0;
}

/* FIXME: edges_X_Y_edges or edges_X_Y_metadata */
static inline int is_edges_name_id(const char *sql)
{
  if (IS_NUMERIC(*sql))
  {
    char last = *(sql-1);
    if (last == '_' || (last == 'n' && *(sql-2) == '_'))
    {
      int i = 1;
      while (IS_NUMERIC(sql[i]))
        ++i;
      if (sql[i] == '_')
        return i;
    }
  }
  return 0;
}

static size_t sql_literal_replace(const char *query,
                                  uint32 query_length,
                                  char *qry_buf,
                                  char *client_id)
{
  const char *s = query;
  char *ns = qry_buf;

#define COPY_NCHAR(to, from, num) \
  do {                            \
    *to++ = *from++;              \
  } while (--num > 0);            \

  while (*s != '\0' && (s - query) < query_length)
  {
    int rsl = 0;
    char expect = '\0';

    if ((rsl = is_numerics(s)))
    {
      s += rsl;
      *ns++ = '?';
    }
    else if ((rsl = is_edges_name_id(s)))
    {
      s += rsl;
      *ns++ = '?';
    }
    else if ((rsl = is_in_list(s, &expect)) ||
             (rsl = is_values_list(s, &expect)) ||
             (rsl = is_like_expr(s, &expect)))
    {
      COPY_NCHAR(ns, s, rsl);
      if (expect == '\0')
        expect = ' ';
      while (*s != expect && *s != SQL_TERM && (s - query) < query_length)
        ++s;
      *ns++ = '?';
    }
    else if (is_negative_numeric(s)) {
      s += 1;
      while (IS_NUMERIC(*s))
        ++s;
      *ns++ = '?';
    }
    else if ((rsl = is_comments(s, &expect))) /* remove comment */
    {
      s += rsl;
      s += retrieve_client_id(s, client_id);
    }
    else if (*s == '\n' || *s == '\r')
    {
      *ns++ = ' ';
      ++s;
    }
    else {
      *ns++ = *s++;
    }
  }
  *ns = '\0';
  return ns - qry_buf;
}

static int query_stats_reader_lock()
{
  int old_reader = query_stats_reader;
  if (!old_reader && my_atomic_cas32(&query_stats_reader, &old_reader, 1))
  {
    return 1;
  }
  return 0;
}

static int query_stats_reader_unlock()
{
  int old_reader = query_stats_reader;
  if (old_reader)
  {
    DBUG_ASSERT(old_reader == 1);
    if (my_atomic_cas32(&query_stats_reader, &old_reader, 0))
      return 1;
  }
  return 0;
}

extern "C" uchar* get_query_stats_key(QUERY_STATS *qstats, size_t *length,
                                      my_bool not_used __attribute__((unused)))
{
  *length = qstats->query_stats_key_len;
  return (uchar*)qstats->query_stats_key;
}

extern "C" void free_query_stats(QUERY_STATS* qstats)
{
  qstats->magic = 0;
  my_free((char*)qstats->query_stats_key);
  my_free((char*)qstats);
}

extern "C" my_hash_value_type get_query_stats_hash(QUERY_STATS *qstats)
{
  return qstats->query_stats_hash;
}

QUERY_STATS* track_query_stats(const char *query, uint32 query_len)
{
#define MAX_QUERY_LENGTH QUERY_STATS_TYPE_STR_LEN
  QUERY_STATS *qstats = NULL;
  char qry_buf[MAX_QUERY_LENGTH + 1];
  char client_id[QUERY_ID_MAX_LENGTH + 1] = "\0";
  char *new_qry = &qry_buf[0];
  uint32 query_typ_len = 0;
  uint32 query_key_len = 0;
  uint32 client_id_len = 0;
  const uint32 max_key_len = query_len + 3*QUERY_ID_MAX_LENGTH + 1;
  bool fast_path = 1;
  bool new_query = 0;
  my_hash_value_type hashkey;

  if (max_key_len > MAX_QUERY_LENGTH)
  {
    new_qry = (char *) my_malloc(max_key_len, MYF(MY_WME));
    if (!new_qry) {
      sql_print_error("Failed to allocate query string buffer\n");
      return NULL;
    }
  }

  query_typ_len = sql_literal_replace(query, query_len, new_qry, client_id);
  DBUG_ASSERT(query_typ_len == strlen(new_qry));

  /* key consists of query string, and possibly client_id etc. */
  query_key_len = query_typ_len;
  if (opt_twitter_query_stats == TWEQS_BY_CLIENT_ID  && client_id[0] != '\0')
  {
    /* copy client_id to query_key for per-client query stats */
    client_id_len = strlen(client_id);
    new_qry[query_key_len++] = '@';
    memcpy(&new_qry[query_key_len], client_id, client_id_len);
    query_key_len += client_id_len;
  }
  new_qry[query_key_len] = '\0';

  hashkey = my_calc_hash(&query_stats_cache, (uchar*)new_qry, query_key_len);

retry:
  mysql_mutex_lock(&LOCK_query_stats_cache);

  qstats = (QUERY_STATS*)
    my_hash_search_using_hash_value_fast(&query_stats_cache, hashkey,
                                         (uchar*)new_qry, query_key_len,
                                         fast_path);
  if (!qstats)
  {
    /* ignore new query type, if exceeding capacity */
    if (query_stats_cache_count >= opt_twitter_query_stats_max)
    {
      mysql_mutex_unlock(&LOCK_query_stats_cache);
      if (max_key_len > MAX_QUERY_LENGTH)
        my_free(new_qry);
      return NULL;
    }

    char *query_typ = 0;
    if (!(query_typ = (char*)my_malloc(query_key_len+1, MYF(MY_WME))))
    {
      sql_print_error("Failed to allocate query_type\n");
      mysql_mutex_unlock(&LOCK_query_stats_cache);
      if (max_key_len > MAX_QUERY_LENGTH)
        my_free(new_qry);
      return NULL;
    }
    memcpy(query_typ, new_qry, query_key_len);
    query_typ[query_key_len] = '\0';
    qstats = (QUERY_STATS *) my_malloc(sizeof(QUERY_STATS), MYF(MY_WME));
    if (!qstats) {
      sql_print_error("Failed to allocate query_stats\n");
      mysql_mutex_unlock(&LOCK_query_stats_cache);
      my_free(query_typ);
      if (max_key_len > MAX_QUERY_LENGTH)
        my_free(new_qry);
      return NULL;
    }

    qstats->query_stats_key = query_typ;
    qstats->query_stats_key_len = query_key_len;
    qstats->query_type_len = query_typ_len;
    qstats->query_stats_hash = hashkey;
    qstats->client_id[0] = '\0';
    if (opt_twitter_query_stats == TWEQS_BY_CLIENT_ID && client_id[0] != '\0')
      strcpy(qstats->client_id, client_id);
#if 0 /* FIXME: per-shard or per-graph stats */
    qstats->shard_id[0] = '\0';
    qstats->graph_id[0] = '\0';
#endif
    qstats->magic = QUERY_STATS_MAGIC;
    clear_query_stats(qstats);

    if (my_hash_insert(&query_stats_cache, (uchar *)qstats))
    {
      sql_print_error("Failed to add to query_stats_cache\n");
      mysql_mutex_unlock(&LOCK_query_stats_cache);
      my_free(query_typ);
      my_free(qstats);
      if (max_key_len > MAX_QUERY_LENGTH)
        my_free(new_qry);
      return NULL;
    }

    new_query = 1;
    query_stats_cache_count++;
  }

  mysql_mutex_unlock(&LOCK_query_stats_cache);

  /* do key string compare outside the mutex on fast path, and retry lookup
     if key string mismatch */
  if (fast_path && !new_query && qstats)
  {
    DBUG_ASSERT(qstats->query_stats_key_len == query_key_len);
    if (my_strnncoll(query_stats_cache.charset,
                     (uchar*)qstats->query_stats_key,
                     qstats->query_stats_key_len,
                     (uchar*)new_qry, query_key_len))
    {
      fast_path = 0;
      goto retry;
    }
  }

  if (max_key_len > MAX_QUERY_LENGTH)
    my_free(new_qry);
  DBUG_ASSERT(qstats->magic == QUERY_STATS_MAGIC);
  return qstats;
}

void init_query_stats_cache()
{
  query_stats_reader = 0;
  query_stats_cache_count = 0;
  if (my_hash_init_extra2(&query_stats_cache, 512, system_charset_info,
                          max_connections, 0 /* key offset */, 0 /* key length */,
                          (my_hash_get_key)get_query_stats_key,
                          (my_hash_free_key)free_query_stats,
                          (my_hash_get_key_hash)get_query_stats_hash,
                          HASH_KEEP_HASH))
  {
    sql_print_error("Failed to initialize query_stats_cache.");
    abort();
  }
}

void free_query_stats_cache()
{
  my_hash_free(&query_stats_cache);
  query_stats_cache_count = 0;
}

/**
 * Query stats can only be enabled at a fixed level or disabled. However,
 * query stats structures are reset even if query stats is disabled. This
 * is necessary for reliability.
 */
void reset_query_stats_cache()
{
  /* FIXME: per-shard or per-graph stats */
  if (opt_twitter_query_stats <= TWEQS_BY_CLIENT_ID) {
    /* must acquire the lightweight reader latch */
    while (!query_stats_reader_lock())
      my_sleep(5000);
    mysql_mutex_lock(&LOCK_query_stats_cache);
    /* reset existing query stats, never delete them */
    for (uint i = 0; i < query_stats_cache.records; ++i)
    {
      QUERY_STATS *qry_stats = (QUERY_STATS*)my_hash_element(&query_stats_cache, i);
      clear_query_stats(qry_stats);
    }
    mysql_mutex_unlock(&LOCK_query_stats_cache);
    query_stats_reader_unlock();
  }
}

ST_FIELD_INFO query_stats_fields_info[]=
{
  {"QUERY_TYPE", 65535, MYSQL_TYPE_STRING, 0, 0, "Statement", SKIP_OPEN_TABLE},
  {"HASH_CODE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"CLIENT_ID", QUERY_ID_MAX_LENGTH, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COUNT", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"LATENCY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"MAX_LATENCY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROWS_SENT", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROWS_EXAMINED", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};

int fill_query_stats(THD *thd, TABLE_LIST *tables, Item *cond)
{
  uint i;
  TABLE* table= tables->table;

  DBUG_ENTER("fill_query_stats");

  /* acquire lightweight reader latch instead of query stats cache mutex,
     so that reads will not interfere with heavy write paths */
  if (!query_stats_reader_lock())
  {
    my_message(ER_LOCK_ABORTED, ER_QUERY_STATS_PULL_ABORTED_MSG, MYF(0));
    DBUG_RETURN(-1);
  }

  for (i = 0; i < query_stats_cache.records; ++i)
  {
    int fidx = 0;
    QUERY_STATS *qry_stats = (QUERY_STATS*) my_hash_element(&query_stats_cache, i);

    if (!qry_stats)
      continue;

    /* set default values for this row */
    restore_record(table, s->default_values);

    table->field[fidx++]->store(qry_stats->query_stats_key,
                                qry_stats->query_type_len,
                                system_charset_info);
    table->field[fidx++]->store(qry_stats->query_stats_hash, TRUE);
    if (qry_stats->client_id[0] != '\0')
      table->field[fidx++]->store(&qry_stats->client_id[0],
                                  strlen(&qry_stats->client_id[0]),
                                  system_charset_info);
    else
      table->field[fidx++]->store("", 0, system_charset_info);
    table->field[fidx++]->store(qry_stats->count, TRUE);
    table->field[fidx++]->store(qry_stats->latency, TRUE);
    table->field[fidx++]->store(qry_stats->max_latency, TRUE);
    table->field[fidx++]->store(qry_stats->rows_sent, TRUE);
    table->field[fidx++]->store(qry_stats->rows_examined, TRUE);

    if (schema_table_store_record(thd, table))
    {
      query_stats_reader_unlock();
      DBUG_RETURN(-1);
    }
  }

  query_stats_reader_unlock();
  DBUG_RETURN(0);
}

