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

namespace exchange {
namespace core {
namespace common {

/**
 * BytesOut - interface for writing binary data
 * Similar to Java's BytesOut from Chronicle Bytes
 */
class BytesOut {
public:
  virtual ~BytesOut() = default;

  /**
   * Write a byte
   */
  virtual BytesOut &WriteByte(int8_t value) = 0;

  /**
   * Write an int
   */
  virtual BytesOut &WriteInt(int32_t value) = 0;

  /**
   * Write a long
   */
  virtual BytesOut &WriteLong(int64_t value) = 0;

  /**
   * Write a boolean
   */
  virtual BytesOut &WriteBoolean(bool value) = 0;

  /**
   * Write bytes from buffer
   */
  virtual BytesOut &Write(const void *buffer, size_t length) = 0;

  /**
   * Get current write position
   */
  virtual int64_t WritePosition() const = 0;
};

} // namespace common
} // namespace core
} // namespace exchange
