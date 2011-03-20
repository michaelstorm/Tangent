# - Try to find LibEvent
# Once done this will define
#  LIBEVENT_FOUND - System has LibEvent
#  LIBEVENT_INCLUDE_DIRS - The LibEvent include directories
#  LIBEVENT_LIBRARIES - The libraries needed to use LibEvent
#  LIBEVENT_DEFINITIONS - Compiler switches required for using LibEvent

find_package(PkgConfig)
pkg_check_modules(PC_LIBEVENT libevent)
set(LIBEVENT_DEFINITIONS ${PC_LIBEVENT_CFLAGS_OTHER})

find_path(LIBEVENT_INCLUDE_DIR event2/event.h
          HINTS ${PC_LIBEVENT_INCLUDEDIR} ${PC_LIBEVENT_INCLUDE_DIRS})

find_library(LIBEVENT_LIBRARY NAMES event libevent
             HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS} )

set(LIBEVENT_LIBRARIES ${LIBEVENT_LIBRARY} )
set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEvent DEFAULT_MSG
                                  LIBEVENT_LIBRARY LIBEVENT_INCLUDE_DIR)

mark_as_advanced(LIBEVENT_INCLUDE_DIR LIBEVENT_LIBRARY )

