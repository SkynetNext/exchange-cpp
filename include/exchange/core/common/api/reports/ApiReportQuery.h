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

#include "ReportQuery.h"
#include "ReportResult.h"
#include <cstdint>
#include <memory>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

/**
 * ApiReportQuery - report query wrapper
 */
class ApiReportQuery {
public:
  int64_t timestamp = 0;

  // transfer unique id
  // can be constant unless going to push data concurrently
  int32_t transferId;

  // serializable object
  std::unique_ptr<ReportQuery<ReportResult>> query;

  ApiReportQuery(int32_t transferId,
                 std::unique_ptr<ReportQuery<ReportResult>> query)
      : transferId(transferId), query(std::move(query)) {}
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
