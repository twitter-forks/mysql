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

#include "my_config.h"
#include "client/linux/handler/exception_handler.h"
#include "my_stacktrace.h"

/**
  Callback invoked after the minidump has been written.
*/

static bool
dump_callback(const char *dump_path,
              const char *minidump_id,
              void *,
              bool succeeded)
{
  my_safe_printf_stderr("Minidump: %s/%s.dmp\n\n", dump_path, minidump_id);
  return succeeded;
}


/**
  Detect if Valgrind is being used from test cases.

  @remark Valgrind does not support clone() as used by breakpad.

  @return true if running on Valgrind, false if not.
*/

static bool
is_valgrind_test(void)
{
  char *env= getenv("VALGRIND_TEST");

  if (env == NULL || *env != '1')
    return false;

  my_safe_printf_stderr("Minidump skipped (VALGRIND_TEST=1).\n\n");

  return true;
}


/**
  Write minidump information to a file in the specified directory.

  @param  dump_path   Where the minidump file is created.
*/

void my_write_minidump(const char *dump_path)
{
  using google_breakpad::ExceptionHandler;

  my_safe_printf_stderr("Attempting to generate minidump information.\n");

  bool status= is_valgrind_test() ? true :
               ExceptionHandler::WriteMinidump(dump_path, dump_callback, NULL);

  if (! status)
    my_safe_printf_stderr("Minidump failed.\n\n");
}

