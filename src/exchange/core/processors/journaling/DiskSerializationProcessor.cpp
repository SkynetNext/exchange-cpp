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
#include <ctime>
#include <exchange/core/ExchangeApi.h>
#include <exchange/core/common/VectorBytesOut.h>
#include <exchange/core/common/WriteBytesMarshallable.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiBinaryDataCommand.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/api/ApiReset.h>
#include <exchange/core/common/api/ApiResumeUser.h>
#include <exchange/core/common/api/ApiSuspendUser.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/processors/journaling/DiskSerializationProcessor.h>
#include <exchange/core/processors/journaling/JournalDescriptor.h>
#include <exchange/core/processors/journaling/SnapshotDescriptor.h>
#include <exchange/core/utils/Logger.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <lz4.h>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

// FileBytesOut - BytesOut implementation for file writing
class FileBytesOut : public common::BytesOut {
private:
  std::ofstream &file_;
  int64_t position_;

public:
  explicit FileBytesOut(std::ofstream &file) : file_(file), position_(0) {}

  BytesOut &WriteByte(int8_t value) override {
    file_.write(reinterpret_cast<const char *>(&value), 1);
    position_++;
    return *this;
  }

  BytesOut &WriteInt(int32_t value) override {
    file_.write(reinterpret_cast<const char *>(&value), sizeof(int32_t));
    position_ += sizeof(int32_t);
    return *this;
  }

  BytesOut &WriteLong(int64_t value) override {
    file_.write(reinterpret_cast<const char *>(&value), sizeof(int64_t));
    position_ += sizeof(int64_t);
    return *this;
  }

  BytesOut &WriteBoolean(bool value) override {
    int8_t byte = value ? 1 : 0;
    file_.write(reinterpret_cast<const char *>(&byte), 1);
    position_++;
    return *this;
  }

  BytesOut &Write(const void *buffer, size_t length) override {
    file_.write(reinterpret_cast<const char *>(buffer), length);
    position_ += static_cast<int64_t>(length);
    return *this;
  }

  int64_t WritePosition() const override { return position_; }
};

DiskSerializationProcessor::DiskSerializationProcessor(
    const common::config::ExchangeConfiguration *exchangeConfig,
    const DiskSerializationProcessorConfiguration *diskConfig)
    : exchangeId_(exchangeConfig->initStateCfg.exchangeId),
      folder_(diskConfig->storageFolder),
      baseSeq_(exchangeConfig->initStateCfg.snapshotBaseSeq),
      journalBufferFlushTrigger_(diskConfig->journalBufferFlushTrigger),
      journalFileMaxSize_(diskConfig->journalFileMaxSize),
      journalBatchCompressThreshold_(diskConfig->journalBatchCompressThreshold),
      lastJournalDescriptor_(nullptr),
      baseSnapshotId_(exchangeConfig->initStateCfg.snapshotId),
      enableJournalAfterSeq_(-1), filesCounter_(0), writtenBytes_(0),
      lastWrittenSeq_(-1),
      journalWriteBuffer_(diskConfig->journalBufferSize, 0),
      lz4WriteBuffer_(LZ4_compressBound(diskConfig->journalBufferSize), 0),
      journalWriteBufferPos_(0) {
  // Create folder if it doesn't exist
  std::filesystem::create_directories(folder_);

  // Initialize lastSnapshotDescriptor as empty snapshot (matches Java version)
  const auto &perfCfg = exchangeConfig->performanceCfg;
  lastSnapshotDescriptor_ = SnapshotDescriptor::CreateEmpty(
      perfCfg.matchingEnginesNum, perfCfg.riskEnginesNum);

  // Initialize snapshotsIndex with empty snapshot
  snapshotsIndex_[0] = lastSnapshotDescriptor_;
}

bool DiskSerializationProcessor::StoreData(int64_t snapshotId, int64_t seq,
                                           int64_t timestampNs,
                                           SerializedModuleType type,
                                           int32_t instanceId, void *obj) {
  const std::string path = GetSnapshotPath(snapshotId, type, instanceId);

  LOG_DEBUG("Writing state into file {} ...", path);

  try {
    // Create directory if needed
    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());

    // Open file with CREATE_NEW (fail if exists)
    std::ofstream file(path, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
      LOG_ERROR("Can not open snapshot file: {}", path);
      return false;
    }

    // Serialize to memory first
    std::vector<uint8_t> serializedData;
    common::VectorBytesOut vectorOut(serializedData);
    const auto *marshallable =
        static_cast<const common::WriteBytesMarshallable *>(obj);
    if (marshallable != nullptr) {
      marshallable->WriteMarshallable(vectorOut);
    }

    // Compress with LZ4
    const int originalSize = static_cast<int>(serializedData.size());
    const int maxCompressedSize = LZ4_compressBound(originalSize);
    std::vector<char> compressedData(maxCompressedSize);
    const int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char *>(serializedData.data()),
        compressedData.data(), originalSize, maxCompressedSize);

    if (compressedSize <= 0) {
      LOG_ERROR("LZ4 compression failed for snapshot file: {}", path);
      return false;
    }

    // Write format: [originalSize (4 bytes)] [compressedSize (4 bytes)]
    // [compressed data]
    file.write(reinterpret_cast<const char *>(&originalSize), sizeof(int32_t));
    file.write(reinterpret_cast<const char *>(&compressedSize),
               sizeof(int32_t));
    file.write(compressedData.data(), compressedSize);

    file.close();

    LOG_DEBUG("done serializing, flushing {} ...", path);
    LOG_DEBUG("completed {}", path);

    // Update snapshot descriptor path if this is the first instance written
    // (matches Java: path is set when snapshot is created)
    if (snapshotsIndex_.find(snapshotId) != snapshotsIndex_.end()) {
      auto *descriptor = snapshotsIndex_[snapshotId];
      if (descriptor && descriptor->path.empty()) {
        // Set path for the snapshot descriptor (use first instance path as
        // base)
        if (instanceId == 0) {
          descriptor->path = path;
        }
      }
    }

    // Write to main log file (synchronized) - matches Java format
    {
      static std::mutex mainLogMutex;
      std::lock_guard<std::mutex> lock(mainLogMutex);
      const std::string mainLogPath = folder_ + "/" + exchangeId_ + ".eca";
      std::ofstream mainLog(mainLogPath, std::ios::app);
      if (mainLog.is_open()) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        mainLog << now << " seq=" << seq << " timestampNs=" << timestampNs
                << " snapshotId=" << snapshotId << " type="
                << (type == SerializedModuleType::RISK_ENGINE ? "RE" : "ME")
                << " instance=" << instanceId << "\n";
        mainLog.close();
      }
    }

    return true;
  } catch (const std::exception &ex) {
    LOG_ERROR("Can not write snapshot file: {} - {}", path, ex.what());
    return false;
  }
}

std::string DiskSerializationProcessor::GetSnapshotPath(
    int64_t snapshotId, SerializedModuleType type, int32_t instanceId) {
  std::ostringstream oss;
  oss << folder_ << "/" << exchangeId_ << "_snapshot_" << snapshotId << "_"
      << (type == SerializedModuleType::RISK_ENGINE ? "RE" : "ME") << instanceId
      << ".ecs";
  return oss.str();
}

std::string DiskSerializationProcessor::GetJournalPath(int64_t snapshotId,
                                                       int32_t fileIndex) {
  std::ostringstream oss;
  oss << folder_ << "/" << exchangeId_ << "_journal_" << snapshotId << "_"
      << std::hex << std::setfill('0') << std::setw(4) << fileIndex << ".ecj";
  return oss.str();
}

void DiskSerializationProcessor::WriteToJournal(common::cmd::OrderCommand *cmd,
                                                int64_t dSeq, bool eob) {
  // Skip if journaling not enabled
  if (enableJournalAfterSeq_ == -1 ||
      dSeq + baseSeq_ <= enableJournalAfterSeq_) {
    return;
  }
  if (dSeq + baseSeq_ == enableJournalAfterSeq_ + 1) {
    LOG_INFO("Enabled journaling at seq = {} ({}+{})",
             enableJournalAfterSeq_ + 1, baseSeq_, dSeq);
  }

  const auto cmdType = cmd->command;

  // Handle shutdown signal
  if (cmdType == common::cmd::OrderCommandType::SHUTDOWN_SIGNAL) {
    std::lock_guard<std::mutex> lock(journalMutex_);
    FlushBufferSync(false, cmd->timestamp);
    LOG_DEBUG("Shutdown signal received, flushed to disk");
    return;
  }

  // Skip non-mutating commands
  if (!common::cmd::IsMutate(cmdType)) {
    return;
  }

  // Start new file if needed
  {
    std::lock_guard<std::mutex> lock(journalMutex_);
    if (!journalFile_ || !journalFile_->is_open()) {
      StartNewFile(cmd->timestamp);
    }

    // Write command to buffer
    auto &buffer = journalWriteBuffer_;
    size_t &pos = journalWriteBufferPos_;

    // Check if buffer is full
    if (pos + 256 > buffer.size()) { // MAX_COMMAND_SIZE_BYTES
      FlushBufferSync(false, cmd->timestamp);
      pos = 0;
    }

    // Mandatory fields
    buffer[pos++] = static_cast<char>(static_cast<int8_t>(cmdType));
    const int64_t currentSeq = baseSeq_ + dSeq;
    *reinterpret_cast<int64_t *>(&buffer[pos]) = currentSeq;
    pos += sizeof(int64_t);
    lastWrittenSeq_ = currentSeq; // Track last written sequence
    *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->timestamp;
    pos += sizeof(int64_t);
    *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->serviceFlags;
    pos += sizeof(int32_t);
    *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->eventsGroup;
    pos += sizeof(int64_t);

    // Write command-specific fields
    if (cmdType == common::cmd::OrderCommandType::MOVE_ORDER) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->symbol;
      pos += sizeof(int32_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->price;
      pos += sizeof(int64_t);
    } else if (cmdType == common::cmd::OrderCommandType::CANCEL_ORDER) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->symbol;
      pos += sizeof(int32_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
    } else if (cmdType == common::cmd::OrderCommandType::REDUCE_ORDER) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->symbol;
      pos += sizeof(int32_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->size;
      pos += sizeof(int64_t);
    } else if (cmdType == common::cmd::OrderCommandType::PLACE_ORDER) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->symbol;
      pos += sizeof(int32_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->price;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->reserveBidPrice;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->size;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->userCookie;
      pos += sizeof(int32_t);
      const int actionAndType = (static_cast<int>(cmd->orderType) << 1) |
                                static_cast<int>(cmd->action);
      buffer[pos++] = static_cast<char>(actionAndType);
    } else if (cmdType == common::cmd::OrderCommandType::BALANCE_ADJUSTMENT) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
      *reinterpret_cast<int32_t *>(&buffer[pos]) = cmd->symbol;
      pos += sizeof(int32_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->price;
      pos += sizeof(int64_t);
      // Match Java: buffer.put(cmd.orderType.getCode());
      // For BALANCE_ADJUSTMENT, orderType code equals adjustmentType code
      buffer[pos++] =
          static_cast<char>(common::OrderTypeToCode(cmd->orderType));
    } else if (cmdType == common::cmd::OrderCommandType::ADD_USER ||
               cmdType == common::cmd::OrderCommandType::SUSPEND_USER ||
               cmdType == common::cmd::OrderCommandType::RESUME_USER) {
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
    } else if (cmdType == common::cmd::OrderCommandType::BINARY_DATA_COMMAND) {
      buffer[pos++] = static_cast<char>(cmd->symbol);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->orderId;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->price;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->reserveBidPrice;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->size;
      pos += sizeof(int64_t);
      *reinterpret_cast<int64_t *>(&buffer[pos]) = cmd->uid;
      pos += sizeof(int64_t);
    }

    // Handle special commands
    if (cmdType == common::cmd::OrderCommandType::PERSIST_STATE_RISK) {
      // Register snapshot change
      RegisterNextSnapshot(cmd->orderId, baseSeq_ + dSeq, cmd->timestamp);
      // Start new file
      baseSnapshotId_ = cmd->orderId;
      filesCounter_ = 0;
      FlushBufferSync(true, cmd->timestamp);
    } else if (cmdType == common::cmd::OrderCommandType::RESET) {
      // Force start next journal file on reset
      FlushBufferSync(true, cmd->timestamp);
    } else if (eob || pos >= static_cast<size_t>(journalBufferFlushTrigger_)) {
      // Flush on end of batch or when buffer is full
      FlushBufferSync(false, cmd->timestamp);
    }
  }
}

void DiskSerializationProcessor::EnableJournaling(int64_t afterSeq, void *api) {
  enableJournalAfterSeq_ = afterSeq;
  // Match Java: api.groupingControl(0, 1);
  if (api) {
    static_cast<IExchangeApi *>(api)->GroupingControl(0, 1);
  }
}

std::map<int64_t, SnapshotDescriptor *>
DiskSerializationProcessor::FindAllSnapshotPoints() {
  return snapshotsIndex_;
}

void DiskSerializationProcessor::ReplayJournalStep(int64_t snapshotId,
                                                   int64_t seqFrom,
                                                   int64_t seqTo, void *api) {
  throw std::runtime_error("ReplayJournalStep not implemented");
}

int64_t DiskSerializationProcessor::ReplayJournalFull(
    const common::config::InitialStateConfiguration *initialStateConfiguration,
    void *api) {
  if (initialStateConfiguration->journalTimestampNs == 0) {
    LOG_DEBUG("No need to replay journal, returning baseSeq={}", baseSeq_);
    return baseSeq_;
  }

  LOG_DEBUG("Replaying journal...");

  // Match Java: api.groupingControl(0, 0);
  if (api) {
    static_cast<IExchangeApi *>(api)->GroupingControl(0, 0);
  }

  int64_t lastSeq = baseSeq_;
  int partitionCounter = 1;

  while (true) {
    const std::string path =
        GetJournalPath(initialStateConfiguration->snapshotId, partitionCounter);

    LOG_DEBUG("Reading journal file: {}", path);

    if (!std::filesystem::exists(path)) {
      LOG_DEBUG("File not found: {}, returning lastSeq={}", path, lastSeq);
      return lastSeq;
    }

    try {
      std::ifstream file(path, std::ios::binary);
      if (!file.is_open()) {
        LOG_DEBUG("Can not open file: {}, try next partition", path);
        partitionCounter++;
        continue;
      }

      ReadCommands(file, api, lastSeq, false);
      file.close();
      partitionCounter++;
      LOG_DEBUG("File end reached, try next partition {}...", partitionCounter);

    } catch (const std::exception &ex) {
      LOG_DEBUG("File end reached through exception: {}", ex.what());
      partitionCounter++;
    }
  }
}

void DiskSerializationProcessor::ReadCommands(std::istream &is, void *api,
                                              int64_t &lastSeq,
                                              bool insideCompressedBlock) {
  while (is.peek() != EOF && is.good()) {
    int8_t cmdByte;
    if (!is.read(reinterpret_cast<char *>(&cmdByte), 1)) {
      break;
    }

    if (cmdByte == static_cast<int8_t>(
                       common::cmd::OrderCommandType::RESERVED_COMPRESSED)) {
      if (insideCompressedBlock) {
        throw std::runtime_error(
            "Recursive compression block (data corrupted)");
      }

      int32_t compressedSize, originalSize;
      if (!is.read(reinterpret_cast<char *>(&compressedSize),
                   sizeof(int32_t)) ||
          !is.read(reinterpret_cast<char *>(&originalSize), sizeof(int32_t))) {
        break;
      }

      if (compressedSize > 1000000 || originalSize > 1000000) {
        throw std::runtime_error("Bad compressed block size (data corrupted)");
      }

      std::vector<char> compressed(compressedSize);
      if (!is.read(compressed.data(), compressedSize) ||
          is.gcount() < compressedSize) {
        throw std::runtime_error("Can not read full compressed block");
      }

      // Decompress
      std::vector<char> original(originalSize);
      const int decompressedSize = LZ4_decompress_safe(
          compressed.data(), original.data(), compressedSize, originalSize);

      if (decompressedSize != originalSize) {
        throw std::runtime_error("LZ4 decompression failed");
      }

      // Read compressed block recursively
      std::istringstream decompressedStream(
          std::string(original.data(), originalSize));
      ReadCommands(decompressedStream, api, lastSeq, true);

    } else {
      // Read command
      int64_t seq, timestampNs, eventsGroup;
      int32_t serviceFlags;

      if (!is.read(reinterpret_cast<char *>(&seq), sizeof(int64_t)) ||
          !is.read(reinterpret_cast<char *>(&timestampNs), sizeof(int64_t)) ||
          !is.read(reinterpret_cast<char *>(&serviceFlags), sizeof(int32_t)) ||
          !is.read(reinterpret_cast<char *>(&eventsGroup), sizeof(int64_t))) {
        break;
      }

      if (seq != lastSeq + 1) {
        LOG_WARN("Sequence gap {}->{} ({})", lastSeq, seq, seq - lastSeq);
      }

      lastSeq = seq;

      const auto cmdType =
          common::cmd::OrderCommandTypeFromCode(static_cast<int8_t>(cmdByte));

      // Convert void* to IExchangeApi*
      auto *exchangeApi = static_cast<IExchangeApi *>(api);

      // Log command for debugging (only log every 10000th command to avoid
      // spam)
      if (seq % 10000 == 0 ||
          cmdType == common::cmd::OrderCommandType::BINARY_DATA_COMMAND) {
        LOG_DEBUG("Replaying command seq={} type={} timestampNs={} "
                  "serviceFlags={} eventsGroup={}",
                  seq, static_cast<int>(cmdByte), timestampNs, serviceFlags,
                  eventsGroup);
      }

      // Handle different command types
      // Match Java: all replay methods use serviceFlags and eventsGroup
      if (cmdType == common::cmd::OrderCommandType::MOVE_ORDER) {
        // Match Java: api.moveOrder(serviceFlags, eventsGroup, timestampNs,
        // price, orderId, symbol, uid);
        int64_t uid, orderId, price;
        int32_t symbol;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&symbol), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&orderId), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&price), sizeof(int64_t))) {
          break;
        }

        exchangeApi->MoveOrderReplay(serviceFlags, eventsGroup, timestampNs,
                                     price, orderId, symbol, uid);

      } else if (cmdType == common::cmd::OrderCommandType::CANCEL_ORDER) {
        // Match Java: api.cancelOrder(serviceFlags, eventsGroup, timestampNs,
        // orderId, symbol, uid);
        int64_t uid, orderId;
        int32_t symbol;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&symbol), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&orderId), sizeof(int64_t))) {
          break;
        }

        exchangeApi->CancelOrderReplay(serviceFlags, eventsGroup, timestampNs,
                                       orderId, symbol, uid);

      } else if (cmdType == common::cmd::OrderCommandType::REDUCE_ORDER) {
        // Match Java: api.reduceOrder(serviceFlags, eventsGroup, timestampNs,
        // reduceSize, orderId, symbol, uid);
        int64_t uid, orderId, reduceSize;
        int32_t symbol;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&symbol), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&orderId), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&reduceSize), sizeof(int64_t))) {
          break;
        }

        exchangeApi->ReduceOrderReplay(serviceFlags, eventsGroup, timestampNs,
                                       reduceSize, orderId, symbol, uid);

      } else if (cmdType == common::cmd::OrderCommandType::PLACE_ORDER) {
        // Match Java: api.placeNewOrder(serviceFlags, eventsGroup, timestampNs,
        // orderId, userCookie, price, reservedBidPrice, size, orderAction,
        // orderType, symbol, uid);
        int64_t uid, orderId, price, reservedBidPrice, size;
        int32_t symbol, userCookie;
        uint8_t actionAndType;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&symbol), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&orderId), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&price), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&reservedBidPrice),
                     sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&size), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&userCookie), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&actionAndType), 1)) {
          break;
        }

        const auto orderAction = common::OrderActionFromCode(
            static_cast<uint8_t>(actionAndType & 0b1));
        const auto orderType = common::OrderTypeFromCode(
            static_cast<uint8_t>((actionAndType >> 1) & 0b1111));

        exchangeApi->PlaceOrderReplay(
            serviceFlags, eventsGroup, timestampNs, orderId, userCookie, price,
            reservedBidPrice, size, orderAction, orderType, symbol, uid);

      } else if (cmdType == common::cmd::OrderCommandType::BALANCE_ADJUSTMENT) {
        // Match Java: api.balanceAdjustment(serviceFlags, eventsGroup,
        // timestampNs, uid, transactionId, currency, amount, adjustmentType);
        int64_t uid, transactionId, amount;
        int32_t currency;
        uint8_t adjustmentTypeCode;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&currency), sizeof(int32_t)) ||
            !is.read(reinterpret_cast<char *>(&transactionId),
                     sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&amount), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&adjustmentTypeCode), 1)) {
          break;
        }

        const auto adjustmentType =
            common::BalanceAdjustmentTypeFromCode(adjustmentTypeCode);

        exchangeApi->BalanceAdjustmentReplay(serviceFlags, eventsGroup,
                                             timestampNs, uid, transactionId,
                                             currency, amount, adjustmentType);

      } else if (cmdType == common::cmd::OrderCommandType::ADD_USER) {
        // Match Java: api.createUser(serviceFlags, eventsGroup, timestampNs,
        // uid);
        int64_t uid;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t))) {
          break;
        }

        exchangeApi->CreateUserReplay(serviceFlags, eventsGroup, timestampNs,
                                      uid);

      } else if (cmdType == common::cmd::OrderCommandType::SUSPEND_USER) {
        // Match Java: api.suspendUser(serviceFlags, eventsGroup, timestampNs,
        // uid);
        int64_t uid;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t))) {
          break;
        }

        exchangeApi->SuspendUserReplay(serviceFlags, eventsGroup, timestampNs,
                                       uid);

      } else if (cmdType == common::cmd::OrderCommandType::RESUME_USER) {
        // Match Java: api.resumeUser(serviceFlags, eventsGroup, timestampNs,
        // uid);
        int64_t uid;
        if (!is.read(reinterpret_cast<char *>(&uid), sizeof(int64_t))) {
          break;
        }

        exchangeApi->ResumeUserReplay(serviceFlags, eventsGroup, timestampNs,
                                      uid);

      } else if (cmdType ==
                 common::cmd::OrderCommandType::BINARY_DATA_COMMAND) {
        // Match Java: api.binaryData(serviceFlags, eventsGroup, timestampNs,
        // lastFlag, word0, word1, word2, word3, word4);
        int8_t lastFlag;
        int64_t word0, word1, word2, word3, word4;
        if (!is.read(reinterpret_cast<char *>(&lastFlag), 1) ||
            !is.read(reinterpret_cast<char *>(&word0), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&word1), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&word2), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&word3), sizeof(int64_t)) ||
            !is.read(reinterpret_cast<char *>(&word4), sizeof(int64_t))) {
          LOG_ERROR("Failed to read BINARY_DATA_COMMAND data at seq={}", seq);
          break;
        }

        LOG_DEBUG("Replaying BINARY_DATA_COMMAND seq={} lastFlag={} word0={} "
                  "word1={} word2={} word3={} word4={}",
                  seq, static_cast<int>(lastFlag), word0, word1, word2, word3,
                  word4);

        try {
          exchangeApi->BinaryData(serviceFlags, eventsGroup, timestampNs,
                                  lastFlag, word0, word1, word2, word3, word4);
        } catch (const std::exception &ex) {
          LOG_ERROR("Exception while replaying BINARY_DATA_COMMAND seq={}: {}",
                    seq, ex.what());
          throw;
        }

      } else if (cmdType == common::cmd::OrderCommandType::RESET) {
        // Match Java: api.reset(timestampNs);
        exchangeApi->ResetReplay(timestampNs);

      } else {
        LOG_WARN("Unexpected command type in journal replay: {}",
                 static_cast<int>(cmdByte));
        throw std::runtime_error("Unexpected command type in journal replay");
      }
    }
  }
}

void DiskSerializationProcessor::ReplayJournalFullAndThenEnableJouraling(
    const common::config::InitialStateConfiguration *initialStateConfiguration,
    void *api) {
  int64_t seq = ReplayJournalFull(initialStateConfiguration, api);
  EnableJournaling(seq, api);
}

bool DiskSerializationProcessor::CheckSnapshotExists(int64_t snapshotId,
                                                     SerializedModuleType type,
                                                     int32_t instanceId) {
  const std::string path = GetSnapshotPath(snapshotId, type, instanceId);
  const bool exists = std::filesystem::exists(path);
  LOG_INFO("Checking snapshot file {} exists:{}", path, exists);
  return exists;
}

void DiskSerializationProcessor::FlushBufferSync(bool forceStartNextFile,
                                                 int64_t timestampNs) {
  if (journalWriteBufferPos_ <
      static_cast<size_t>(journalBatchCompressThreshold_)) {
    // Uncompressed write for single messages or small batches
    writtenBytes_ += journalWriteBufferPos_;
    if (journalFile_ && journalFile_->is_open()) {
      journalFile_->write(journalWriteBuffer_.data(), journalWriteBufferPos_);
      journalFile_->flush();
    }
    journalWriteBufferPos_ = 0;
  } else {
    // Compressed write for bigger batches
    const int originalLength = static_cast<int>(journalWriteBufferPos_);
    const int maxCompressedSize = LZ4_compressBound(originalLength);
    std::vector<char> compressed(maxCompressedSize);
    const int compressedSize =
        LZ4_compress_default(journalWriteBuffer_.data(), compressed.data(),
                             originalLength, maxCompressedSize);

    if (compressedSize > 0 && journalFile_ && journalFile_->is_open()) {
      // Write compressed block marker
      const int8_t compressedMarker = static_cast<int8_t>(
          common::cmd::OrderCommandType::RESERVED_COMPRESSED);
      journalFile_->write(reinterpret_cast<const char *>(&compressedMarker), 1);
      journalFile_->write(reinterpret_cast<const char *>(&compressedSize),
                          sizeof(int32_t));
      journalFile_->write(reinterpret_cast<const char *>(&originalLength),
                          sizeof(int32_t));
      journalFile_->write(compressed.data(), compressedSize);
      journalFile_->flush();
      writtenBytes_ += compressedSize + 9; // 1 + 4 + 4
    }
    journalWriteBufferPos_ = 0;
  }

  if (forceStartNextFile || writtenBytes_ >= journalFileMaxSize_) {
    StartNewFile(timestampNs);
    writtenBytes_ = 0;
  }
}

void DiskSerializationProcessor::StartNewFile(int64_t timestampNs) {
  filesCounter_++;
  if (journalFile_ && journalFile_->is_open()) {
    // Update seqLast for previous journal descriptor before closing
    // Match Java: seqLast is the last sequence number written to this journal
    if (lastJournalDescriptor_) {
      lastJournalDescriptor_->seqLast = lastWrittenSeq_;
    }
    journalFile_->close();
  }

  const std::string fileName = GetJournalPath(baseSnapshotId_, filesCounter_);
  if (std::filesystem::exists(fileName)) {
    throw std::runtime_error("File already exists: " + fileName);
  }

  journalFile_ = std::make_unique<std::fstream>(
      fileName, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!journalFile_->is_open()) {
    throw std::runtime_error("Can not open journal file: " + fileName);
  }

  // Register new journal (matches Java: registerNextJournal(baseSnapshotId,
  // timestampNs))
  // Note: Java version uses baseSnapshotId as seq parameter, which represents
  // the starting sequence for this journal
  RegisterNextJournal(baseSnapshotId_, timestampNs);
}

void DiskSerializationProcessor::RegisterNextJournal(int64_t seq,
                                                     int64_t timestampNs) {
  // Create new JournalDescriptor and add to linked list
  lastJournalDescriptor_ = new JournalDescriptor(
      timestampNs, seq, lastSnapshotDescriptor_, lastJournalDescriptor_);

  // Add journal to snapshot's journals map
  if (lastSnapshotDescriptor_) {
    lastSnapshotDescriptor_->journals[seq] = lastJournalDescriptor_;
  }
}

void DiskSerializationProcessor::RegisterNextSnapshot(int64_t snapshotId,
                                                      int64_t seq,
                                                      int64_t timestampNs) {
  // Create next snapshot descriptor using createNext method (matches Java)
  if (lastSnapshotDescriptor_) {
    lastSnapshotDescriptor_ =
        lastSnapshotDescriptor_->CreateNext(snapshotId, seq, timestampNs);

    // Update snapshotsIndex
    snapshotsIndex_[snapshotId] = lastSnapshotDescriptor_;

    // Update path for the snapshot (set when snapshot is actually written)
    // Note: path will be set when StoreData is called
  }
}

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
