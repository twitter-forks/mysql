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

SET(COMPILATION_COMMENT "Twitter MySQL")
SET(COMPILATION_COMMENT_VERSION_SOURCE TRUE)

#
# Feature set.
#

SET(FEATURE_SET "community" CACHE STRING "Twitter MySQL feature set.")

IF(FEATURE_SET)
  STRING(TOLOWER ${FEATURE_SET} FEATURE_SET)
  IF(FEATURE_SET STREQUAL "community")
    # Disable embedded MySQL server library
    SET(WITH_EMBEDDED_SERVER FALSE CACHE BOOL "Disable embedded library")
  ELSE()
    MESSAGE(FATAL_ERROR "Unknown feature set.")
  ENDIF()
ENDIF()

#
# Compiler options.
#

# Default GCC flags
IF(CMAKE_COMPILER_IS_GNUCC)
  SET(COMMON_C_FLAGS               "-g -static-libgcc -fno-omit-frame-pointer -fno-strict-aliasing")
  SET(CMAKE_C_FLAGS_DEBUG          "-O ${COMMON_C_FLAGS}")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_C_FLAGS}")
ENDIF()
IF(CMAKE_COMPILER_IS_GNUCXX)
  SET(COMMON_CXX_FLAGS               "-g -static-libgcc -fno-omit-frame-pointer -fno-strict-aliasing")
  SET(CMAKE_CXX_FLAGS_DEBUG          "-O ${COMMON_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_CXX_FLAGS}")
ENDIF()

# Default Clang flags
IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(COMMON_C_FLAGS               "-g -fno-omit-frame-pointer -fno-strict-aliasing")
  SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_C_FLAGS}")
ENDIF()
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(COMMON_CXX_FLAGS               "-g -fno-omit-frame-pointer -fno-strict-aliasing")
  SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_CXX_FLAGS}")
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
SET(WITH_SSL system CACHE STRING "Use system openssl")

#
# Linux-native asynchronous I/O access.
#

IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  CHECK_INCLUDE_FILES(libaio.h HAVE_LIBAIO_H)
  CHECK_LIBRARY_EXISTS(aio io_queue_init "" HAVE_LIBAIO)

  IF(NOT HAVE_LIBAIO_H OR NOT HAVE_LIBAIO)
    MESSAGE(FATAL_ERROR "Build requires development files for the "
      "Linux-native asynchronous I/O facility (libaio-devel).")
  ENDIF()
ENDIF()

