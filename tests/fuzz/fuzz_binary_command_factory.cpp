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
 * LibFuzzer target: BinaryDataCommandFactory::createCommand(type, bytes).
 * First 4 bytes = type (1002 ADD_ACCOUNTS, 1003 ADD_SYMBOLS), rest = payload.
 * Build with: -DBUILD_FUZZ=ON, Clang, -fsanitize=fuzzer,address,undefined
 */

#include <exchange/core/common/VectorBytesIn.h>
#include <exchange/core/common/api/binary/BinaryCommandType.h>
#include <exchange/core/common/api/binary/BinaryDataCommandFactory.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Cap input size to avoid OOM from huge payloads
static constexpr size_t kMaxInputSize = 65536;
// Cap first 4-byte length in payload (map/array size) to avoid OOM
static constexpr int32_t kMaxLengthField = 4096;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < sizeof(int32_t) || size > kMaxInputSize) {
    return 0;
  }
  std::vector<uint8_t> buf(size);
  std::memcpy(buf.data(), data, size);
  int32_t type_code;
  std::memcpy(&type_code, buf.data(), sizeof(int32_t));
  const size_t payload_offset = sizeof(int32_t);
  std::vector<uint8_t> payload(buf.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                               buf.end());
  if (payload.size() >= sizeof(int32_t)) {
    int32_t length;
    std::memcpy(&length, payload.data(), sizeof(int32_t));
    if (length < 0 || length > kMaxLengthField) {
      return 0;
    }
  }
  exchange::core::common::VectorBytesIn in(payload);
  try {
    auto type = exchange::core::common::api::binary::BinaryCommandTypeFromCode(type_code);
    auto cmd =
      exchange::core::common::api::binary::BinaryDataCommandFactory::getInstance().createCommand(
        type, in);
    (void)cmd;
  } catch (...) {
    // Expected for unknown type or invalid payload
  }
  return 0;
}
