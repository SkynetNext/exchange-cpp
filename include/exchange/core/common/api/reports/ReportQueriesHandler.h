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
#include <optional>
#include "ReportQuery.h"
#include "ReportResult.h"

namespace exchange::core::common::api::reports {

/**
 * ReportQueriesHandler - interface for handling report queries
 *
 * Note: In C++, template methods cannot be virtual, so we use a different
 * approach. The HandleReport template method calls a virtual method
 * HandleReportImpl that subclasses can override.
 */
class ReportQueriesHandler {
public:
  virtual ~ReportQueriesHandler() = default;

  template <typename R>
  std::optional<std::unique_ptr<R>> HandleReport(ReportQuery<R>* reportQuery) {
    // Call virtual method with type erasure
    // Subclasses override HandleReportImpl to provide actual implementation
    // ReportQuery<R>* can be safely cast to ReportQueryBase* since
    // ReportQuery<R> inherits from ReportQueryBase
    auto result = HandleReportImpl(static_cast<ReportQueryBase*>(reportQuery));
    if (result.has_value()) {
      // Cast from ReportResult* to R*
      R* casted = dynamic_cast<R*>(result.value().get());
      if (casted != nullptr) {
        // Transfer ownership
        result.value().release();
        return std::make_optional(std::unique_ptr<R>(casted));
      }
    }
    return std::nullopt;
  }

protected:
  // Virtual method for type-erased handling
  // Subclasses should override this to provide actual implementation
  // Uses ReportQueryBase* for type erasure (non-template base class)
  virtual std::optional<std::unique_ptr<ReportResult>>
  HandleReportImpl(ReportQueryBase* reportQuery) {
    return std::nullopt;
  }
};

}  // namespace exchange::core::common::api::reports
