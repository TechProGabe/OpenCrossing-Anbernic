#!/bin/bash
# build_pc.sh - Build the Animal Crossing PC port
#
# Windows (MSYS2 MINGW32):
#   Prerequisites: pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-SDL2
#   Run from MSYS2 MINGW32 shell
#
# Linux x86 (32-bit):
#   Prerequisites: sudo apt install gcc-multilib g++-multilib cmake libsdl2-dev:i386
#                  sudo dnf install gcc.i686 glibc-devel.i686 SDL2-devel.i686
#
# Linux ARMhf (32-bit ARM):
#   Prerequisites: sudo dpkg --add-architecture armhf && sudo apt update
#                  sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
#                                   cmake libsdl2-dev:armhf
#   Pass --armhf flag: ./build_pc.sh --armhf
#
# Usage:
#   1. ./build_pc.sh [--armhf]
#   2. Place your disc image (.ciso/.iso/.gcm) in pc/build32/bin/rom/
#   3. Run: pc/build32/bin/AnimalCrossing[.exe]

set -e

# --- Parse arguments ---
ARCH="x86"
for arg in "$@"; do
    case "$arg" in
        --armhf) ARCH="armhf" ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# --- Detect platform ---
if [ "$MSYSTEM" = "MINGW32" ]; then
    PLATFORM="mingw32"
elif [ "$(uname -s)" = "Linux" ]; then
    PLATFORM="linux"
else
    echo "Error: Unsupported platform."
    echo "  Windows: Run from MSYS2 MINGW32 shell"
    echo "  Linux:   Run normally (bash build_pc.sh)"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/pc/build32"
BIN_DIR="$BUILD_DIR/bin"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$PLATFORM" = "mingw32" ]; then
    # --- Windows / MSYS2 MINGW32 ---
    if [ ! -f Makefile ]; then
        echo "=== Configuring CMake (Windows MinGW32) ==="
        cmake .. -G "MinGW Makefiles"
    fi
    echo "=== Building ==="
    mingw32-make -j$(nproc)

elif [ "$ARCH" = "armhf" ]; then
    # --- Linux ARMhf (cross-compile from x86_64) ---

    # Help pkg-config find armhf libraries on multiarch hosts.
    for dir in \
        /usr/lib/arm-linux-gnueabihf/pkgconfig \
        /usr/lib/pkgconfig; do
        if [ -d "$dir" ]; then
            export PKG_CONFIG_PATH="$dir:${PKG_CONFIG_PATH:-}"
        fi
    done

    if command -v arm-linux-gnueabihf-pkg-config &>/dev/null; then
        export PKG_CONFIG=arm-linux-gnueabihf-pkg-config
    fi

    if [ ! -f Makefile ]; then
        echo "=== Configuring CMake (Linux ARMhf 32-bit) ==="
        cmake .. -G "Unix Makefiles" \
            -DCMAKE_TOOLCHAIN_FILE="../cmake/Toolchain-arm-linux-gnueabihf.cmake"
    fi
    echo "=== Building ==="
    make -j$(nproc)

else
    # --- Linux x86 32-bit ---

    # Help pkg-config find 32-bit libraries on multiarch x86_64 hosts.
    # libsdl2-dev:i386 installs to /usr/lib/i386-linux-gnu/pkgconfig on Debian/Ubuntu.
    # SDL2-devel.i686  installs to /usr/lib/pkgconfig or /usr/lib32/pkgconfig on Fedora.
    for dir in \
        /usr/lib/i386-linux-gnu/pkgconfig \
        /usr/lib32/pkgconfig \
        /usr/lib/pkgconfig; do
        if [ -d "$dir" ]; then
            export PKG_CONFIG_PATH="$dir:${PKG_CONFIG_PATH:-}"
        fi
    done

    # Use i686-linux-gnu-pkg-config if available (avoids mixing 64-bit flags on 64-bit host)
    if command -v i686-linux-gnu-pkg-config &>/dev/null; then
        export PKG_CONFIG=i686-linux-gnu-pkg-config
    fi

    if [ ! -f Makefile ]; then
        echo "=== Configuring CMake (Linux x86 32-bit) ==="
        cmake .. -G "Unix Makefiles" \
            -DCMAKE_TOOLCHAIN_FILE="../cmake/Toolchain-linux32.cmake"
    fi
    echo "=== Building ==="
    make -j$(nproc)
fi

# --- Create runtime directories ---
mkdir -p "$BIN_DIR/rom"
mkdir -p "$BIN_DIR/texture_pack"
mkdir -p "$BIN_DIR/save"

echo ""
echo "=== Build complete! ==="
echo ""
echo "Place your Animal Crossing disc image (.ciso/.iso/.gcm) in:"
echo "  pc/build32/bin/rom/"
echo ""
if [ "$PLATFORM" = "mingw32" ]; then
    echo "Run: pc/build32/bin/AnimalCrossing.exe"
else
    echo "Run: ./pc/build32/bin/AnimalCrossing"
fi
