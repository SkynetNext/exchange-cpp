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

#include <chrono>
#include <disruptor/AlertException.h>
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/YieldingWaitStrategy.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/processors/GroupingProcessor.h>
#include <exchange/core/processors/SharedPool.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/utils/LatencyBreakdown.h>
#include <exchange/core/utils/Logger.h>

namespace exchange {
namespace core {
namespace processors {

template <typename WaitStrategyT>
GroupingProcessor<WaitStrategyT>::GroupingProcessor(
    disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
        *ringBuffer,
    disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
        *sequenceBarrier,
    const common::config::PerformanceConfiguration *perfCfg,
    common::CoreWaitStrategy coreWaitStrategy, SharedPool *sharedPool)
    : running_(IDLE), ringBuffer_(ringBuffer),
      sequenceBarrier_(sequenceBarrier),
      waitSpinningHelper_(
          new WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT>(
              ringBuffer, sequenceBarrier, GROUP_SPIN_LIMIT, coreWaitStrategy,
              "GroupingProcessor")),
      sequence_(disruptor::Sequence::INITIAL_VALUE), sharedPool_(sharedPool),
      msgsInGroupLimit_(perfCfg->msgsInGroupLimit),
      maxGroupDurationNs_(perfCfg->maxGroupDurationNs) {
  if (msgsInGroupLimit_ > perfCfg->ringBufferSize / 4) {
    throw std::invalid_argument(
        "msgsInGroupLimit should be less than quarter ringBufferSize");
  }
}

template <typename WaitStrategyT>
disruptor::Sequence &GroupingProcessor<WaitStrategyT>::getSequence() {
  return sequence_;
}

template <typename WaitStrategyT>
void GroupingProcessor<WaitStrategyT>::halt() {
  running_.store(HALTED);
  // Match Java: sequenceBarrier.alert();
  sequenceBarrier_->alert();
}

template <typename WaitStrategyT>
bool GroupingProcessor<WaitStrategyT>::isRunning() {
  return running_.load() != IDLE;
}

template <typename WaitStrategyT> void GroupingProcessor<WaitStrategyT>::run() {
  if (running_.compare_exchange_strong(const_cast<int32_t &>(IDLE), RUNNING)) {
    // Match Java: sequenceBarrier.clearAlert();
    sequenceBarrier_->clearAlert();

    try {
      if (running_.load() == RUNNING) {
        ProcessEvents();
      }
    } catch (...) {
      // Handle exception
    }
    running_.store(IDLE);
  } else {
    if (running_.load() == RUNNING) {
      throw std::runtime_error("Thread is already running");
    }
  }
}

template <typename WaitStrategyT>
void GroupingProcessor<WaitStrategyT>::ProcessEvents() {
  int64_t nextSequence = sequence_.get() + 1L;

  int64_t groupCounter = 0;
  int64_t msgsInGroup = 0;

  int64_t groupLastNs = 0;
  int64_t l2dataLastNs = 0;
  bool triggerL2DataRequest = false;

  bool groupingEnabled = true;

  // Use OrderBookEventsHelper::EVENTS_POOLING to match Java behavior
  constexpr bool EVENTS_POOLING =
      orderbook::OrderBookEventsHelper::EVENTS_POOLING;

  // Thread-local event chain accumulation (matches Java implementation)
  // Best practice: Use counter-based batching instead of traversing chains
  // This avoids O(n) traversal in hot path and reduces overhead
  common::MatcherTradeEvent *tradeEventHead = nullptr;
  common::MatcherTradeEvent *tradeEventTail = nullptr;
  int32_t tradeEventCounter = 0;
  int32_t tradeEventChainLengthTarget = 0;
  if constexpr (EVENTS_POOLING) {
    if (sharedPool_ != nullptr) {
      tradeEventChainLengthTarget = sharedPool_->GetChainLength();
    }
  }

  // Performance optimization: Use thread_local epoch to reduce time conversion
  // overhead This avoids calling time_since_epoch() repeatedly, using relative
  // time instead
  static thread_local auto epoch = std::chrono::steady_clock::now();
  static thread_local int64_t epoch_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          epoch.time_since_epoch())
          .count();

  while (true) {
    try {
      // should spin and also check another barrier
      int64_t availableSequence = waitSpinningHelper_->TryWaitFor(nextSequence);

      if (nextSequence <= availableSequence) {
        while (nextSequence <= availableSequence) {
          common::cmd::OrderCommand *cmd = &ringBuffer_->get(nextSequence);
          int64_t currentSeq = nextSequence;
          nextSequence++;

#ifdef ENABLE_LATENCY_BREAKDOWN
          utils::LatencyBreakdown::Record(
              cmd, currentSeq, utils::LatencyBreakdown::Stage::GROUPING_START);
#endif

          if (cmd->command == common::cmd::OrderCommandType::GROUPING_CONTROL) {
            groupingEnabled = (cmd->orderId == 1);
            cmd->resultCode = common::cmd::CommandResultCode::SUCCESS;
          }

          if (!groupingEnabled) {
            cmd->matcherEvent = nullptr;
            cmd->marketData = nullptr;
            continue;
          }

          // some commands should trigger R2 stage
          if (cmd->command == common::cmd::OrderCommandType::RESET ||
              cmd->command ==
                  common::cmd::OrderCommandType::PERSIST_STATE_MATCHING ||
              cmd->command == common::cmd::OrderCommandType::GROUPING_CONTROL) {
            // Flush any remaining event chains before switching group
            if constexpr (EVENTS_POOLING) {
              if (tradeEventHead != nullptr) {
                sharedPool_->PutChain(tradeEventHead);
                tradeEventCounter = 0;
                tradeEventTail = nullptr;
                tradeEventHead = nullptr;
              }
            }
            groupCounter++;
            msgsInGroup = 0;
          }

          // report/binary commands also should trigger R2 stage
          if ((cmd->command ==
                   common::cmd::OrderCommandType::BINARY_DATA_COMMAND ||
               cmd->command ==
                   common::cmd::OrderCommandType::BINARY_DATA_QUERY) &&
              cmd->symbol == -1) {
            // Flush any remaining event chains before switching group
            if constexpr (EVENTS_POOLING) {
              if (tradeEventHead != nullptr) {
                sharedPool_->PutChain(tradeEventHead);
                tradeEventCounter = 0;
                tradeEventTail = nullptr;
                tradeEventHead = nullptr;
              }
            }
            groupCounter++;
            msgsInGroup = 0;
          }

          cmd->eventsGroup = groupCounter;

          if (triggerL2DataRequest) {
            triggerL2DataRequest = false;
            cmd->serviceFlags = 1;
          } else {
            cmd->serviceFlags = 0;
          }

          // cleaning attached events (matches Java implementation)
          // Best practice: Counter-based batching avoids O(n) chain traversal
          if (cmd->matcherEvent != nullptr) {
            if constexpr (EVENTS_POOLING) {
              // Thread-local accumulation: append to local chain
              if (tradeEventTail == nullptr) {
                tradeEventHead = cmd->matcherEvent;
              } else {
                tradeEventTail->nextEvent = cmd->matcherEvent;
              }
              tradeEventTail = cmd->matcherEvent;
              tradeEventCounter++;

              // Find tail and update counter (single traversal)
              while (tradeEventTail->nextEvent != nullptr) {
                tradeEventTail = tradeEventTail->nextEvent;
                tradeEventCounter++;
              }

              // Batch return when target length reached (avoids frequent
              // PutChain calls)
              if (tradeEventCounter >= tradeEventChainLengthTarget &&
                  tradeEventChainLengthTarget > 0) {
                sharedPool_->PutChain(tradeEventHead);
                tradeEventCounter = 0;
                tradeEventTail = nullptr;
                tradeEventHead = nullptr;
              }
            } else {
              // Fast path: direct deletion when pooling is disabled
              common::MatcherTradeEvent::DeleteChain(cmd->matcherEvent);
            }
            cmd->matcherEvent = nullptr;
          }
          cmd->marketData = nullptr;

          msgsInGroup++;

          // switch group after each N messages
          if (msgsInGroup >= msgsInGroupLimit_ &&
              cmd->command !=
                  common::cmd::OrderCommandType::PERSIST_STATE_RISK) {
            // Flush any remaining event chains before switching group
            if constexpr (EVENTS_POOLING) {
              if (tradeEventHead != nullptr) {
                sharedPool_->PutChain(tradeEventHead);
                tradeEventCounter = 0;
                tradeEventTail = nullptr;
                tradeEventHead = nullptr;
              }
            }
            groupCounter++;
            msgsInGroup = 0;
          }

#ifdef ENABLE_LATENCY_BREAKDOWN
          utils::LatencyBreakdown::Record(
              cmd, currentSeq, utils::LatencyBreakdown::Stage::GROUPING_END);
#endif
        }
        sequence_.set(availableSequence);
        waitSpinningHelper_->SignalAllWhenBlocking();
        // Performance optimization: Use relative time calculation to reduce
        // conversion overhead
        auto now = std::chrono::steady_clock::now();
        int64_t t =
            epoch_ns +
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - epoch)
                .count();
        groupLastNs = t + maxGroupDurationNs_;

      } else {
        // Performance optimization: Use relative time calculation to reduce
        // conversion overhead This is called frequently when no messages are
        // available, so optimization is important for maintaining 10ms L2 data
        // publishing precision
        auto now = std::chrono::steady_clock::now();
        int64_t t =
            epoch_ns +
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - epoch)
                .count();
        if (msgsInGroup > 0 && t > groupLastNs) {
          // switch group after T microseconds elapsed
          // Flush any remaining event chains before switching group
          if constexpr (EVENTS_POOLING) {
            if (tradeEventHead != nullptr) {
              sharedPool_->PutChain(tradeEventHead);
              tradeEventCounter = 0;
              tradeEventTail = nullptr;
              tradeEventHead = nullptr;
            }
          }
          groupCounter++;
          msgsInGroup = 0;
        }

        if (t > l2dataLastNs) {
          l2dataLastNs =
              t + L2_PUBLISH_INTERVAL_NS; // trigger L2 data every 10ms
          triggerL2DataRequest = true;
        }
      }

    } catch (const disruptor::AlertException &ex) {
      if (running_.load() != RUNNING) {
        // Flush any remaining event chains before halting
        if constexpr (EVENTS_POOLING) {
          if (tradeEventHead != nullptr) {
            sharedPool_->PutChain(tradeEventHead);
            tradeEventCounter = 0;
            tradeEventTail = nullptr;
            tradeEventHead = nullptr;
          }
        }
        break;
      }
    } catch (...) {
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
    }
  }
}

// Explicit template instantiations
template class GroupingProcessor<disruptor::BusySpinWaitStrategy>;
template class GroupingProcessor<disruptor::YieldingWaitStrategy>;
template class GroupingProcessor<disruptor::BlockingWaitStrategy>;

} // namespace processors
} // namespace core
} // namespace exchange
