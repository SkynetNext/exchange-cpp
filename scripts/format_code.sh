#!/bin/bash
# Format C++ source files using clang-format
# This script formats all files in the directories checked by the CI clang-format job
# Usage: ./scripts/format_code.sh

set -e

# Colors for output (compatible with git bash and Linux)
if [ -t 1 ]; then
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  YELLOW='\033[1;33m'
  NC='\033[0m' # No Color
else
  RED=''
  GREEN=''
  YELLOW=''
  NC=''
fi

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
  echo -e "${RED}Error: clang-format not found in PATH${NC}"
  echo "Please install clang-format and ensure it's in your PATH"
  exit 1
fi

echo "Using clang-format: $(clang-format --version)"

# Directories to format (same as CI job)
DIRS="include tests examples benchmarks"

# File extensions to format
EXTENSIONS="cpp hpp h"

# Build find command to locate all C++ files
# Exclude third_party and reference directories
# Use a temporary file to store file list (more compatible with git bash)
TMPFILE=$(mktemp 2>/dev/null || echo "/tmp/format_code_$$.txt")
trap "rm -f '$TMPFILE'" EXIT

for dir in $DIRS; do
  if [ ! -d "$dir" ]; then
    echo -e "${YELLOW}Warning: Directory '$dir' not found, skipping${NC}"
    continue
  fi
  
  for ext in $EXTENSIONS; do
    # Find files, exclude third_party and reference directories
    # Use -type f to only match files, not directories
    find "$dir" -type f -name "*.${ext}" 2>/dev/null | \
      grep -v "/third_party/" | \
      grep -v "/reference/" >> "$TMPFILE" || true
  done
done

# Count files
FILE_COUNT=$(wc -l < "$TMPFILE" 2>/dev/null | tr -d ' ' || echo "0")

if [ "$FILE_COUNT" -eq 0 ] || [ ! -s "$TMPFILE" ]; then
  echo -e "${YELLOW}No C++ files found to format${NC}"
  exit 0
fi

echo -e "${GREEN}Found $FILE_COUNT file(s) to format${NC}"
echo ""

# Format each file
FORMATTED=0
FAILED=0

while IFS= read -r file || [ -n "$file" ]; do
  if [ -z "$file" ] || [ ! -f "$file" ]; then
    continue
  fi
  
  echo -n "Formatting: $file ... "
  
  if clang-format -i "$file" 2>/dev/null; then
    echo -e "${GREEN}✓${NC}"
    FORMATTED=$((FORMATTED + 1))
  else
    echo -e "${RED}✗${NC}"
    FAILED=$((FAILED + 1))
  fi
done < "$TMPFILE"

echo ""
if [ $FAILED -eq 0 ]; then
  echo -e "${GREEN}Successfully formatted $FORMATTED file(s)${NC}"
  exit 0
else
  echo -e "${RED}Failed to format $FAILED file(s)${NC}"
  exit 1
fi
