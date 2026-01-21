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

#include <memory>
#include <vector>
#include "BytesIn.h"
#include "VectorBytesIn.h"

namespace exchange::core::common {

/**
 * Wire - simple wrapper for BytesIn
 * Match Java: Wire from Chronicle Bytes
 * Simple 1:1 translation
 */
class Wire {
private:
  std::vector<uint8_t> bytes_;
  std::shared_ptr<VectorBytesIn> bytesIn_;

public:
  // Default constructor (required for std::map::operator[])
  Wire() : bytesIn_(std::make_shared<VectorBytesIn>(bytes_)) {}

  explicit Wire(std::vector<uint8_t> bytes)
    : bytes_(std::move(bytes)), bytesIn_(std::make_shared<VectorBytesIn>(bytes_)) {}

  // Copy constructor - ensure bytesIn_ references the new bytes_
  Wire(const Wire& other)
    : bytes_(other.bytes_), bytesIn_(std::make_shared<VectorBytesIn>(bytes_)) {}

  // Move constructor
  Wire(Wire&& other) noexcept
    : bytes_(std::move(other.bytes_)), bytesIn_(std::make_shared<VectorBytesIn>(bytes_)) {}

  // Copy assignment operator
  Wire& operator=(const Wire& other) {
    if (this != &other) {
      bytes_ = other.bytes_;
      bytesIn_ = std::make_shared<VectorBytesIn>(bytes_);
    }
    return *this;
  }

  // Move assignment operator
  Wire& operator=(Wire&& other) noexcept {
    if (this != &other) {
      bytes_ = std::move(other.bytes_);
      bytesIn_ = std::make_shared<VectorBytesIn>(bytes_);
    }
    return *this;
  }

  /**
   * Get BytesIn from Wire
   * Match Java: Wire::bytes
   * Reset position to 0 before returning (each call should start from
   * beginning)
   */
  BytesIn& bytes() {
    bytesIn_->SetPosition(0);
    return *bytesIn_;
  }

  const BytesIn& bytes() const {
    const_cast<VectorBytesIn*>(bytesIn_.get())->SetPosition(0);
    return *bytesIn_;
  }

  /**
   * Get underlying bytes vector
   * For ProcessReportAny compatibility
   */
  const std::vector<uint8_t>& GetBytes() const {
    return bytes_;
  }
};

}  // namespace exchange::core::common
