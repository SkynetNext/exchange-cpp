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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/api/reports/ReportQuery.h>
#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <stdexcept>

namespace exchange::core::common::api::reports {

// Use Meyers Singleton
ReportQueryFactory& ReportQueryFactory::getInstance() {
    static ReportQueryFactory instance;
    return instance;
}

// ReportQueryTypeRegistrar constructor implementation
detail::ReportQueryTypeRegistrar::ReportQueryTypeRegistrar(ReportType type,
                                                           ReportQueryConstructor constructor) {
    ReportQueryFactory::getInstance().registerQueryType(type, constructor);
}

// Register report query type
void ReportQueryFactory::registerQueryType(ReportType type, ReportQueryConstructor constructor) {
    size_t index = static_cast<size_t>(ReportTypeToCode(type));

    // Resize if needed (lazy initialization)
    if (constructors_.size() <= index) {
        constructors_.resize(index + 1);
    }

    constructors_[index] = constructor;
}

// Get constructor for specific type
ReportQueryConstructor ReportQueryFactory::getConstructor(ReportType type) {
    size_t index = static_cast<size_t>(ReportTypeToCode(type));

    if (index >= constructors_.size()) {
        return nullptr;
    }

    return constructors_[index];
}

// Create report query from bytes (returns ReportQueryBase pointer)
ReportQueryBase* ReportQueryFactory::createQuery(ReportType type, BytesIn& bytes) {
    auto constructor = getConstructor(type);
    if (!constructor) {
        throw std::runtime_error("No constructor registered for ReportType: "
                                 + std::to_string(ReportTypeToCode(type)));
    }
    return constructor(bytes);
}

}  // namespace exchange::core::common::api::reports
