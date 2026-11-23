#!/usr/bin/env python3
"""Build the pinned libheif in an isolated prefix (Linux/macOS/MSYS2 Python)."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="isolated ignored build directory")
    parser.add_argument("--archive", type=Path, help="use an existing archive instead of downloading")
    parser.add_argument("--plugins", action="store_true", help="build x265/libde265 as loadable plugins")
    args = parser.parse_args()
    lock = json.loads(Path(__file__).with_name("dependencies.json").read_text())["libheif"]
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    archive = args.archive.resolve() if args.archive else directory / "libheif-source.tar.gz"
    if not archive.exists():
        if args.archive:
            raise FileNotFoundError(archive)
        with urllib.request.urlopen(lock["archive"], timeout=60) as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != lock["sha256"]:
        raise RuntimeError("Pinned libheif archive SHA-256 does not match")
    source = directory / ("libheif-" + lock["commit"])
    if not source.exists():
        with tarfile.open(archive) as tar:
            # The verified source archive is extracted only below this prefix.
            for member in tar.getmembers():
                path = (directory / member.name).resolve()
                if directory not in path.parents or member.issym() or member.islnk():
                    raise RuntimeError("Unexpected archive member: " + member.name)
            tar.extractall(directory)
    mode = "plugins" if args.plugins else "builtin"
    build = directory / (mode + "-build")
    prefix = directory / (mode + "-install")
    enabled = "ON" if args.plugins else "OFF"
    configure = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
        "-DCMAKE_INSTALL_LIBDIR=lib", "-DBUILD_TESTING=OFF", "-DBUILD_DOCUMENTATION=OFF",
        "-DWITH_EXAMPLES=OFF", "-DWITH_GDK_PIXBUF=OFF", "-DENABLE_PLUGIN_LOADING=" + enabled,
        "-DWITH_X265=ON", "-DWITH_LIBDE265=ON", "-DWITH_X265_PLUGIN=" + enabled,
        "-DWITH_LIBDE265_PLUGIN=" + enabled, "-DWITH_AOM_DECODER=OFF", "-DWITH_AOM_ENCODER=OFF",
        "-DWITH_X264=OFF", "-DWITH_OpenH264_DECODER=OFF", "-DWITH_HEADER_COMPRESSION=OFF"]
    subprocess.run(configure, check=True)
    command = ["cmake", "--build", str(build), "-j4"]
    if os.name == "posix":
        command = ["nice", "-n", "10"] + command
    subprocess.run(command, check=True)
    subprocess.run(["cmake", "--install", str(build)], check=True)
    versions = {}
    for package in ("libde265", "x265", "lcms2", "glib-2.0"):
        versions[package] = subprocess.check_output(
            ["pkg-config", "--modversion", package], text=True).strip()
    (directory / (mode + "-versions.json")).write_text(
        json.dumps(dict(libheif=lock, dependencies=versions), indent=2) + "\n", encoding="utf-8")
    print("Prefix:", prefix)
    print("Add to PKG_CONFIG_PATH:", prefix / "lib/pkgconfig")


if __name__ == "__main__":
    main()
