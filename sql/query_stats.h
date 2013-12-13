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

#ifndef QUERY_STATS_INCLUDED
#define QUERY_STATS_INCLUDED

extern mysql_mutex_t LOCK_query_stats_cache;

/**
 * Twitter query stats level.
 *
 * The higher the level, the finer granuarity the stats, and the higher
 * overhead. Currently, only first 2 levels are supported.
 */
enum enum_query_stats_level
{
  TWEQS_BY_QUERY_TYPE = 1,
  TWEQS_BY_CLIENT_ID,
  TWEQS_BY_SHARD_ID,
  TWEQS_BY_GRAPH_ID,
  TWEQS_BY_CLIENT_SHARD,
  TWEQS_BY_CLIENT_GRAPH,
  TWEQS_BY_SHARD_GRAPH,
  TWEQS_BY_CLIENT_SHARD_GRAPH,
  TWEQS_MAX
};

/* Query type string length limit */
#define QUERY_STATS_TYPE_STR_LEN 4096
#define QUERY_STATS_TYPE_STR_MAX (1024*256)

/* Query type stats hash table capacity limit */
#define QUERY_STATS_CACHE_SIZE 10240
#define QUERY_STATS_CACHE_MAX  1000000

#define QUERY_ID_MAX_LENGTH  48
#define QUERY_STATS_MAGIC    0xBEEFBEEF

typedef struct query_stats_st
{
  char *query_stats_key;                 /* canonicalized query type key */
  uint  query_stats_key_len;             /* hash key length */
  uint  query_type_len;                  /* canonicalized query type length */

  my_hash_value_type query_stats_hash;   /* query type key hash value */

  char client_id[QUERY_ID_MAX_LENGTH+1];
#if 0 /* FIXME: per-shard or per-graph stats; ID length */
  char shard_id[QUERY_ID_MAX_LENGTH];
  char graph_id[QUERY_ID_MAX_LENGTH];
#endif

  ulonglong count;                      /* number of executions */
  ulonglong latency;                    /* accumulated latency */
  ulonglong max_latency;                /* max query latency */

  ulonglong rows_sent;                  /* number of rows sent */
  ulonglong rows_examined;              /* number of rows examined */

  uint magic;
} QUERY_STATS;

void init_query_stats_cache();
void free_query_stats_cache();
void reset_query_stats_cache();

/**
 * Fetch QUERY_STATS structure for a given qualified query. Query stats
 * is tracked by query type, i.e. canonicalized query text that is
 * stripped of constant values.
 */
QUERY_STATS* track_query_stats(const char *query, uint32 query_length);

#define TRACK_QUERY(sql) \
  (sql == SQLCOM_SELECT || sql == SQLCOM_UPDATE || sql == SQLCOM_INSERT || \
   sql == SQLCOM_DELETE || sql == SQLCOM_INSERT_SELECT)

#define clear_query_stats(qstats) \
  {                               \
    qstats->count    = 0;         \
    qstats->latency  = 0;         \
    qstats->max_latency = 0;      \
    qstats->rows_sent = 0;        \
    qstats->rows_examined = 0;    \
  }

/* Information schema query_statistics */
extern ST_FIELD_INFO query_stats_fields_info[];

int fill_query_stats(THD *thd, TABLE_LIST *tables, Item *cond);

#endif /* QUERY_STATS_INCLUDED */
