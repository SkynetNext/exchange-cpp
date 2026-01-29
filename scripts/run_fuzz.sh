#!/usr/bin/env bash
# Build and run libFuzzer targets for Binary API (BatchAddAccountsCommand, createCommand).
# Requires Clang. Use on Linux or Git Bash (Windows).
#
# Usage:
#   ./scripts/run_fuzz.sh              # build + run each fuzzer 10000 times
#   ./scripts/run_fuzz.sh -runs=50000   # run 50000 times each
#   ./scripts/run_fuzz.sh -runs=0       # run indefinitely (Ctrl+C to stop)
#   ./scripts/run_fuzz.sh --build-only  # only configure and build, do not run

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

BUILD_DIR="build_fuzz"
RUNS="-runs=10000"
BUILD_ONLY=0

while [[ $# -gt 0 ]]; do
  case $1 in
    -runs=*)
      RUNS="$1"
      shift
      ;;
    --build-only)
      BUILD_ONLY=1
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [options]"
      echo "  -runs=N       Run each fuzzer N times (default: 10000). -runs=0 = run until Ctrl+C"
      echo "  --build-only  Only configure and build, do not run fuzzers"
      echo "  --help        Show this help"
      exit 0
      ;;
    *)
      echo -e "${RED}Unknown option: $1${NC}"
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Prefer CXX if set, otherwise clang++
if [[ -n "$CXX" ]]; then
  CXX_COMPILER="$CXX"
else
  if ! command -v clang++ &>/dev/null; then
    echo -e "${RED}clang++ not found. Install Clang or set CXX.${NC}"
    exit 1
  fi
  CXX_COMPILER="clang++"
fi

if ! "$CXX_COMPILER" --version 2>/dev/null | grep -qi clang; then
  echo -e "${RED}LibFuzzer requires Clang. CXX=$CXX_COMPILER is not Clang.${NC}"
  exit 1
fi

echo -e "${GREEN}=== Fuzz build (Clang + libFuzzer) ===${NC}"
echo "Compiler: $CXX_COMPILER"
echo "Build dir: $BUILD_DIR"
[[ -n "$MSYSTEM" || "$(uname -s)" == MINGW* ]] && echo "Windows: fuzzer+UBSan only (no ASan); Linux: fuzzer+ASan+UBSan"
echo ""

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Fuzz build: Release everywhere (faster, and on Windows matches sanitizer _ITERATOR_DEBUG_LEVEL)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_FUZZ=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -G "Ninja" \
  -DCMAKE_MAKE_PROGRAM=ninja 2>/dev/null || \
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_FUZZ=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER"

cmake --build . --target fuzz_batch_add_accounts fuzz_binary_command_factory

if [[ $BUILD_ONLY -eq 1 ]]; then
  echo -e "${GREEN}Build done. Run fuzzers manually from $BUILD_DIR/tests/fuzz/ or ./fuzz_*${NC}"
  exit 0
fi

# Run fuzzers from build dir; executables may be in . or tests/ depending on CMake/OS
run_fuzz() {
  local name="$1"
  local ext=""
  [[ -n "$MSYSTEM" || "$(uname -s)" == MINGW* ]] && ext=".exe"
  for exe in "./$name$ext" "./$name" "./tests/$name$ext" "./tests/$name"; do
    if [[ -x "$exe" ]]; then
      echo -e "${GREEN}Running $name $RUNS${NC}"
      # Cap RSS to avoid OOM; override with -rss_limit_mb=N if needed
      local rss_limit="-rss_limit_mb=1536"
      if [[ "$RUNS" == "-runs=0" ]]; then
        "$exe" -runs=0 $rss_limit
      else
        "$exe" $RUNS $rss_limit
      fi
      return 0
    fi
  done
  echo -e "${RED}$name executable not found${NC}"
  return 1
}

run_fuzz fuzz_batch_add_accounts
run_fuzz fuzz_binary_command_factory

echo -e "${GREEN}=== Fuzz run finished ===${NC}"
