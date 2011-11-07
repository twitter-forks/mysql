# Copyright (c) 2011 Twitter, Inc.
#
# Twitter MySQL build configuration. Used to determine which components
# should be built and how the build will be optimized.
#

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)

#
# Overwrite server compilation comment string.
#

SET(COMPILATION_COMMENT "Twitter MySQL Server - ${CMAKE_BUILD_TYPE}")

#
# Feature set.
#

IF(FEATURE_SET)
  STRING(TOLOWER ${FEATURE_SET} FEATURE_SET)
  IF(FEATURE_SET STREQUAL "community")
    SET(WITH_EMBEDDED_SERVER TRUE CACHE BOOL "Embedded MySQL Server Library")
  ELSE()
    MESSAGE(FATAL_ERROR "Unknown feature set.")
  ENDIF()
ENDIF()

#
# Compiler options.
#

IF(CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX)
  SET(COMMON_FLAGS "-g -fno-omit-frame-pointer -fno-strict-aliasing")
  SET(CMAKE_C_FLAGS_DEBUG "-O ${COMMON_FLAGS}")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_FLAGS}")
  SET(CMAKE_CXX_FLAGS_DEBUG "-O ${COMMON_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_FLAGS}")
ELSE()
  MESSAGE(WARNING "There are no specific compiler options.")
ENDIF()

#
# Include and/or exclude storage engines.
#

SET(WITH_ARCHIVE_STORAGE_ENGINE TRUE BOOL)
SET(WITH_BLACKHOLE_STORAGE_ENGINE TRUE BOOL)
SET(WITHOUT_PERFSCHEMA_STORAGE_ENGINE TRUE BOOL)

#
# Use system libraries instead of bundled ones.
#

SET(WITH_ZLIB system CACHE STRING "Use system zlib")

SET(WITH_LIBEDIT OFF CACHE BOOL "Disable bundled libedit")
SET(WITH_READLINE OFF CACHE BOOL "Disable bundled readline")

#
# Linux-native asynchronous I/O access.
#

CHECK_INCLUDE_FILES(libaio.h HAVE_LIBAIO_H)
CHECK_LIBRARY_EXISTS(aio io_queue_init "" HAVE_LIBAIO)

IF(NOT HAVE_LIBAIO_H OR NOT HAVE_LIBAIO)
  MESSAGE(FATAL_ERROR "Build requires development files for the "
    "Linux-native asynchronous I/O facility (libaio-devel).")
ENDIF()

