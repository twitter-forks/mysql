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

#ifndef MINIDUMP_INCLUDED
#define MINIDUMP_INCLUDED

#if defined(HAVE_BREAKPAD) && !defined(EMBEDDED_LIBRARY)
void my_write_minidump(const char *);
#else
static void my_write_minidump(const char *) {}
#endif

#endif /* MINIDUMP_INCLUDED */
