# MinGW-w64 i686 (32-bit) toolchain for GWY launcher / vmrp PE32 builds.
# Usage:
#   cmake -S . -B build-i686 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-i686.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

if(NOT DEFINED ENV{MSYS2_MINGW32})
  set(_MINGW32_ROOT "C:/msys64/mingw32")
else()
  file(TO_CMAKE_PATH "$ENV{MSYS2_MINGW32}" _MINGW32_ROOT)
endif()

set(CMAKE_C_COMPILER   "${_MINGW32_ROOT}/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${_MINGW32_ROOT}/bin/g++.exe")
set(CMAKE_RC_COMPILER  "${_MINGW32_ROOT}/bin/windres.exe")
set(CMAKE_AR           "${_MINGW32_ROOT}/bin/ar.exe")
set(CMAKE_RANLIB       "${_MINGW32_ROOT}/bin/ranlib.exe")
set(CMAKE_OBJDUMP      "${_MINGW32_ROOT}/bin/objdump.exe")
set(CMAKE_NM           "${_MINGW32_ROOT}/bin/nm.exe")
set(CMAKE_STRIP        "${_MINGW32_ROOT}/bin/strip.exe")

set(CMAKE_FIND_ROOT_PATH "${_MINGW32_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-m32")
set(CMAKE_CXX_FLAGS_INIT "-m32")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
