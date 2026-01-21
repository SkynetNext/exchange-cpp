# Code Quality Tools

This document describes the code quality tools and sanitizers used in the project.

## Overview

The project uses multiple layers of code quality checks:

1. **Sanitizers** - Runtime memory and thread safety checks
2. **Static Analysis** - Compile-time code analysis
3. **Code Formatting** - Consistent code style enforcement

## Sanitizers

Sanitizers are compiler instrumentation tools that detect bugs at runtime. They are enabled via compiler flags and require special build configurations.

### AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan) + LeakSanitizer (LSan)

**Purpose**: Detects memory errors, undefined behavior, and memory leaks.

**Detects**:
- Use-after-free
- Buffer overflows
- Use of uninitialized memory
- Memory leaks
- Undefined behavior (signed integer overflow, null pointer dereference, etc.)

**Configuration**:
```bash
-fsanitize=address,undefined
-fno-omit-frame-pointer
-fno-sanitize-recover=all
```

**Environment Variables**:
- `ASAN_OPTIONS`: Controls ASan behavior
  - `detect_leaks=1`: Enable leak detection (LSan)
  - `check_initialization_order=1`: Check initialization order
  - `strict_init_order=1`: Strict initialization order checking
- `UBSAN_OPTIONS`: Controls UBSan behavior
  - `print_stacktrace=1`: Print stack traces
  - `halt_on_error=1`: Stop on first error

**Performance Impact**: ~2x slower, ~2x more memory usage

**CI Job**: `sanitizer-asan-ubsan`

### MemorySanitizer (MSan) + UBSan

**Purpose**: Detects uninitialized memory reads.

**Detects**:
- Reading uninitialized memory
- Use of uninitialized values in conditionals
- Undefined behavior

**Configuration**:
```bash
-fsanitize=memory,undefined
-fsanitize-memory-track-origins
-fno-omit-frame-pointer
```

**Environment Variables**:
- `MSAN_OPTIONS`: Controls MSan behavior
  - `print_stats=1`: Print statistics
  - `halt_on_error=1`: Stop on first error

**Performance Impact**: ~3x slower, ~3x more memory usage

**CI Job**: `sanitizer-msan`

**Note**: MSan requires instrumented standard library. Clang is recommended.

### ThreadSanitizer (TSan)

**Purpose**: Detects data races and thread safety issues.

**Detects**:
- Data races (concurrent access to shared memory)
- Deadlocks
- Lock order violations

**Configuration**:
```bash
-fsanitize=thread
-fno-omit-frame-pointer
```

**Environment Variables**:
- `TSAN_OPTIONS`: Controls TSan behavior
  - `halt_on_error=1`: Stop on first error
  - `history_size=7`: Thread history size

**Performance Impact**: ~5-10x slower, ~5-10x more memory usage

**CI Job**: `sanitizer-tsan`

**Important**: TSan cannot be combined with other sanitizers. Must run separately.

## Static Analysis

### clang-tidy

**Purpose**: Static code analysis to find bugs, enforce coding standards, and suggest improvements.

**Configuration**: See `.clang-tidy` file

**Checks Enabled**:
- Bug detection (use-after-move, dangling handles, etc.)
- Modern C++ suggestions (use nullptr, auto, etc.)
- Performance warnings (inefficient vector operations)
- Code style enforcement (Google style guide)

**Usage**:
```bash
clang-tidy src/file.cpp -p build --config-file=.clang-tidy
```

**CI Job**: `static-analysis`

## Code Formatting

### clang-format

**Purpose**: Enforces consistent code formatting.

**Configuration**: See `.clang-format` file (based on Google style)

**Usage**:
```bash
# Check formatting
clang-format --dry-run --Werror file.cpp

# Fix formatting
clang-format -i file.cpp
```

**CI Job**: `code-format`

## Code Coverage

### gcov / lcov

**Purpose**: Measures test coverage to identify untested code paths.

**What it does**:
- Tracks which lines, branches, and functions are executed during tests
- Generates HTML reports showing coverage percentages
- Helps identify untested code paths

**Configuration**:
```bash
--coverage  # GCC flag (equivalent to -fprofile-arcs -ftest-coverage)
-g -O0      # Debug symbols, no optimization
```

**Usage**:
```bash
# Build with coverage flags
cmake .. -DCMAKE_CXX_FLAGS="--coverage -g -O0"

# Run tests
ctest

# Generate report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '*/third_party/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

**CI Job**: `code-coverage`

**Popularity**: Very popular in open-source C++ projects, widely used with GCC/Clang on Linux.

## CI Workflow

The code quality checks run in `.github/workflows/code-quality.yml`:

1. **Sanitizer - ASan+UBSan+LSan**: Most common memory errors
2. **Sanitizer - MSan+UBSan**: Uninitialized memory detection
3. **Sanitizer - TSan**: Thread safety checks
4. **Static Analysis**: clang-tidy checks
5. **Code Format**: clang-format validation
6. **Code Coverage**: gcov/lcov coverage analysis

### Running Locally

#### ASan + UBSan
```bash
mkdir build-asan && cd build-asan
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build . --parallel
ASAN_OPTIONS="detect_leaks=1" UBSAN_OPTIONS="print_stacktrace=1" ctest
```

#### MSan
```bash
mkdir build-msan && cd build-msan
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=memory,undefined -fno-omit-frame-pointer -fsanitize-memory-track-origins -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=memory,undefined"
cmake --build . --parallel
MSAN_OPTIONS="print_stats=1" ctest
```

#### TSan
```bash
mkdir build-tsan && cd build-tsan
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build . --parallel
TSAN_OPTIONS="halt_on_error=1" ctest
```

#### clang-tidy
```bash
mkdir build && cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build .
find ../src -name "*.cpp" | xargs clang-tidy -p . --config-file=../.clang-tidy
```

#### clang-format
```bash
# Check all files
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run --Werror

# Fix all files
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

#### gcov/lcov
```bash
mkdir build-coverage && cd build-coverage
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
  -DCMAKE_C_FLAGS="--coverage -g -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build . --parallel
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '*/third_party/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report

# View report
# Open coverage-report/index.html in browser
```

## Best Practices

1. **Run sanitizers before committing**: Catch memory errors early
2. **Fix clang-tidy warnings**: Improve code quality and maintainability
3. **Keep code formatted**: Use clang-format or enable editor integration
4. **Run TSan on multi-threaded code**: Essential for lock-free data structures
5. **Use MSan for new code**: Catch uninitialized memory issues
6. **Monitor code coverage**: Aim for high coverage on critical paths, identify untested code
7. **Exclude third-party code from coverage**: Focus on your own code

## Performance Considerations

- Sanitizers significantly slow down execution (2-10x)
- Use Debug builds with sanitizers (not Release)
- Disable LTO when using sanitizers
- Sanitizers are for testing, not production builds

## Limitations

- **ASan and MSan cannot be combined**: Choose one based on what you're testing
- **TSan cannot be combined with other sanitizers**: Must run separately
- **MSan requires instrumented standard library**: Clang recommended
- **Sanitizers may have false positives**: Review reports carefully

## References

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [MemorySanitizer Documentation](https://github.com/google/sanitizers/wiki/MemorySanitizer)
- [clang-tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [clang-format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [gcov Documentation](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)
- [lcov Documentation](https://github.com/linux-test-project/lcov)
