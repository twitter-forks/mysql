/* Copyright (c) 2012, Twitter, Inc. All rights reserved.

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

#include "sql_class.h"      /* THD */
#include "sql_timer.h"      /* thd_timer_set, etc. */
#include "my_timer.h"       /* my_timer_t */

struct st_thd_timer
{
  THD *thd;
  my_timer_t timer;
  pthread_mutex_t mutex;
  bool destroy;
};

C_MODE_START
static void timer_callback(my_timer_t *);
C_MODE_END

/**
  Allocate and initialize a thread timer object.

  @return NULL on failure.
*/

static thd_timer_t *
thd_timer_create(void)
{
  thd_timer_t *ttp;
  DBUG_ENTER("thd_timer_create");

  ttp= (thd_timer_t *) my_malloc(sizeof(*ttp), MYF(MY_WME | MY_ZEROFILL));

  if (ttp == NULL)
    DBUG_RETURN(NULL);

  ttp->timer.notify_function= timer_callback;
  pthread_mutex_init(&ttp->mutex, MY_MUTEX_INIT_FAST);

  if (! my_timer_create(&ttp->timer))
    DBUG_RETURN(ttp);

  pthread_mutex_destroy(&ttp->mutex);
  my_free(ttp);

  DBUG_RETURN(NULL);
}


/**
  Release resources allocated for a thread timer.

  @param  ttp   Thread timer object.
*/

static void
thd_timer_destroy(thd_timer_t *ttp)
{
  DBUG_ENTER("thd_timer_destroy");

  my_timer_delete(&ttp->timer);
  pthread_mutex_destroy(&ttp->mutex);
  my_free(ttp);

  DBUG_VOID_RETURN;
}


/**
  Notify a thread (session) that its timer has expired.

  @param  ttp   Thread timer object.

  @return true if the object should be destroyed.
*/

static bool
timer_notify(thd_timer_t *ttp)
{
  THD *thd= ttp->thd;

  DBUG_ASSERT(!ttp->destroy || !thd);

  /*
    Statement might have finished while the timer notification
    was being delivered. If this is the case, the timer object
    was detached (orphaned) and has no associated session (thd).
  */
  if (thd)
  {
    mysql_mutex_lock(&thd->LOCK_thd_data);
    thd->awake(THD::KILL_TIMEOUT);
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }

  /* Mark the object as unreachable. */
  ttp->thd= NULL;

  return ttp->destroy;
}


/**
  Timer expiration notification callback.

  @param  timer   Timer (mysys) object.

  @note Invoked in a separate thread of control.
*/

static void
timer_callback(my_timer_t *timer)
{
  bool destroy;
  thd_timer_t *ttp;

  ttp= my_container_of(timer, thd_timer_t, timer);

  pthread_mutex_lock(&ttp->mutex);
  destroy= timer_notify(ttp);
  pthread_mutex_unlock(&ttp->mutex);

  if (destroy)
    thd_timer_destroy(ttp);
}


/**
  Set the time until the currently running statement is aborted.

  @param  thd   Thread (session) context.
  @param  ttp   Thread timer object.
  @param  time  Length of time, in milliseconds, until the currently
                running statement is aborted.

  @return NULL on failure.
*/

thd_timer_t *
thd_timer_set(THD *thd, thd_timer_t *ttp, unsigned long time)
{
  DBUG_ENTER("thd_timer_set");

  /* Create a new thread timer object if one was not provided. */
  if (ttp == NULL && (ttp= thd_timer_create()) == NULL)
    DBUG_RETURN(NULL);

  DBUG_ASSERT(!ttp->destroy && !ttp->thd);

  /* Mark the notification as pending. */
  ttp->thd= thd;

  /* Arm the timer. */
  if (! my_timer_set(&ttp->timer, time))
    DBUG_RETURN(ttp);

  /* Dispose of the (cached) timer object. */
  thd_timer_destroy(ttp);

  DBUG_RETURN(NULL);
}


/**
  Reap a (possibly) pending timer object.

  @param  ttp   Thread timer object.

  @return true if the timer object is unreachable.
*/

static bool
reap_timer(thd_timer_t *ttp, bool pending)
{
  bool unreachable;

  /* Cannot be tagged for destruction. */
  DBUG_ASSERT(!ttp->destroy);

  /* If not pending, timer hasn't fired. */
  DBUG_ASSERT(pending || ttp->thd);

  /*
    The timer object can be reused if the timer was stopped before
    expiring. Otherwise, the timer notification function might be
    executing asynchronously in the context of a separate thread.
  */
  unreachable= pending ? ttp->thd == NULL : true;

  ttp->thd= NULL;

  return unreachable;
}

/**
  Deactivate the given timer.

  @param  ttp   Thread timer object.

  @return NULL if the timer object was orphaned.
          Otherwise, the given timer object is returned.
*/

thd_timer_t *
thd_timer_reset(thd_timer_t *ttp)
{
  bool unreachable;
  int status, state;
  DBUG_ENTER("thd_timer_reset");

  status= my_timer_reset(&ttp->timer, &state);

  /*
    If the notification function cannot possibly run anymore, cache
    the timer object as there are no outstanding references to it.
  */
  pthread_mutex_lock(&ttp->mutex);
  unreachable= reap_timer(ttp, status ? true : !state);
  ttp->destroy= unreachable ? false : true;
  pthread_mutex_unlock(&ttp->mutex);

  DBUG_RETURN(unreachable ? ttp : NULL);
}


/**
  Release resources allocated for a given thread timer.

  @param  ttp   Thread timer object.
*/

void
thd_timer_end(thd_timer_t *ttp)
{
  DBUG_ENTER("thd_timer_end");

  thd_timer_destroy(ttp);

  DBUG_VOID_RETURN;
}

