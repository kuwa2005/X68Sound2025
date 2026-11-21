#!/bin/bash
# =============================================================================
# X68Sound2025 test runner (Linux)
#   - test_audio_fixes      : legacy tests (self-contained copies of functions)
#   - test_units            : internals of the real source (ring buffer, RNG, effects)
#   - test_render_integration: real X68Sound.cpp rendered offline through the
#                             public DLL API + environment variables
#   - mxwav                 : MDX -> WAV end-to-end (MXDRVg + X68Sound2025)
# The real source is compiled with a Win32 API stub (compat/linux).
# Usage: tests/run_tests.sh
# =============================================================================
set -u
cd "$(dirname "$0")"
OUT=build
mkdir -p "$OUT"
CXX=${CXX:-g++}
FLAGS="-std=c++14 -O2 -D_WIN64 -fno-operator-names -fpermissive -w -I../compat/linux"

fail=0
build_and_run() {
	local name=$1; shift
	echo
	echo "######## $name ########"
	if ! $CXX "$@" -o "$OUT/$name" -lm; then
		echo "BUILD FAILED: $name"; fail=1; return
	fi
	"./$OUT/$name" || fail=1
}

build_and_run test_audio_fixes -O2 test_audio_fixes.cpp
build_and_run test_units $FLAGS test_units.cpp
build_and_run test_render_integration $FLAGS test_render_integration.cpp

# mxwav (MXDRVg + X68Sound2025) end-to-end tests: real MDX/PDX -> WAV
echo
echo "######## mxwav ########"
if ../mxwav/build.sh linux >/dev/null && ../mxwav/tests/run_mxwav_tests.sh; then :; else fail=1; fi

echo
if [ $fail -eq 0 ]; then echo "ALL TEST SUITES PASSED"; else echo "SOME TEST SUITES FAILED"; fi
exit $fail
