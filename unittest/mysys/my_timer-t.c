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

#include <string.h>

#include "my_timer.h"
#include "thr_template.c"

typedef struct
{
  my_timer_t timer;
  unsigned int fired;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} test_timer_t;

static void timer_notify_function(my_timer_t *timer)
{
  test_timer_t *test= my_container_of(timer, test_timer_t, timer);

  pthread_mutex_lock(&test->mutex);
  test->fired++;
  pthread_cond_signal(&test->cond);
  pthread_mutex_unlock(&test->mutex);
}

static void test_timer_create(test_timer_t *test)
{
  memset(test, 0, sizeof(test_timer_t));
  pthread_mutex_init(&test->mutex, NULL);
  pthread_cond_init(&test->cond, NULL);
  ok(my_timer_create(&test->timer) == 0, "my_timer_create");
  test->timer.notify_function= timer_notify_function;
}

static void test_timer_destroy(test_timer_t *test)
{
  pthread_mutex_destroy(&test->mutex);
  pthread_cond_destroy(&test->cond);
  my_timer_delete(&test->timer);
}

static void test_create_and_delete(void)
{
  int rc;
  my_timer_t timer;

  diag("test_create_and_delete");

  memset(&timer, 0, sizeof(timer));

  rc= my_timer_create(&timer);
  ok(rc == 0, "my_timer_create");

  my_timer_delete(&timer);
}

static void test_reset(void)
{
  int rc, state;
  test_timer_t test;

  diag("test_reset");

  test_timer_create(&test);

  rc= my_timer_set(&test.timer, 3600000U);
  ok(rc == 0, "my_timer_set");

  rc= my_timer_reset(&test.timer, &state);
  ok(rc == 0, "my_timer_reset");

  ok(state == 1, "timer state is nonsignaled");
  ok(test.fired == 0, "timer has not fired");

  test_timer_destroy(&test);
}

static void test_timer(void)
{
  int rc, state;
  test_timer_t test;

  diag("test_timer");

  test_timer_create(&test);

  pthread_mutex_lock(&test.mutex);

  rc= my_timer_set(&test.timer, 5);
  ok(rc == 0, "my_timer_set");

  ok(test.fired == 0, "not fired yet");

  while (!test.fired)
    pthread_cond_wait(&test.cond, &test.mutex);

  ok(test.fired == 1, "timer fired once");

  rc= my_timer_reset(&test.timer, &state);
  ok(rc == 0, "my_timer_reset");

  ok(state == 0, "timer state was signaled");

  pthread_mutex_unlock(&test.mutex);

  test_timer_destroy(&test);
}

static void timer_set_and_wait(test_timer_t *test, unsigned int fired_count)
{
  int rc, state;

  rc= my_timer_set(&test->timer, 5);
  ok(rc == 0, "my_timer_set");

  ok(test->fired != fired_count, "not fired yet");

  while (test->fired != fired_count)
    pthread_cond_wait(&test->cond, &test->mutex);

  ok(test->fired == fired_count, "timer fired");

  rc= my_timer_reset(&test->timer, &state);
  ok(rc == 0, "my_timer_reset");

  ok(state == 0, "timer state was signaled");
}

static void test_timer_reuse(void)
{
  test_timer_t test;

  diag("test_timer_reuse");

  test_timer_create(&test);

  pthread_mutex_lock(&test.mutex);

  timer_set_and_wait(&test, 1);
  timer_set_and_wait(&test, 2);
  timer_set_and_wait(&test, 3);

  pthread_mutex_unlock(&test.mutex);

  test_timer_destroy(&test);
}

static void test_independent_timers(void)
{
  int rc, state;
  test_timer_t test;

  diag("test_independent_timers");

  test_timer_create(&test);

  rc= my_timer_set(&test.timer, 3600000U);
  ok(rc == 0, "my_timer_set");

  test_timer();

  rc= my_timer_reset(&test.timer, &state);
  ok(rc == 0, "my_timer_reset");

  ok(state == 1, "timer state is nonsignaled");
  ok(test.fired == 0, "timer has not fired");

  test_timer_destroy(&test);
}

static void test_timer_no_tap(void)
{
  int rc, state;
  test_timer_t test;

  memset(&test, 0, sizeof(test_timer_t));

  pthread_mutex_init(&test.mutex, NULL);
  pthread_cond_init(&test.cond, NULL);

  test.timer.notify_function= timer_notify_function;

  rc= my_timer_create(&test.timer);
  assert(rc == 0);

  pthread_mutex_lock(&test.mutex);

  rc= my_timer_set(&test.timer, 5);
  assert(rc == 0);

  assert(test.fired == 0); /* not fired yet */

  while (!test.fired)
    pthread_cond_wait(&test.cond, &test.mutex);

  assert(test.fired == 1); /* timer fired once */

  rc= my_timer_reset(&test.timer, &state);
  assert(rc == 0);

  assert(state == 0); /* timer state was signaled */

  pthread_mutex_unlock(&test.mutex);

  pthread_mutex_destroy(&test.mutex);
  pthread_cond_destroy(&test.cond);
  my_timer_delete(&test.timer);
}

static pthread_handler_t test_timer_per_thread(void *arg)
{
  int iter= *(int *) arg;

  while (iter--)
    test_timer_no_tap();

  pthread_mutex_lock(&mutex);
  if (!--running_threads)
    pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  return NULL;
}

static void test_reinitialization(void)
{
  diag("test_reinitialization");

  my_timer_deinit();
  ok(my_timer_init_ext() == 0, "my_timer_init_ext");
  test_timer();
  my_timer_deinit();
  ok(my_timer_init_ext() == 0, "my_timer_init_ext");
}

void do_tests()
{
  plan(49);

  ok(my_timer_init_ext() == 0, "my_timer_init_ext");

  test_create_and_delete();
  test_reset();
  test_timer();
  test_timer_reuse();
  test_independent_timers();
  test_concurrently("per-thread", test_timer_per_thread, THREADS, 5);
  test_reinitialization();

  my_timer_deinit();
}
