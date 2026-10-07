#!/bin/sh
# Runs every command (and some error paths) under Valgrind.
# Usage: sh tests/valgrind.sh ./myar     (build with `make debug` for best traces)
set -eu

BIN=$(cd "$(dirname "${1:-./myar}")" && pwd)/$(basename "${1:-./myar}")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"

vg() { valgrind -q --leak-check=full --show-leak-kinds=all --track-origins=yes \
                --track-fds=yes --error-exitcode=99 "$BIN" "$@"; }
# expect_fail: command must fail normally (rc 1), not because Valgrind found an error (rc 99)
expect_fail() { rc=0; vg "$@" >/dev/null 2>&1 || rc=$?; [ "$rc" -eq 1 ] || { echo "FAIL: '$*' rc=$rc"; exit 1; }; }

printf 'hello\n' > a.txt; : > empty.txt; head -c 300000 /dev/urandom > b.bin

vg pack  t.myar a.txt empty.txt b.bin >/dev/null
vg list  t.myar >/dev/null
vg test  t.myar >/dev/null
vg extract t.myar out >/dev/null

expect_fail pack dup.myar a.txt a.txt
expect_fail extract t.myar out          # files already exist
head -c 500 t.myar > trunc.myar
expect_fail test trunc.myar
printf 'not an archive' > junk.myar
expect_fail list junk.myar

echo "valgrind: 0 errors, no leaks, no leaked file descriptors"
