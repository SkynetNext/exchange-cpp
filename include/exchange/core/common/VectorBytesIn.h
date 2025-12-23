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

#include "BytesIn.h"
#include <cstring>
#include <vector>

namespace exchange {
namespace core {
namespace common {

/**
 * VectorBytesIn - BytesIn implementation using std::vector<uint8_t>
 * Simple implementation for reading from byte arrays
 */
class VectorBytesIn : public BytesIn {
private:
  const std::vector<uint8_t> &data_;
  size_t position_;

public:
  explicit VectorBytesIn(const std::vector<uint8_t> &data)
      : data_(data), position_(0) {}

  int8_t ReadByte() override {
    if (position_ >= data_.size()) {
      throw std::runtime_error("VectorBytesIn: Read beyond end of data");
    }
    return static_cast<int8_t>(data_[position_++]);
  }

  int32_t ReadInt() override {
    if (position_ + sizeof(int32_t) > data_.size()) {
      throw std::runtime_error("VectorBytesIn: Read beyond end of data");
    }
    int32_t value = 0;
    std::memcpy(&value, data_.data() + position_, sizeof(int32_t));
    position_ += sizeof(int32_t);
    return value;
  }

  int64_t ReadLong() override {
    if (position_ + sizeof(int64_t) > data_.size()) {
      throw std::runtime_error("VectorBytesIn: Read beyond end of data");
    }
    int64_t value = 0;
    std::memcpy(&value, data_.data() + position_, sizeof(int64_t));
    position_ += sizeof(int64_t);
    return value;
  }

  bool ReadBoolean() override {
    if (position_ >= data_.size()) {
      throw std::runtime_error("VectorBytesIn: Read beyond end of data");
    }
    return data_[position_++] != 0;
  }

  int64_t ReadRemaining() const override {
    return static_cast<int64_t>(data_.size() - position_);
  }

  void Read(void *buffer, size_t length) override {
    if (position_ + length > data_.size()) {
      throw std::runtime_error("VectorBytesIn: Read beyond end of data");
    }
    std::memcpy(buffer, data_.data() + position_, length);
    position_ += length;
  }

  size_t GetPosition() const { return position_; }
  void SetPosition(size_t pos) { position_ = pos; }
};

} // namespace common
} // namespace core
} // namespace exchange

