#!/bin/sh
# This is the source code of AyuGram for Desktop.
#
# We do not and cannot prevent the use of our code,
# but be respectful and credit the original author.
#
# Copyright @Radolyn, 2026
#
# Compiles Telegram/SourceFiles/ayu/data/ayu_database.cpp against the vendored
# sqlite amalgamation, plus ayu/data/entities.h and the argument-only helpers of
# ayu/utils/telegram_helpers.cpp, and runs the behaviour tests over all three.
# The main project build is not involved: only those sources and a few Qt/base
# shims are used.

set -eu

tests="$(cd "$(dirname "$0")" && pwd)"
sources="$(cd "$tests/../.." && pwd)"
sqlite="$sources/ayu/libs/sqlite"
build="${TMPDIR:-/tmp}/ayugram_ayu_tests"

mkdir -p "$build"

if [ ! -f "$build/sqlite3.o" ] || [ "$sqlite/sqlite3.c" -nt "$build/sqlite3.o" ]; then
	echo "compiling sqlite3 amalgamation..."
	cc -O1 -w -DSQLITE_THREADSAFE=1 -c "$sqlite/sqlite3.c" -o "$build/sqlite3.o"
fi

vendored="-Wno-deprecated-declarations -Wno-deprecated-literal-operator -Wno-c++26-extensions"
vendored="$vendored -Wno-unused-but-set-variable -Wno-unused-local-typedef"
flags="-std=c++20 -O0 -g -fexceptions -Wall $vendored -I$sources -I$tests/shims -I$sqlite -include $tests/ayu_test_shims.h"

# entities.h must name every header it uses. Compiled here on its own, with no
# forced prelude, so a type it only gets through some other includer's headers
# is a hard error instead of a build that breaks the day an include order moves.
echo "checking entities.h is self-contained..."
printf '#include "ayu/data/entities.h"\n' > "$build/entities_self_contained.cpp"
c++ -std=c++20 -Wall -fsyntax-only -I"$sources" -I"$tests/shims" \
	"$build/entities_self_contained.cpp"

echo "compiling ayu_database.cpp..."
# shellcheck disable=SC2086
c++ $flags -c "$sources/ayu/data/ayu_database.cpp" -o "$build/ayu_database.o"

# telegram_helpers.cpp cannot be compiled on its own, so the functions that
# depend on nothing beyond their arguments are copied out of it verbatim.
echo "copying pure helpers out of telegram_helpers.cpp..."
python3 "$tests/extract_functions.py" \
	--source "$sources/ayu/utils/telegram_helpers.cpp" \
	--output "$build/telegram_helpers_pure.cpp" \
	--include "ayu/data/entities.h" \
	--system-include algorithm \
	--system-include cmath \
	"ID getUserIdFromPackId(uint64 id)" \
	"int getScheduleTime(int64 sumSize)" \
	"QString getDCName(int dc)" \
	"QString formatTTL(int time, bool isDoc)"

echo "compiling telegram_helpers_pure.cpp..."
# shellcheck disable=SC2086
c++ $flags -c "$build/telegram_helpers_pure.cpp" -o "$build/telegram_helpers_pure.o"

for suite in ayu_database_tests ayu_entities_tests ayu_telegram_helpers_tests; do
	echo "compiling $suite.cpp..."
	# shellcheck disable=SC2086
	c++ $flags -c "$tests/$suite.cpp" -o "$build/$suite.o"
done

echo "linking..."
c++ -o "$build/ayu_tests" \
	"$build/ayu_database_tests.o" \
	"$build/ayu_entities_tests.o" \
	"$build/ayu_telegram_helpers_tests.o" \
	"$build/telegram_helpers_pure.o" \
	"$build/ayu_database.o" \
	"$build/sqlite3.o"

echo "running..."
"$build/ayu_tests"
