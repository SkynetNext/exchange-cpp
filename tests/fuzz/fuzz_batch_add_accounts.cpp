/*
 * Copyright 2025 Justin Zhu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * LibFuzzer target: BatchAddAccountsCommand(BytesIn&).
 * Feed random bytes; catches OOB, overflow, and invalid length bugs.
 * Build with: -DBUILD_FUZZ=ON, Clang, -fsanitize=fuzzer,address,undefined
 */

#include <exchange/core/common/VectorBytesIn.h>
#include <exchange/core/common/api/binary/BatchAddAccountsCommand.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Cap input size to avoid OOM from huge payloads
static constexpr size_t kMaxInputSize = 65536;
// Cap first 4-byte length (map size) to avoid OOM from malicious length
static constexpr int32_t kMaxLengthField = 4096;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0 || size > kMaxInputSize) {
    return 0;
  }
  if (size >= sizeof(int32_t)) {
    int32_t length;
    std::memcpy(&length, data, sizeof(int32_t));
    if (length < 0 || length > kMaxLengthField) {
      return 0;
    }
  }
  std::vector<uint8_t> buf(size);
  std::memcpy(buf.data(), data, size);
  exchange::core::common::VectorBytesIn in(buf);
  try {
    exchange::core::common::api::binary::BatchAddAccountsCommand cmd(in);
  } catch (...) {
    // Expected for truncated or invalid input
  }
  return 0;
}
