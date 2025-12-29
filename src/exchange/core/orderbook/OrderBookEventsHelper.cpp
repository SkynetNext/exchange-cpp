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

#include <cstring>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <map>
#include <vector>

namespace exchange {
namespace core {
namespace orderbook {

OrderBookEventsHelper::OrderBookEventsHelper(EventFactory factory)
    : eventFactory_(std::move(factory)), eventsChainHead_(nullptr) {}

common::MatcherTradeEvent *
OrderBookEventsHelper::SendTradeEvent(const common::IOrder *matchingOrder,
                                      bool makerCompleted, bool takerCompleted,
                                      int64_t size, int64_t bidderHoldPrice) {

  common::MatcherTradeEvent *event = NewMatcherEvent();

  event->eventType = common::MatcherEventType::TRADE;
  event->section = 0;
  event->activeOrderCompleted = takerCompleted;
  event->matchedOrderId = matchingOrder->GetOrderId();
  event->matchedOrderUid = matchingOrder->GetUid();
  event->matchedOrderCompleted = makerCompleted;
  event->price = matchingOrder->GetPrice();
  event->size = size;
  event->bidderHoldPrice = bidderHoldPrice;

  return event;
}

common::MatcherTradeEvent *
OrderBookEventsHelper::SendReduceEvent(const common::IOrder *order,
                                       int64_t reduceSize, bool completed) {

  common::MatcherTradeEvent *event = NewMatcherEvent();
  event->eventType = common::MatcherEventType::REDUCE;
  event->section = 0;
  event->activeOrderCompleted = completed;
  event->matchedOrderId = 0;
  event->matchedOrderCompleted = false;
  event->price = order->GetPrice();
  event->size = reduceSize;
  event->bidderHoldPrice = order->GetReserveBidPrice();

  return event;
}

common::MatcherTradeEvent *
OrderBookEventsHelper::SendReduceEvent(int64_t price, int64_t reserveBidPrice,
                                       int64_t reduceSize, bool completed) {

  common::MatcherTradeEvent *event = NewMatcherEvent();
  event->eventType = common::MatcherEventType::REDUCE;
  event->section = 0;
  event->activeOrderCompleted = completed;
  event->matchedOrderId = 0;
  event->matchedOrderCompleted = false;
  event->price = price;
  event->size = reduceSize;
  event->bidderHoldPrice = reserveBidPrice;

  return event;
}

void OrderBookEventsHelper::AttachRejectEvent(common::cmd::OrderCommand *cmd,
                                              int64_t rejectedSize) {

  common::MatcherTradeEvent *event = NewMatcherEvent();

  event->eventType = common::MatcherEventType::REJECT;
  event->section = 0;
  event->activeOrderCompleted = true;
  event->matchedOrderId = 0;
  event->matchedOrderCompleted = false;
  event->price = cmd->price;
  event->size = rejectedSize;
  event->bidderHoldPrice = cmd->reserveBidPrice;

  // Insert event at the head of the chain
  event->nextEvent = cmd->matcherEvent;
  cmd->matcherEvent = event;
}

::exchange::core::common::MatcherTradeEvent *
OrderBookEventsHelper::NewMatcherEvent() {
  if constexpr (EVENTS_POOLING) {
    // Match Java: consume nodes from current chain sequentially
    if (eventsChainHead_ == nullptr) {
      // Chain exhausted, get new chain from SharedPool
      eventsChainHead_ = eventFactory_();
      if (eventsChainHead_ == nullptr) {
        // Fallback: create single event if factory returns nullptr
        return new common::MatcherTradeEvent();
      }
    }
    // Return current head and advance to next node
    // CRITICAL: Clear nextEvent to avoid chain corruption when this node is
    // used
    common::MatcherTradeEvent *res = eventsChainHead_;
    eventsChainHead_ = eventsChainHead_->nextEvent;
    res->nextEvent = nullptr; // Clear nextEvent to break the chain link
    return res;
  } else {
    // Not using pooling, create new event
    return eventFactory_();
  }
}

common::MatcherTradeEvent *OrderBookEventsHelper::CreateBinaryEventsChain(
    int64_t timestamp, int32_t section, const std::vector<uint8_t> &bytes) {
  // Match Java: createBinaryEventsChain()
  // Convert bytes to long array with padding=5 (each message carries 5 longs)
  const int padding = 5;
  std::vector<int64_t> dataArray =
      utils::SerializationUtils::BytesToLongArray(bytes, padding);

  common::MatcherTradeEvent *firstEvent = nullptr;
  common::MatcherTradeEvent *lastEvent = nullptr;

  // Create events from long array (5 longs per event)
  for (size_t i = 0; i < dataArray.size(); i += padding) {
    common::MatcherTradeEvent *event = NewMatcherEvent();

    event->eventType = common::MatcherEventType::BINARY_EVENT;
    event->section = section;
    event->matchedOrderId = dataArray[i];
    event->matchedOrderUid = dataArray[i + 1];
    event->price = dataArray[i + 2];
    event->size = dataArray[i + 3];
    event->bidderHoldPrice = dataArray[i + 4];
    event->nextEvent = nullptr;

    // Attach in direct order
    if (firstEvent == nullptr) {
      firstEvent = event;
    } else {
      lastEvent->nextEvent = event;
    }
    lastEvent = event;
  }

  return firstEvent;
}

OrderBookEventsHelper *OrderBookEventsHelper::NonPooledEventsHelper() {
  static OrderBookEventsHelper instance;
  return &instance;
}

std::map<int32_t, common::Wire>
OrderBookEventsHelper::DeserializeEvents(const common::cmd::OrderCommand *cmd) {
  // Match Java: deserializeEvents()
  // Group events by section
  std::map<int32_t, std::vector<common::MatcherTradeEvent *>> sections;

  // Process all events in the chain
  cmd->ProcessMatcherEvents([&sections](common::MatcherTradeEvent *evt) {
    if (evt->eventType == common::MatcherEventType::BINARY_EVENT) {
      sections[evt->section].push_back(evt);
    }
  });

  // Convert each section's events to Wire (matches Java NavigableMap<Integer,
  // Wire>)
  std::map<int32_t, common::Wire> result;

  for (const auto &[section, events] : sections) {
    // Skip empty events lists (matches Java behavior where empty sections are
    // not added)
    if (events.empty()) {
      continue;
    }

    // Build long array from events (5 longs per event)
    std::vector<int64_t> dataArray;
    dataArray.reserve(events.size() * 5);

    for (const auto *evt : events) {
      dataArray.push_back(evt->matchedOrderId);
      dataArray.push_back(evt->matchedOrderUid);
      dataArray.push_back(evt->price);
      dataArray.push_back(evt->size);
      dataArray.push_back(evt->bidderHoldPrice);
    }

    // Convert long array to Wire (matches Java SerializationUtils.longsToWire)
    result[section] = utils::SerializationUtils::LongsToWire(dataArray);
  }

  return result;
}

} // namespace orderbook
} // namespace core
} // namespace exchange
