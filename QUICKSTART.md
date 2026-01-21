# Quick Start

## Prerequisites

- **CMake** 3.30+
- **GCC 14+** or **Clang 19+** (C++26 support required)
- See [README.md](README.md) for detailed GCC 14 installation instructions

## Clone and Build

```bash
# 1. Clone the repository
git clone <repository-url>
cd exchange-cpp

# 2. Initialize submodules
git submodule update --init --recursive

# 3. Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Verify

```bash
# Check submodule status
git submodule status

# Should see:
# <hash> reference/exchange-core (...)
# <hash> third_party/disruptor-cpp (...)
```

For detailed information, see [README.md](README.md) and [PLAN.md](PLAN.md).

