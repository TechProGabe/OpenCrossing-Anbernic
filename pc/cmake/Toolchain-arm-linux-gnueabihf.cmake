# CMake toolchain file for Linux ARMhf (32-bit ARM hard-float)
#
# Requires:
#   sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
#                    libsdl2-dev:armhf
#
# On Debian/Ubuntu multiarch you may first need:
#   sudo dpkg --add-architecture armhf
#   sudo apt update

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Tell CMake's find_library / find_package to search ARMhf multiarch paths.
set(CMAKE_FIND_ROOT_PATH
    /usr/lib/arm-linux-gnueabihf
    /usr/arm-linux-gnueabihf
    /usr
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # build tools from host
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # libs from target (armhf)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # headers from target
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)    # cmake package configs from either
