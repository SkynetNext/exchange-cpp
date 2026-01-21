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

#pragma once

#include <cstddef>
#include <cstdint>

namespace exchange::core::common {

/**
 * BytesIn - interface for reading binary data
 * Similar to Java's BytesIn from Chronicle Bytes
 */
class BytesIn {
public:
  virtual ~BytesIn() = default;

  /**
   * Read a byte
   */
  virtual int8_t ReadByte() = 0;

  /**
   * Read an int
   */
  virtual int32_t ReadInt() = 0;

  /**
   * Read a long
   */
  virtual int64_t ReadLong() = 0;

  /**
   * Read a boolean
   */
  virtual bool ReadBoolean() = 0;

  /**
   * Read remaining bytes
   */
  virtual int64_t ReadRemaining() const = 0;

  /**
   * Read bytes into buffer
   */
  virtual void Read(void* buffer, size_t length) = 0;
};

}  // namespace exchange::core::common
