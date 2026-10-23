#!/bin/sh
# yaft-dalmoori-8x8 build script
set -e

case "$(locale charmap 2>/dev/null)" in
    UTF-8) ;;
    *) echo "error: UTF-8 locale required (e.g. export LANG=ko_KR.UTF-8)" >&2; exit 1 ;;
esac

command -v pkg-config >/dev/null || { echo "error: pkg-config (base-devel) required" >&2; exit 1; }
pkg-config --exists libhangul || { echo "error: libhangul required" >&2; exit 1; }

make clean
make
echo
echo "built: ./yaft"
echo "install: sudo make install"
