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

#include <functional>
#include <memory>
#include <vector>
#include "BinaryCommandType.h"
#include "BinaryDataCommand.h"

namespace exchange {
namespace core {
namespace common {
class BytesIn;

namespace api {
namespace binary {

using BinaryDataCommandConstructor = std::function<std::unique_ptr<BinaryDataCommand>(BytesIn&)>;

class BinaryDataCommandFactory {
private:
  std::vector<BinaryDataCommandConstructor> constructors_;

  BinaryDataCommandFactory() = default;

public:
  BinaryDataCommandFactory(BinaryDataCommandFactory const&) = delete;
  BinaryDataCommandFactory& operator=(BinaryDataCommandFactory const&) = delete;

  // Use Meyers Singleton
  static BinaryDataCommandFactory& getInstance();

  // Register binary command type
  void registerCommandType(BinaryCommandType type, BinaryDataCommandConstructor constructor);

  // Get constructor for specific type
  BinaryDataCommandConstructor getConstructor(BinaryCommandType type);

  // Create binary command from bytes
  std::unique_ptr<BinaryDataCommand> createCommand(BinaryCommandType type, BytesIn& bytes);
};

namespace detail {
struct BinaryCommandTypeRegistrar {
  BinaryCommandTypeRegistrar(BinaryCommandType type, BinaryDataCommandConstructor constructor);
};
}  // namespace detail

// Register binary command type macro
#define REGISTER_BINARY_COMMAND_TYPE(CommandType, EnumType)              \
  static detail::BinaryCommandTypeRegistrar CommandType##_registrar(     \
    EnumType, [](BytesIn& bytes) -> std::unique_ptr<BinaryDataCommand> { \
      return std::make_unique<CommandType>(bytes);                       \
    })

}  // namespace binary
}  // namespace api
}  // namespace common
}  // namespace core
}  // namespace exchange
