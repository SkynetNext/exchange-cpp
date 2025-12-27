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

#include "../../WriteBytesMarshallable.h"
#include <cstdint>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace binary {

/**
 * BinaryDataCommand - interface for binary data commands
 * Matches Java: BinaryDataCommand extends WriteBytesMarshallable
 */
class BinaryDataCommand : public WriteBytesMarshallable {
public:
  virtual ~BinaryDataCommand() = default;

  /**
   * Get binary command type code
   */
  virtual int32_t GetBinaryCommandTypeCode() const = 0;
};

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
