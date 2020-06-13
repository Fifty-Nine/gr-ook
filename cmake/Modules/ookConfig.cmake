INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_OOK ook)

FIND_PATH(
    OOK_INCLUDE_DIRS
    NAMES ook/api.h
    HINTS $ENV{OOK_DIR}/include
        ${PC_OOK_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    OOK_LIBRARIES
    NAMES gnuradio-ook
    HINTS $ENV{OOK_DIR}/lib
        ${PC_OOK_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/ookTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OOK DEFAULT_MSG OOK_LIBRARIES OOK_INCLUDE_DIRS)
MARK_AS_ADVANCED(OOK_LIBRARIES OOK_INCLUDE_DIRS)
