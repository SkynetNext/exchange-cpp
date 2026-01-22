# Build Guide

## Prerequisites

- **CMake** 3.30 or higher (required for C++26 support)
- **C++26** compatible compiler:
  - **GCC 14+** (recommended) or **Clang 19+**
  - MSVC 19.40+ (Windows)
- **Git** (for submodules)

## Quick Start

```bash
# Clone with submodules
git clone --recursive <repository-url>
cd exchange-cpp

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Build Options

```bash
# Run tests (requires Google Test)
cmake .. -DBUILD_TESTING=ON
cmake --build . -j$(nproc)
ctest --output-on-failure

# Build benchmarks (requires Google Benchmark)
cmake .. -DBUILD_BENCHMARKS=ON
cmake --build . -j$(nproc)
```

## Installing GCC 15

### Ubuntu 24.04+

```bash
sudo apt install gcc-15 g++-15 gdb-15
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 100
sudo update-alternatives --install /usr/bin/gdb gdb /usr/bin/gdb-15 100
```

### Ubuntu 22.04 (Build from Source)

```bash
sudo apt install build-essential libmpfr-dev libgmp3-dev libmpc-dev -y
wget http://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.gz
tar -xf gcc-15.2.0.tar.gz
cd gcc-15.2.0
./configure -v --build=$(uname -m)-linux-gnu --host=$(uname -m)-linux-gnu \
    --target=$(uname -m)-linux-gnu --prefix=/usr/local/gcc-15.2.0 \
    --enable-checking=release --enable-languages=c,c++ \
    --disable-multilib --program-suffix=-15.2.0
make -j$(nproc)
sudo make install

# Set as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/local/gcc-15.2.0/bin/gcc-15.2.0 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/local/gcc-15.2.0/bin/g++-15.2.0 100
sudo update-alternatives --set gcc /usr/local/gcc-15.2.0/bin/gcc-15.2.0
sudo update-alternatives --set g++ /usr/local/gcc-15.2.0/bin/g++-15.2.0

# Add to library path
echo 'export LD_LIBRARY_PATH=/usr/local/gcc-15.2.0/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# Verify
gcc --version
```

## Third-Party Dependencies

The project uses the following submodules:

| Submodule | Description |
|-----------|-------------|
| `reference/exchange-core` | Original Java implementation for reference |
| `third_party/disruptor-cpp` | High-performance inter-thread communication library |

Initialize submodules if not cloned with `--recursive`:

```bash
git submodule update --init --recursive
```

## System Optimization

For optimal performance, configure CPU isolation to reduce kernel scheduler overhead.

```bash
sudo nano /etc/default/grub
# Add to GRUB_CMDLINE_LINUX: isolcpus=8-15 nohz_full=8-15 rcu_nocbs=8-15
sudo update-grub
sudo reboot
```

**Verify:** `cat /sys/devices/system/cpu/isolated` should show `8-15`

**Note:** Reserve some CPUs (e.g., 0-7) for system use. Never isolate all CPUs.

## Running Tests & Benchmarks

```bash
cd build
ctest --output-on-failure          # Run all tests
./tests/perf/PerfLatency           # Run latency benchmarks
./benchmarks/perf_long_adaptive_radix_tree_map  # ART benchmarks
```

See [PERFORMANCE_BENCHMARK_COMPARISON.md](PERFORMANCE_BENCHMARK_COMPARISON.md) for detailed benchmark results.
