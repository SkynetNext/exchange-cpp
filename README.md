# Exchange-CPP

A high-performance, low-latency cryptocurrency exchange matching engine written in C++, 1:1 ported from [exchange-core](https://github.com/exchange-core/exchange-core).

## Overview

This project is a complete C++ rewrite of the Java-based exchange-core matching engine, utilizing [disruptor-cpp](https://github.com/SkynetNext/disruptor-cpp.git) for ultra-fast inter-thread communication, inspired by LMAX Disruptor.

## Features

- **Ultra-fast matching engine**: Lock-free, high-throughput order matching
- **Low latency**: Optimized for HFT (High-Frequency Trading) scenarios
- **Order book management**: Efficient order book data structures
- **Risk management**: Margin checks, position limits, and risk controls
- **Multi-currency support**: Support for multiple trading pairs
- **Event-driven architecture**: Based on disruptor-cpp ring buffer

## Technology Stack

- **C++17+**: Modern C++ features for performance and safety
- **disruptor-cpp**: High-performance inter-thread communication ([SkynetNext/disruptor-cpp](https://github.com/SkynetNext/disruptor-cpp.git))
- **CMake**: Build system
- **Google Test**: Unit testing framework
- **Google Benchmark**: Performance benchmarking

## Project Status

🚧 **In Development** - Initial planning and architecture phase

## Project Structure

```
exchange-cpp/
├── CMakeLists.txt          # Main CMake configuration
├── README.md               # This file
├── PLAN.md                 # Development plan
├── include/                # Public headers
│   └── exchange/
│       ├── core/          # Core matching engine
│       ├── api/           # Public API
│       ├── model/         # Data models (Order, Trade, etc.)
│       └── risk/          # Risk management
├── src/                    # Implementation files
│   └── exchange/
├── tests/                  # Unit tests
├── benchmarks/             # Performance benchmarks
├── examples/               # Usage examples
└── docs/                   # Documentation
```

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/your-org/exchange-cpp.git
cd exchange-cpp

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Testing

```bash
cd build
ctest --output-on-failure
```

## Benchmarks

```bash
cd build
./benchmarks/exchange_cpp_benchmarks
```

## References

- **Original Java Implementation**: [exchange-core](https://github.com/exchange-core/exchange-core)
- **Disruptor C++ Library**: [disruptor-cpp](https://github.com/SkynetNext/disruptor-cpp.git)
- **LMAX Disruptor**: [Official Documentation](https://lmax-exchange.github.io/disruptor/)

## License

[To be determined - check original exchange-core license]

## Contributing

Contributions are welcome! Please see [PLAN.md](PLAN.md) for development roadmap.

