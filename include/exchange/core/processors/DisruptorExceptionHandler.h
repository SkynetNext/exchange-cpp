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

#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <string>

namespace exchange {
namespace core {
namespace processors {

/**
 * DisruptorExceptionHandler - exception handler for Disruptor
 */
template <typename T> class DisruptorExceptionHandler {
public:
  using OnExceptionHandler =
      std::function<void(const std::exception &, int64_t)>;

  DisruptorExceptionHandler(const std::string &name,
                            OnExceptionHandler onException)
      : name_(name), onException_(std::move(onException)) {}

  void HandleEventException(const std::exception &ex, int64_t sequence,
                            T *event) {
    // Log and call handler
    onException_(ex, sequence);
  }

  void HandleOnStartException(const std::exception &ex) {
    // Log startup exception
  }

  void HandleOnShutdownException(const std::exception &ex) {
    // Log shutdown exception
  }

private:
  std::string name_;
  OnExceptionHandler onException_;
};

} // namespace processors
} // namespace core
} // namespace exchange
