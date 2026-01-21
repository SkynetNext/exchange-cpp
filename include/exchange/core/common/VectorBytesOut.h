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

#include <cstring>
#include <vector>
#include "BytesOut.h"

namespace exchange {
namespace core {
namespace common {

/**
 * VectorBytesOut - BytesOut implementation using std::vector<uint8_t>
 * Simple implementation for writing to byte arrays
 */
class VectorBytesOut : public BytesOut {
private:
  std::vector<uint8_t>& data_;
  size_t position_;

public:
  explicit VectorBytesOut(std::vector<uint8_t>& data) : data_(data), position_(0) {}

  BytesOut& WriteByte(int8_t value) override {
    if (position_ >= data_.size()) {
      data_.resize(position_ + 1);
    }
    data_[position_++] = static_cast<uint8_t>(value);
    return *this;
  }

  BytesOut& WriteInt(int32_t value) override {
    if (position_ + sizeof(int32_t) > data_.size()) {
      data_.resize(position_ + sizeof(int32_t));
    }
    std::memcpy(data_.data() + position_, &value, sizeof(int32_t));
    position_ += sizeof(int32_t);
    return *this;
  }

  BytesOut& WriteLong(int64_t value) override {
    if (position_ + sizeof(int64_t) > data_.size()) {
      data_.resize(position_ + sizeof(int64_t));
    }
    std::memcpy(data_.data() + position_, &value, sizeof(int64_t));
    position_ += sizeof(int64_t);
    return *this;
  }

  BytesOut& WriteBoolean(bool value) override {
    if (position_ >= data_.size()) {
      data_.resize(position_ + 1);
    }
    data_[position_++] = value ? 1 : 0;
    return *this;
  }

  BytesOut& Write(const void* buffer, size_t length) override {
    if (position_ + length > data_.size()) {
      data_.resize(position_ + length);
    }
    std::memcpy(data_.data() + position_, buffer, length);
    position_ += length;
    return *this;
  }

  int64_t WritePosition() const override {
    return static_cast<int64_t>(position_);
  }

  size_t GetPosition() const {
    return position_;
  }

  void SetPosition(size_t pos) {
    position_ = pos;
  }

  // Get the underlying vector (for reading back)
  const std::vector<uint8_t>& GetData() const {
    return data_;
  }

  std::vector<uint8_t>& GetData() {
    return data_;
  }
};

}  // namespace common
}  // namespace core
}  // namespace exchange
