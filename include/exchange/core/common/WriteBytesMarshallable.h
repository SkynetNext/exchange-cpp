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

#include "BytesOut.h"

namespace exchange {
namespace core {
namespace common {

/**
 * WriteBytesMarshallable - interface for objects that can be serialized
 * Similar to Java's WriteBytesMarshallable from Chronicle Bytes
 */
class WriteBytesMarshallable {
public:
  virtual ~WriteBytesMarshallable() = default;

  /**
   * Write object to bytes
   * Const method - serialization should not modify object state
   */
  virtual void WriteMarshallable(BytesOut& bytes) const = 0;
};

}  // namespace common
}  // namespace core
}  // namespace exchange
