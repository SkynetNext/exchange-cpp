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

#include "ReportType.h"
#include <functional>
#include <vector>

namespace exchange {
namespace core {
namespace common {
class BytesIn;
namespace api {
namespace reports {

// Forward declaration
template <typename T> class ReportQuery;

// Type-erased report query constructor
using ReportQueryConstructor = std::function<void *(BytesIn &)>;

class ReportQueryFactory {
private:
  std::vector<ReportQueryConstructor> constructors_;

  ReportQueryFactory() = default;

public:
  ReportQueryFactory(ReportQueryFactory const &) = delete;
  ReportQueryFactory &operator=(ReportQueryFactory const &) = delete;

  // Use Meyers Singleton
  static ReportQueryFactory &getInstance();

  // Register report query type
  void registerQueryType(ReportType type, ReportQueryConstructor constructor);

  // Get constructor for specific type
  ReportQueryConstructor getConstructor(ReportType type);

  // Create report query from bytes (returns type-erased pointer)
  void *createQuery(ReportType type, BytesIn &bytes);
};

namespace detail {
struct ReportQueryTypeRegistrar {
  ReportQueryTypeRegistrar(ReportType type, ReportQueryConstructor constructor);
};
} // namespace detail

// Register report query type macro
#define REGISTER_REPORT_QUERY_TYPE(QueryType, EnumType)                        \
  static detail::ReportQueryTypeRegistrar QueryType##_registrar(               \
      EnumType, [](BytesIn &bytes) -> void * { return new QueryType(bytes); })

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
