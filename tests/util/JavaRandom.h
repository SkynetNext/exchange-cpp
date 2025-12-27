/*
 * JavaRandom - C++ implementation of Java's Random class
 * Matches Java java.util.Random behavior exactly
 */

#pragma once

#include <cstdint>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * JavaRandom - C++ implementation matching Java's Random class
 * Uses the same linear congruential generator algorithm
 */
class JavaRandom {
public:
  explicit JavaRandom(int64_t seed) : seed_(initialScramble(seed)) {}

  // Generate next random value (internal state update)
  // Match Java: returns unsigned value in range [0, 2^bits)
  // Java uses unsigned right shift (>>>), so we need to ensure unsigned
  // behavior
  int32_t next(int bits) {
    seed_ = (seed_ * 0x5DEECE66DL + 0xBL) & ((1LL << 48) - 1);
    // Java uses unsigned right shift (>>>), so cast to uint64_t first
    // This ensures the result is always non-negative for bits <= 31
    return static_cast<int32_t>(static_cast<uint64_t>(seed_) >> (48 - bits));
  }

  // Generate next int (32 bits)
  int32_t nextInt() { return next(32); }

  // Generate next int in range [0, n)
  int32_t nextInt(int32_t n) {
    if (n <= 0) {
      return 0; // Match Java behavior
    }

    if ((n & -n) == n) { // n is a power of 2
      return static_cast<int32_t>((n * static_cast<int64_t>(next(31))) >> 31);
    }

    int32_t bits, val;
    do {
      bits = next(31); // Returns unsigned 31-bit value
      val = bits % n;
    } while (bits - val + (n - 1) < 0);
    return val;
  }

  // Generate next double in range [0.0, 1.0)
  double nextDouble() {
    return (((static_cast<int64_t>(next(26)) << 27) + next(27)) /
            static_cast<double>(1LL << 53));
  }

  // Generate next long (64 bits)
  // Match Java: return ((long)next(32) << 32) + next(32);
  // Note: next(32) returns value in range [0, 2^32), but stored as signed int
  // Java casts to long, which sign-extends, but we want unsigned behavior
  // So we mask to get unsigned 32-bit value
  int64_t nextLong() {
    uint32_t high = static_cast<uint32_t>(next(32));
    uint32_t low = static_cast<uint32_t>(next(32));
    return (static_cast<int64_t>(high) << 32) + static_cast<int64_t>(low);
  }

private:
  int64_t seed_;

  // Initial scramble function matching Java Random constructor
  static int64_t initialScramble(int64_t seed) {
    return (seed ^ 0x5DEECE66DL) & ((1LL << 48) - 1);
  }
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
