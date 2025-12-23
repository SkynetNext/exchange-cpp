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

#include <cstring>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/VectorBytesIn.h>
#include <exchange/core/common/WriteBytesMarshallable.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/processors/BinaryCommandsProcessor.h>
#include <exchange/core/utils/HashingUtils.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <stdexcept>
#include <vector>

namespace exchange {
namespace core {
namespace processors {

// TransferRecord - internal class for storing binary frame data
class TransferRecord : public common::WriteBytesMarshallable {
public:
  std::vector<int64_t> dataArray;
  int wordsTransferred;

  TransferRecord(int expectedLength)
      : wordsTransferred(0), dataArray(expectedLength, 0) {}

  /**
   * Constructor from BytesIn (deserialization)
   */
  TransferRecord(common::BytesIn &bytes) {
    wordsTransferred = bytes.ReadInt();
    dataArray = utils::SerializationUtils::ReadLongArray(bytes);
  }

  void AddWord(int64_t word) {
    if (wordsTransferred >= static_cast<int>(dataArray.size())) {
      // Resize if needed
      dataArray.resize(dataArray.size() * 2);
    }
    dataArray[wordsTransferred++] = word;
  }

  void WriteMarshallable(common::BytesOut &bytes) override {
    bytes.WriteInt(wordsTransferred);
    utils::SerializationUtils::MarshallLongArray(dataArray, bytes);
  }
};

// Helper function to calculate required long array size
static int RequiredLongArraySize(int bytesLength, int longsPerMessage) {
  // Each message carries longsPerMessage longs
  // We need to calculate how many longs are needed for bytesLength bytes
  int longsNeeded = (bytesLength + 7) / 8; // Round up to nearest long
  // Round up to nearest multiple of longsPerMessage
  return ((longsNeeded + longsPerMessage - 1) / longsPerMessage) *
         longsPerMessage;
}

BinaryCommandsProcessor::BinaryCommandsProcessor(
    CompleteMessagesHandler completeMessagesHandler,
    common::api::reports::ReportQueriesHandler *reportQueriesHandler,
    SharedPool *sharedPool,
    const common::config::ReportsQueriesConfiguration *queriesConfiguration,
    int32_t section)
    : completeMessagesHandler_(std::move(completeMessagesHandler)),
      reportQueriesHandler_(reportQueriesHandler),
      queriesConfiguration_(queriesConfiguration), section_(section) {
  if (sharedPool != nullptr) {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>(
        [sharedPool]() { return sharedPool->GetChain(); });
  } else {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>();
  }
}

BinaryCommandsProcessor::BinaryCommandsProcessor(
    CompleteMessagesHandler completeMessagesHandler,
    common::api::reports::ReportQueriesHandler *reportQueriesHandler,
    SharedPool *sharedPool,
    const common::config::ReportsQueriesConfiguration *queriesConfiguration,
    common::BytesIn *bytesIn, int32_t section)
    : completeMessagesHandler_(std::move(completeMessagesHandler)),
      reportQueriesHandler_(reportQueriesHandler),
      queriesConfiguration_(queriesConfiguration), section_(section) {
  if (bytesIn == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }

  // Read incomingData (long -> TransferRecord*)
  int length = bytesIn->ReadInt();
  for (int i = 0; i < length; i++) {
    int64_t transactionId = bytesIn->ReadLong();
    TransferRecord *record = new TransferRecord(*bytesIn);
    incomingData_[transactionId] = record;
  }

  if (sharedPool != nullptr) {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>(
        [sharedPool]() { return sharedPool->GetChain(); });
  } else {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>();
  }
}

common::cmd::CommandResultCode
BinaryCommandsProcessor::AcceptBinaryFrame(common::cmd::OrderCommand *cmd) {
  const int32_t transferId = cmd->userCookie;

  // Get or create transfer record
  auto it = incomingData_.find(transferId);
  TransferRecord *record = nullptr;

  if (it == incomingData_.end()) {
    // First frame - extract expected length from orderId
    // Format: (bytesLength << 32) | other_data
    const int bytesLength = static_cast<int>((cmd->orderId >> 32) & 0x7FFFFFFF);
    const int longsPerMessage = 5; // Each OrderCommand carries 5 longs
    const int longArraySize =
        RequiredLongArraySize(bytesLength, longsPerMessage);
    record = new TransferRecord(longArraySize);
    incomingData_[transferId] = record;
  } else {
    record = static_cast<TransferRecord *>(it->second);
  }

  // Add words from this frame
  record->AddWord(cmd->orderId);
  record->AddWord(cmd->price);
  record->AddWord(cmd->reserveBidPrice);
  record->AddWord(cmd->size);
  record->AddWord(cmd->uid);

  // Check if this is the last frame (symbol == -1 indicates last frame)
  if (cmd->symbol == -1) {
    // All frames received - process complete message
    incomingData_.erase(transferId);

    // Decompress with LZ4 and create BytesIn
    std::vector<uint8_t> decompressedBytes =
        utils::SerializationUtils::LongsLz4ToBytes(record->dataArray,
                                                   record->wordsTransferred);
    common::VectorBytesIn bytesIn(decompressedBytes);

    if (cmd->command == common::cmd::OrderCommandType::BINARY_DATA_QUERY) {
      // Handle report query
      // Read class code and deserialize query
      int32_t classCode = bytesIn.ReadInt();
      // TODO: Use queriesConfiguration to get constructor and deserialize
      // For now, this is a placeholder
      // if (reportQueriesHandler_) {
      //   auto query = DeserializeQuery(bytesIn, classCode);
      //   if (query) {
      //     auto result = reportQueriesHandler_->HandleReport(query.get());
      //     if (result) {
      //       // Create binary events chain and append to cmd
      //       // Implementation depends on OrderBookEventsHelper
      //     }
      //   }
      // }
    } else if (cmd->command ==
               common::cmd::OrderCommandType::BINARY_DATA_COMMAND) {
      // Handle binary data command
      // Read class code and deserialize command
      int32_t classCode = bytesIn.ReadInt();
      // TODO: Use queriesConfiguration to get constructor and deserialize
      // For now, this is a placeholder
      // auto binaryCommand = DeserializeBinaryCommand(bytesIn, classCode);
      // if (completeMessagesHandler_) {
      //   completeMessagesHandler_(binaryCommand.get());
      // }
    } else {
      throw std::runtime_error("Invalid binary command type");
    }

    delete record;
    return common::cmd::CommandResultCode::SUCCESS;
  } else {
    // More frames expected
    return common::cmd::CommandResultCode::ACCEPTED;
  }
}

void BinaryCommandsProcessor::Reset() {
  // Clean up all transfer records
  for (auto &pair : incomingData_) {
    delete static_cast<TransferRecord *>(pair.second);
  }
  incomingData_.clear();
}

int32_t BinaryCommandsProcessor::GetStateHash() const {
  // Calculate hash based on incoming data
  // For now, use a simple hash of transfer IDs
  std::size_t hash = 0;
  for (const auto &pair : incomingData_) {
    hash ^= std::hash<int64_t>{}(pair.first) << 1;
  }
  return static_cast<int32_t>(hash);
}

void BinaryCommandsProcessor::WriteMarshallable(common::BytesOut &bytes) {
  // Write incomingData (transactionId -> TransferRecord)
  bytes.WriteInt(static_cast<int32_t>(incomingData_.size()));
  for (const auto &pair : incomingData_) {
    bytes.WriteLong(pair.first);
    TransferRecord *record = static_cast<TransferRecord *>(pair.second);
    if (record != nullptr) {
      record->WriteMarshallable(bytes);
    }
  }
}

} // namespace processors
} // namespace core
} // namespace exchange
