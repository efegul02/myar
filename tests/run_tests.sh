#!/bin/sh
# Usage: sh tests/run_tests.sh ./myar
set -eu

BIN=$(cd "$(dirname "${1:-./myar}")" && pwd)/$(basename "${1:-./myar}")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; exit 1; }

printf 'hello\n' > a.txt
: > empty.txt
head -c 1000000 /dev/urandom > rand.bin
head -c 1000 /dev/zero > zeros.bin

# 1. round trip (text, empty file, binary)
"$BIN" pack all.myar a.txt empty.txt rand.bin >/dev/null || fail "pack"
mkdir out
"$BIN" extract all.myar out >/dev/null || fail "extract"
for f in a.txt empty.txt rand.bin; do
    cmp "$f" "out/$f" || fail "round trip of $f"
done
pass "round trip"

# 2. list and test
"$BIN" list all.myar | grep -q rand.bin || fail "list"
"$BIN" test all.myar >/dev/null || fail "test"
pass "list / test"

# 3. never overwrite existing files
if "$BIN" extract all.myar out >/dev/null 2>&1; then fail "overwrite protection"; fi
pass "overwrite protection"

# 4. duplicate names are rejected and no archive is left behind
mkdir sub
printf 'x' > sub/a.txt
if "$BIN" pack dup.myar a.txt sub/a.txt >/dev/null 2>&1; then fail "duplicate names"; fi
[ ! -e dup.myar ] || fail "half-written archive left behind"
pass "duplicate names"

# 5. corrupted data is detected
"$BIN" pack z.myar zeros.bin >/dev/null
printf '\377' | dd of=z.myar bs=1 seek=300 conv=notrunc 2>/dev/null
if "$BIN" test z.myar >/dev/null 2>&1; then fail "corruption not detected"; fi
pass "corruption detected"

# 6. garbage and truncated archives are rejected
printf 'this is not an archive' > junk.myar
if "$BIN" list junk.myar >/dev/null 2>&1; then fail "bad magic"; fi
head -c 500 all.myar > trunc.myar
if "$BIN" test trunc.myar >/dev/null 2>&1; then fail "truncated archive"; fi
pass "invalid archives rejected"

echo "all tests passed"
