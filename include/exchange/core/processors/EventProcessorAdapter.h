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

#include <disruptor/EventProcessor.h>
#include <disruptor/EventHandlerIdentity.h>
#include <disruptor/Sequence.h>

namespace exchange {
namespace core {
namespace processors {

/**
 * EventProcessorAdapter - adapter to make processors compatible with
 * disruptor::EventProcessor interface and EventHandlerIdentity
 */
class EventProcessorAdapter : public disruptor::EventProcessor,
                              public disruptor::EventHandlerIdentity {
public:
  EventProcessorAdapter(disruptor::Sequence *sequence,
                        std::function<void()> runFunc,
                        std::function<void()> haltFunc,
                        std::function<bool()> isRunningFunc)
      : sequence_(sequence), runFunc_(runFunc), haltFunc_(haltFunc),
        isRunningFunc_(isRunningFunc) {}

  void run() override {
    if (runFunc_) {
      runFunc_();
    }
  }

  disruptor::Sequence &getSequence() override { return *sequence_; }

  void halt() override {
    if (haltFunc_) {
      haltFunc_();
    }
  }

  bool isRunning() override {
    if (isRunningFunc_) {
      return isRunningFunc_();
    }
    return false;
  }

private:
  disruptor::Sequence *sequence_;
  std::function<void()> runFunc_;
  std::function<void()> haltFunc_;
  std::function<bool()> isRunningFunc_;
};

} // namespace processors
} // namespace core
} // namespace exchange
