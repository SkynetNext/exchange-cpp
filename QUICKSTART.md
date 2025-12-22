# Quick Start Guide

## Initial Setup

### 1. Initialize Git Submodules

This project depends on two submodules:

- **`reference/exchange-core`**: Original Java implementation (for reference)
- **`third_party/disruptor-cpp`**: C++ Disruptor library (required dependency)

```bash
# Initialize all submodules
git submodule update --init --recursive

# Or clone with submodules from the start
git clone --recursive <repository-url>
```

### 2. Verify Submodule Status

```bash
git submodule status
```

You should see:
```
<commit-hash> reference/exchange-core (vX.X.X)
<commit-hash> third_party/disruptor-cpp (vX.X.X)
```

### 3. Build the Project

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)  # Linux/Mac
# or
cmake --build . -j%NUMBER_OF_PROCESSORS%  # Windows
```

### 4. Verify Build

```bash
# Check if disruptor-cpp is linked correctly
cmake --build . --target disruptor-cpp

# List all targets
cmake --build . --target help
```

## Project Structure

```
exchange-cpp/
├── CMakeLists.txt              # Main CMake configuration
├── third_party/
│   └── disruptor-cpp/          # Git submodule (required)
├── reference/
│   └── exchange-core/          # Git submodule (reference only)
├── include/exchange/           # Public headers
│   ├── core/                   # Matching engine
│   ├── api/                    # Public API
│   ├── model/                  # Data models
│   └── risk/                   # Risk management
├── src/exchange/               # Implementation files
├── tests/                      # Unit tests
├── benchmarks/                 # Performance benchmarks
└── examples/                   # Usage examples
```

## Troubleshooting

### Submodule Not Found

If you see an error like:
```
disruptor-cpp submodule not found. Please run: git submodule update --init --recursive
```

**Solution:**
```bash
git submodule update --init --recursive
```

### CMake Can't Find disruptor-cpp

**Solution:** Ensure the submodule is initialized and the path is correct:
```bash
ls third_party/disruptor-cpp/CMakeLists.txt  # Should exist
```

### Build Errors

1. **Check C++ standard:** Ensure your compiler supports C++17
2. **Check CMake version:** Requires CMake 3.20+
3. **Clean build:** Remove `build/` directory and rebuild

```bash
rm -rf build/
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Next Steps

1. Review the [System Architecture](README.md#system-architecture) in README.md
2. Check the [Development Plan](PLAN.md) for implementation roadmap
3. Start implementing core data models (see Phase 2 in PLAN.md)

