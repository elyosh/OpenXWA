set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Debian's MinGW-w64 tools for UCRT, the C runtime of Windows 10 and later.
set(CMAKE_C_COMPILER x86_64-w64-mingw32ucrt-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32ucrt-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32ucrt-windres)

set(XWA_MINGW_PREFIX "/opt/xwa-windows" CACHE PATH
    "Prefix containing Windows dependencies built with MinGW-w64")
set(CMAKE_FIND_ROOT_PATH "${XWA_MINGW_PREFIX}")
set(CMAKE_PREFIX_PATH "${XWA_MINGW_PREFIX}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR}
    "${XWA_MINGW_PREFIX}/lib/pkgconfig:${XWA_MINGW_PREFIX}/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
