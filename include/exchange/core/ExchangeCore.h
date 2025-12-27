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

#include "ExchangeApi.h"
#include "common/cmd/OrderCommand.h"
#include "common/config/ExchangeConfiguration.h"
#include <cstdint>
#include <functional>
#include <memory>

// Forward declarations
namespace exchange {
namespace core {
namespace processors {
namespace journaling {
class ISerializationProcessor;
}
} // namespace processors

/**
 * ExchangeCore - main exchange core class
 * Builds configuration and starts disruptor pipeline
 */
class ExchangeCore {
public:
  using ResultsConsumer =
      std::function<void(common::cmd::OrderCommand *, int64_t)>;

  ExchangeCore(
      ResultsConsumer resultsConsumer,
      const common::config::ExchangeConfiguration *exchangeConfiguration);

  ~ExchangeCore();

  /**
   * Startup - start the disruptor and replay journal
   */
  void Startup();

  /**
   * Shutdown - stop the disruptor
   */
  void Shutdown(int64_t timeoutMs = -1);

  /**
   * Get ExchangeApi instance
   */
  IExchangeApi *GetApi();

  // Internal implementation interface (must be public for template class access
  // in .cpp)
  struct IImpl;

private:
  std::unique_ptr<IImpl> impl_;

  const common::config::ExchangeConfiguration *exchangeConfiguration_;
};

} // namespace core
} // namespace exchange
