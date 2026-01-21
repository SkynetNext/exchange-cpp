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

#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>

namespace exchange::core::common {

MatcherTradeEvent MatcherTradeEvent::Copy() const {
  MatcherTradeEvent evt;
  evt.eventType = this->eventType;
  evt.section = this->section;
  evt.activeOrderCompleted = this->activeOrderCompleted;
  evt.matchedOrderId = this->matchedOrderId;
  evt.matchedOrderUid = this->matchedOrderUid;
  evt.matchedOrderCompleted = this->matchedOrderCompleted;
  evt.price = this->price;
  evt.size = this->size;
  evt.bidderHoldPrice = this->bidderHoldPrice;
  return evt;
}

MatcherTradeEvent* MatcherTradeEvent::FindTail() {
  MatcherTradeEvent* tail = this;
  while (tail->nextEvent != nullptr) {
    tail = tail->nextEvent;
  }
  return tail;
}

int32_t MatcherTradeEvent::GetChainSize() const {
  const MatcherTradeEvent* tail = this;
  int32_t c = 1;
  while (tail->nextEvent != nullptr) {
    tail = tail->nextEvent;
    c++;
  }
  return c;
}

MatcherTradeEvent* MatcherTradeEvent::CreateEventChain(int32_t chainLength) {
  if (chainLength <= 0) {
    return nullptr;
  }
  MatcherTradeEvent* head = new MatcherTradeEvent();
  MatcherTradeEvent* prev = head;
  for (int32_t j = 1; j < chainLength; j++) {
    MatcherTradeEvent* nextEvent = new MatcherTradeEvent();
    prev->nextEvent = nextEvent;
    prev = nextEvent;
  }
  // Ensure last node's nextEvent is nullptr (should be already, but be
  // explicit)
  prev->nextEvent = nullptr;
  return head;
}

void MatcherTradeEvent::DeleteChain(MatcherTradeEvent* head) {
  while (head != nullptr) {
    MatcherTradeEvent* next = head->nextEvent;
    delete head;
    head = next;
  }
}

std::vector<MatcherTradeEvent*> MatcherTradeEvent::AsList(MatcherTradeEvent* head) {
  std::vector<MatcherTradeEvent*> list;
  while (head != nullptr) {
    list.push_back(head);
    head = head->nextEvent;
  }
  return list;
}

}  // namespace exchange::core::common
