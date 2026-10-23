# Cross-compile the armhf device binary on Windows, no WSL or Docker.
#
# Used by pc/tools/build_windows.py, which downloads everything this file
# points at into pc/build-toolchain/:
#   gcc/      Arm GNU Toolchain 12.2.rel1 (Windows host, arm-none-linux-gnueabihf).
#             GCC 12.2 + glibc 2.36, the same pair Debian bookworm ships, so the
#             binary matches what CI builds in the debian:bookworm container.
#   sysroot/  SDL2 and GLES headers/libs unpacked from Debian bookworm armhf .debs.
#   cmake/    SDL2Config.cmake pointing at the sysroot.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

get_filename_component(OC_DEPS "${CMAKE_CURRENT_LIST_DIR}/../build-toolchain" ABSOLUTE)
set(OC_SYSROOT "${OC_DEPS}/sysroot")

set(CMAKE_C_COMPILER   "${OC_DEPS}/gcc/bin/arm-none-linux-gnueabihf-gcc.exe")
set(CMAKE_CXX_COMPILER "${OC_DEPS}/gcc/bin/arm-none-linux-gnueabihf-g++.exe")

# Only SDL2/GLES/EGL/KHR headers live in the sysroot; libc comes from the
# toolchain itself, so these can't shadow anything.
set(_oc_includes
    "${OC_SYSROOT}/usr/include"
    "${OC_SYSROOT}/usr/include/arm-linux-gnueabihf"
    "${OC_SYSROOT}/usr/include/SDL2")
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES   ${_oc_includes})
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${_oc_includes})

# libSDL2 pulls in X11, Wayland, PulseAudio and friends. They exist on the
# device, not here, so let the linker leave libSDL2's own imports unresolved.
# (SDL2 and GLESv2 are linked by full path, so no -L is needed.)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--allow-shlib-undefined")

# Re-root the /usr/lib/arm-linux-gnueabihf searches in pc/CMakeLists.txt
# into the sysroot. SDL2_DIR is read directly, so packages can search both.
set(CMAKE_FIND_ROOT_PATH "${OC_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(SDL2_DIR "${OC_DEPS}/cmake" CACHE PATH "SDL2 config from the Windows armhf sysroot")
