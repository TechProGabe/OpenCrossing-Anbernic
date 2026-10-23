#!/usr/bin/env python3
"""Build the armhf device binary on Windows. No WSL, Docker or MSYS2 needed.

    python pc/tools/build_windows.py            # fetch tools (first run), configure, build
    python pc/tools/build_windows.py --jobs 8
    python pc/tools/build_windows.py --clean    # wipe the build dir first
    python pc/tools/build_windows.py --fetch-only

Needs Python 3.8+, CMake and Ninja on PATH (`pip install cmake ninja` works).
On first run it downloads about 260 MB into pc/build-toolchain/ (gitignored):
  - Arm GNU Toolchain 12.2.rel1, Windows host, arm-none-linux-gnueabihf target
  - SDL2 and GLES packages from Debian bookworm armhf
Every download is checked against a SHA-256 before use.

Output: pc/build-armhf-win/bin/AnimalCrossing, the same binary CI builds with
pc/build-armhf-docker.sh (same compiler version, glibc and tuning flags).
"""

import argparse
import hashlib
import io
import lzma
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PC = REPO / "pc"
DEPS = PC / "build-toolchain"
BUILD = PC / "build-armhf-win"
TOOLCHAIN_FILE = PC / "cmake" / "Toolchain-armhf-windows.cmake"

GCC_VERSION = "12.2.rel1"
GCC_ZIP = f"arm-gnu-toolchain-{GCC_VERSION}-mingw-w64-i686-arm-none-linux-gnueabihf.zip"
GCC_URL = f"https://developer.arm.com/-/media/Files/downloads/gnu/{GCC_VERSION}/binrel/{GCC_ZIP}"
GCC_SHA256 = "5c023fab51fb7fa59938ed4c8c32f40bdb1b8394d7dfbae986c90ab506a11e3a"

DEBIAN = "https://deb.debian.org/debian"
DEBIAN_INDEX = f"{DEBIAN}/dists/bookworm/main/binary-armhf/Packages.xz"
DEBIAN_PACKAGES = ["libsdl2-dev", "libsdl2-2.0-0", "libgles-dev", "libgles2", "libegl-dev"]

# Same flags as pc/build-armhf-docker.sh: Allwinner H700 = Cortex-A53, armv7 userland, NEON.
TUNE = "-fsigned-char -mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard"


def log(msg):
    print(f"=== {msg}", flush=True)


def long_path(p):
    # The toolchain has deep paths; skip MAX_PATH on Windows.
    p = str(Path(p).resolve())
    return "\\\\?\\" + p if os.name == "nt" and not p.startswith("\\\\?\\") else p


def download(url, dest, sha256=None):
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and (sha256 is None or file_sha256(dest) == sha256):
        return dest
    tmp = dest.with_suffix(dest.suffix + ".part")
    req = urllib.request.Request(url, headers={"User-Agent": "opencrossing-build"})
    with urllib.request.urlopen(req) as r, open(tmp, "wb") as f:
        total = int(r.headers.get("Content-Length") or 0)
        done = 0
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
            done += len(chunk)
            if total > 8 << 20:
                print(f"\r    {dest.name}: {done >> 20}/{total >> 20} MB", end="", flush=True)
        if total > 8 << 20:
            print()
    if sha256 is not None and file_sha256(tmp) != sha256:
        tmp.unlink()
        sys.exit(f"ERROR: checksum mismatch for {url}")
    tmp.replace(dest)
    return dest


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch_gcc():
    gcc = DEPS / "gcc"
    stamp = gcc / ".version"
    if stamp.exists() and stamp.read_text().strip() == GCC_VERSION:
        return
    log(f"downloading Arm GNU Toolchain {GCC_VERSION} (~240 MB)")
    zpath = download(GCC_URL, DEPS / "downloads" / GCC_ZIP, GCC_SHA256)
    log("unpacking toolchain")
    if gcc.exists():
        shutil.rmtree(long_path(gcc))
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            # Drop the long top-level folder name.
            parts = info.filename.split("/", 1)
            if len(parts) < 2 or not parts[1]:
                continue
            out = gcc / parts[1]
            if info.is_dir():
                os.makedirs(long_path(out), exist_ok=True)
                continue
            os.makedirs(long_path(out.parent), exist_ok=True)
            with z.open(info) as src, open(long_path(out), "wb") as dst:
                shutil.copyfileobj(src, dst)
    stamp.write_text(GCC_VERSION + "\n")


def read_ar(data):
    """Yield (name, bytes) from a Debian .deb (an ar archive)."""
    assert data[:8] == b"!<arch>\n", "not a .deb"
    pos = 8
    while pos + 60 <= len(data):
        hdr = data[pos:pos + 60]
        name = hdr[:16].decode().strip().rstrip("/")
        size = int(hdr[48:58])
        pos += 60
        yield name, data[pos:pos + size]
        pos += size + (size & 1)


def fetch_sysroot():
    sysroot = DEPS / "sysroot"
    stamp = sysroot / ".packages"
    want = " ".join(DEBIAN_PACKAGES)
    if stamp.exists() and stamp.read_text().strip() == want:
        return
    log("reading the Debian bookworm armhf package index")
    index = urllib.request.urlopen(DEBIAN_INDEX).read()
    pkgs = {}
    for stanza in lzma.decompress(index).decode("utf-8", "replace").split("\n\n"):
        fields = dict(line.split(": ", 1) for line in stanza.splitlines() if ": " in line and not line.startswith(" "))
        if fields.get("Package") in DEBIAN_PACKAGES:
            pkgs[fields["Package"]] = fields
    missing = set(DEBIAN_PACKAGES) - set(pkgs)
    if missing:
        sys.exit(f"ERROR: not in the Debian index: {', '.join(sorted(missing))}")

    if sysroot.exists():
        shutil.rmtree(sysroot)
    links = []
    for name in DEBIAN_PACKAGES:
        p = pkgs[name]
        log(f"fetching {name} {p['Version']}")
        deb = download(f"{DEBIAN}/{p['Filename']}", DEPS / "downloads" / Path(p["Filename"]).name, p["SHA256"])
        for member, blob in read_ar(deb.read_bytes()):
            if not member.startswith("data.tar"):
                continue
            with tarfile.open(fileobj=io.BytesIO(blob)) as tar:
                for m in tar.getmembers():
                    rel = m.name.lstrip("./")
                    if not (rel.startswith("usr/include/") or rel.startswith("usr/lib/arm-linux-gnueabihf/")):
                        continue
                    out = sysroot / rel
                    if m.isdir():
                        out.mkdir(parents=True, exist_ok=True)
                    elif m.issym():
                        links.append((out, m.linkname))
                    elif m.isfile():
                        out.parent.mkdir(parents=True, exist_ok=True)
                        out.write_bytes(tar.extractfile(m).read())

    # Windows can't make symlinks without admin rights; copy the targets instead.
    for _ in range(4):
        pending = []
        for out, target in links:
            src = (sysroot / target.lstrip("/")) if target.startswith("/") else (out.parent / target)
            if src.is_file():
                out.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, out)
            else:
                pending.append((out, target))
        links = pending
    # Whatever is left points into packages we don't fetch (libEGL.so.1 etc.).

    cfg = DEPS / "cmake"
    cfg.mkdir(parents=True, exist_ok=True)
    s = sysroot.as_posix()
    (cfg / "SDL2Config.cmake").write_text(
        "# Generated by pc/tools/build_windows.py\n"
        "set(SDL2_FOUND TRUE)\n"
        f'set(SDL2_INCLUDE_DIRS "{s}/usr/include/SDL2;{s}/usr/include;{s}/usr/include/arm-linux-gnueabihf/SDL2")\n'
        f'set(SDL2_LIBRARIES "{s}/usr/lib/arm-linux-gnueabihf/libSDL2.so")\n')
    stamp.write_text(want + "\n")


def need_tool(name):
    path = shutil.which(name)
    if path:
        return path
    # `pip install --user cmake ninja` often lands outside PATH; ask the modules.
    try:
        if name == "cmake":
            import cmake
            bin_dir = cmake.CMAKE_BIN_DIR
        else:
            import ninja
            bin_dir = ninja.BIN_DIR
        path = shutil.which(name, path=bin_dir)
    except (ImportError, AttributeError):
        path = None
    if not path:
        sys.exit(f"ERROR: {name} not found. Install it with: pip install cmake ninja")
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--clean", action="store_true", help="delete the build dir first")
    ap.add_argument("--fetch-only", action="store_true", help="only download the toolchain and libraries")
    args = ap.parse_args()

    fetch_gcc()
    fetch_sysroot()
    if args.fetch_only:
        log(f"toolchain ready in {DEPS}")
        return

    cmake = need_tool("cmake")
    ninja = need_tool("ninja")
    if args.clean and BUILD.exists():
        shutil.rmtree(BUILD)

    if not (BUILD / "build.ninja").exists():
        log("cmake configure")
        subprocess.run([cmake, "-S", str(PC), "-B", str(BUILD), "-G", "Ninja",
                        f"-DCMAKE_MAKE_PROGRAM={ninja}",
                        f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN_FILE}",
                        "-DPC_USE_GLES=ON",
                        f"-DCMAKE_C_FLAGS={TUNE}",
                        f"-DCMAKE_CXX_FLAGS={TUNE}"], check=True)

    log(f"building with {args.jobs} jobs")
    r = subprocess.run([cmake, "--build", str(BUILD), "--", f"-j{args.jobs}"])
    if r.returncode != 0:
        sys.exit(r.returncode)

    binary = BUILD / "bin" / "AnimalCrossing"
    log(f"done: {binary.relative_to(REPO).as_posix()}")


if __name__ == "__main__":
    main()
