#!/bin/bash
# Run clang-tidy on all source files in the project

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${RED}Error: clang-tidy not found in PATH${NC}"
    echo "Please install clang-tidy:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-tidy"
    echo "  macOS: brew install llvm"
    echo "  Windows: Install LLVM from https://llvm.org/builds/ or use: choco install llvm"
    exit 1
fi

# Check if compile_commands.json exists
BUILD_DIR="${1:-build}"
COMPILE_COMMANDS="${BUILD_DIR}/compile_commands.json"

if [ ! -f "$COMPILE_COMMANDS" ]; then
    echo -e "${YELLOW}Warning: compile_commands.json not found at ${COMPILE_COMMANDS}${NC}"
    echo "Please run CMake first to generate compile_commands.json:"
    echo "  mkdir -p ${BUILD_DIR} && cd ${BUILD_DIR}"
    echo "  cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    exit 1
fi

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${GREEN}Running clang-tidy on project source files...${NC}"
echo "Build directory: ${BUILD_DIR}"
echo ""

# Find all C++ source files (excluding third_party and build directories)
# Use find with proper path handling for Windows Git Bash
if [ -d "src" ] || [ -d "include" ] || [ -d "tests" ]; then
    FILES=$(find src include tests -type f \( -name "*.cpp" -o -name "*.h" \) \
        ! -path "*/third_party/*" \
        ! -path "*/build/*" \
        ! -path "*/.git/*" 2>/dev/null | sort)
else
    echo -e "${RED}Error: src, include, or tests directories not found${NC}"
    echo "Current directory: $(pwd)"
    exit 1
fi

if [ -z "$FILES" ]; then
    echo -e "${RED}Error: No source files found${NC}"
    exit 1
fi

# Count files
FILE_COUNT=$(echo "$FILES" | wc -l)
echo "Found ${FILE_COUNT} source files"
echo ""

# Check if clang-tidy supports -j flag (parallel execution)
# Windows versions of clang-tidy typically don't support -j
SUPPORTS_PARALLEL=false
if clang-tidy --help 2>&1 | grep -qE "\-j[[:space:]]|--jobs"; then
    SUPPORTS_PARALLEL=true
fi

# Determine number of jobs for parallel execution
if command -v nproc &> /dev/null; then
    NUM_JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
    NUM_JOBS=$(sysctl -n hw.ncpu)
elif [ -n "$NUMBER_OF_PROCESSORS" ]; then
    # Windows environment variable
    NUM_JOBS="$NUMBER_OF_PROCESSORS"
else
    NUM_JOBS=4
fi

if [ "$SUPPORTS_PARALLEL" = true ]; then
    echo "Running clang-tidy with ${NUM_JOBS} parallel jobs (using -j flag)..."
    echo ""
    
    # Convert files to array for better handling
    FILE_ARRAY=()
    while IFS= read -r line; do
        FILE_ARRAY+=("$line")
    done <<< "$FILES"
    
    # Run clang-tidy with -j flag
    if clang-tidy -p "${BUILD_DIR}" -j "${NUM_JOBS}" "${FILE_ARRAY[@]}" 2>&1; then
        echo ""
        echo -e "${GREEN}✓ clang-tidy completed successfully${NC}"
        exit 0
    else
        EXIT_CODE=$?
        echo ""
        echo -e "${RED}✗ clang-tidy found issues${NC}"
        echo ""
        echo "To fix issues automatically (where possible), run:"
        echo "  clang-tidy -p ${BUILD_DIR} -j ${NUM_JOBS} -fix ${FILE_ARRAY[*]}"
        exit $EXIT_CODE
    fi
else
    # Windows or older clang-tidy: use xargs for parallel processing
    echo "Running clang-tidy using xargs for parallel processing (${NUM_JOBS} jobs)..."
    echo "Note: This version of clang-tidy doesn't support -j flag"
    echo ""
    
    FAILED=0
    # Use xargs -P for parallel execution
    # -n 1: process one file at a time per job
    # -P: number of parallel jobs
    # -I {}: replace {} with the file name
    echo "$FILES" | xargs -n 1 -P "${NUM_JOBS}" -I {} sh -c "clang-tidy -p \"${BUILD_DIR}\" \"{}\" 2>&1" || FAILED=1
    
    if [ $FAILED -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ clang-tidy completed successfully${NC}"
        exit 0
    else
        echo ""
        echo -e "${RED}✗ clang-tidy found issues${NC}"
        echo ""
        echo "To fix issues automatically (where possible), run:"
        echo "  echo \"\$FILES\" | xargs -n 1 -P ${NUM_JOBS} -I {} clang-tidy -p ${BUILD_DIR} -fix \"{}\""
        exit 1
    fi
fi
