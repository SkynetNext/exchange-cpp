/*
 * Copyright 2019 Maksim Zheravin
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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/api/binary/BinaryDataCommandFactory.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace binary {

// Use Meyers Singleton
BinaryDataCommandFactory &BinaryDataCommandFactory::getInstance() {
  static BinaryDataCommandFactory instance;
  return instance;
}

// BinaryCommandTypeRegistrar constructor implementation
detail::BinaryCommandTypeRegistrar::BinaryCommandTypeRegistrar(
    BinaryCommandType type, BinaryDataCommandConstructor constructor) {
  BinaryDataCommandFactory::getInstance().registerCommandType(type,
                                                              constructor);
}

// Register binary command type
void BinaryDataCommandFactory::registerCommandType(
    BinaryCommandType type, BinaryDataCommandConstructor constructor) {
  size_t index = static_cast<size_t>(BinaryCommandTypeToCode(type));

  // Resize if needed (lazy initialization)
  if (constructors_.size() <= index) {
    constructors_.resize(index + 1);
  }

  constructors_[index] = constructor;
}

// Get constructor for specific type
BinaryDataCommandConstructor
BinaryDataCommandFactory::getConstructor(BinaryCommandType type) {
  size_t index = static_cast<size_t>(BinaryCommandTypeToCode(type));

  if (index >= constructors_.size()) {
    return nullptr;
  }

  return constructors_[index];
}

// Create binary command from bytes
std::unique_ptr<BinaryDataCommand>
BinaryDataCommandFactory::createCommand(BinaryCommandType type,
                                        BytesIn &bytes) {
  auto constructor = getConstructor(type);
  if (!constructor) {
    throw std::runtime_error(
        "No constructor registered for BinaryCommandType: " +
        std::to_string(BinaryCommandTypeToCode(type)));
  }
  return constructor(bytes);
}

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
