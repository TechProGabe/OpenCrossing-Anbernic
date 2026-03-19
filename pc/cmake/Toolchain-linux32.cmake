# CMake toolchain file for Linux i686 (32-bit x86)
#
# Requires one of:
#   sudo apt install gcc-multilib g++-multilib libsdl2-dev:i386   (Debian/Ubuntu)
#   sudo dnf install gcc.i686 glibc-devel.i686 SDL2-devel.i686   (Fedora)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Prefer the explicit i686 cross-compiler if installed (gcc-i686-linux-gnu package).
# Fall back to the host gcc with -m32 (gcc-multilib package).
find_program(_I686_GCC NAMES i686-linux-gnu-gcc NAMES_PER_DIR)
if(_I686_GCC)
    set(CMAKE_C_COMPILER   i686-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER i686-linux-gnu-g++)
else()
    # Detect host compiler name (may be 'gcc', 'gcc-13', etc.)
    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER   gcc)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER g++)
    endif()
    # Force 32-bit output
    set(CMAKE_C_FLAGS_INIT   "-m32")
    set(CMAKE_CXX_FLAGS_INIT "-m32")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
endif()
unset(_I686_GCC CACHE)

# Tell CMake's find_library / find_package to search 32-bit multiarch paths first.
# This prevents accidentally linking 64-bit libraries when cross-compiling on x86_64.
set(CMAKE_FIND_ROOT_PATH
    /usr/lib/i386-linux-gnu
    /usr/lib32
    /usr
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # build tools from host
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # libs from target (32-bit)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # headers from target
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)    # cmake package configs from either
