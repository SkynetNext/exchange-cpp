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

#include <exchange/core/ExchangeApi.h>
#include <exchange/core/ExchangeCore.h>
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/api/ApiNop.h>
#include <exchange/core/common/api/ApiReset.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>
#include <exchange/core/common/api/binary/BinaryDataCommand.h>
#include <exchange/core/common/api/reports/SingleUserReportQuery.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/common/api/reports/StateHashReportQuery.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportQuery.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "TestDataParameters.h"
#include "TestOrdersGenerator.h"

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * TestDataFutures - futures for async test data preparation
 *
 * Note: Using std::shared_future instead of std::future to allow multiple calls
 * to get() This matches Java CompletableFuture behavior where join() can be
 * called multiple times. std::future.get() can only be called once, but
 * std::shared_future.get() can be called multiple times, making it more similar
 * to Java's CompletableFuture.
 *
 * Note: genResult uses std::shared_ptr because MultiSymbolGenResult contains
 * non-copyable OrderCommand objects. Using shared_ptr allows the future to
 * return a pointer copy instead of trying to copy the entire structure.
 */
struct TestDataFutures {
  std::shared_future<std::vector<exchange::core::common::CoreSymbolSpecification>>
    coreSymbolSpecifications;
  std::shared_future<std::vector<std::vector<bool>>> usersAccounts;
  std::shared_future<std::shared_ptr<TestOrdersGenerator::MultiSymbolGenResult>> genResult;
};

/**
 * ExchangeTestContainer - RAII container for ExchangeCore in tests
 * Manages lifecycle of ExchangeCore and provides convenient test methods
 */
class ExchangeTestContainer {
public:
  /**
   * Create container with default initial state and serialization config
   */
  static std::unique_ptr<ExchangeTestContainer>
  Create(const exchange::core::common::config::PerformanceConfiguration& perfCfg);

  /**
   * Create container with custom configuration
   */
  static std::unique_ptr<ExchangeTestContainer>
  Create(const exchange::core::common::config::PerformanceConfiguration& perfCfg,
         const exchange::core::common::config::InitialStateConfiguration& initStateCfg,
         const exchange::core::common::config::SerializationConfiguration& serializationCfg);

  /**
   * Prepare test data asynchronously
   */
  static TestDataFutures PrepareTestDataAsync(const TestDataParameters& parameters, int seed);

  /**
   * Generate random symbols for testing
   */
  static std::vector<exchange::core::common::CoreSymbolSpecification>
  GenerateRandomSymbols(int num,
                        const std::set<int32_t>& currenciesAllowed,
                        AllowedSymbolTypes allowedSymbolTypes);

  /**
   * Time-based exchange ID generator
   */
  static std::string TimeBasedExchangeId();

  /**
   * Check success callback for command validation
   */
  static void CheckSuccess(const exchange::core::common::cmd::OrderCommand& cmd);

  /**
   * Destructor - automatically shuts down exchange core
   */
  ~ExchangeTestContainer();

  // Non-copyable
  ExchangeTestContainer(const ExchangeTestContainer&) = delete;
  ExchangeTestContainer& operator=(const ExchangeTestContainer&) = delete;

  // Movable
  ExchangeTestContainer(ExchangeTestContainer&&) noexcept;
  ExchangeTestContainer& operator=(ExchangeTestContainer&&) noexcept;

  /**
   * Get ExchangeApi
   */
  exchange::core::IExchangeApi* GetApi() const {
    return api_;
  }

  /**
   * Initialize basic symbols (EUR_USD, ETH_XBT)
   */
  void InitBasicSymbols();

  /**
   * Initialize fee symbols (XBT_LTC, USD_JPY)
   */
  void InitFeeSymbols();

  /**
   * Initialize basic users (UID_1 to UID_4)
   */
  void InitBasicUsers();

  /**
   * Initialize fee users (UID_1 to UID_4 with fee currencies)
   */
  void InitFeeUsers();

  /**
   * Initialize a single basic user
   */
  void InitBasicUser(int64_t uid);

  /**
   * Initialize a single fee user
   */
  void InitFeeUser(int64_t uid);

  /**
   * Create user with money
   */
  void CreateUserWithMoney(int64_t uid, int32_t currency, int64_t amount);

  /**
   * Add money to existing user
   */
  void AddMoneyToUser(int64_t uid, int32_t currency, int64_t amount);

  /**
   * Add symbol to exchange
   */
  void AddSymbol(const exchange::core::common::CoreSymbolSpecification& symbol);

  /**
   * Add multiple symbols to exchange
   */
  void AddSymbols(const std::vector<exchange::core::common::CoreSymbolSpecification>& symbols);

  /**
   * Send binary data command synchronously
   */
  void SendBinaryDataCommandSync(
    std::unique_ptr<exchange::core::common::api::binary::BinaryDataCommand> data,
    int timeOutMs);

  /**
   * Initialize user accounts from currency BitSets
   */
  void UserAccountsInit(const std::vector<std::vector<bool>>& userCurrencies);

  /**
   * Initialize users with currencies
   */
  void UsersInit(int numUsers, const std::set<int32_t>& currencies);

  /**
   * Reset exchange core
   */
  void ResetExchangeCore();

  /**
   * Submit command synchronously and validate result code
   */
  void SubmitCommandSync(std::unique_ptr<exchange::core::common::api::ApiCommand> apiCommand,
                         exchange::core::common::cmd::CommandResultCode expectedResultCode);

  /**
   * Submit command synchronously and validate with custom validator
   */
  void SubmitCommandSync(
    std::unique_ptr<exchange::core::common::api::ApiCommand> apiCommand,
    std::function<void(const exchange::core::common::cmd::OrderCommand&)> validator);

  /**
   * Request current order book
   */
  std::shared_ptr<exchange::core::common::L2MarketData> RequestCurrentOrderBook(int32_t symbol);

  /**
   * Validate user state
   */
  void ValidateUserState(
    int64_t uid,
    std::function<void(const exchange::core::common::api::reports::SingleUserReportResult&)>
      resultValidator);

  /**
   * Get user profile
   */
  std::unique_ptr<exchange::core::common::api::reports::SingleUserReportResult>
  GetUserProfile(int64_t clientId);

  /**
   * Get total balance report
   */
  std::unique_ptr<exchange::core::common::api::reports::TotalCurrencyBalanceReportResult>
  TotalBalanceReport();

  /**
   * Request state hash
   */
  int32_t RequestStateHash();

  /**
   * Load symbols, users and prefill orders from test data futures
   */
  void LoadSymbolsUsersAndPrefillOrders(const TestDataFutures& testDataFutures);

  /**
   * Load symbols, users and prefill orders (no logging)
   */
  void LoadSymbolsUsersAndPrefillOrdersNoLog(const TestDataFutures& testDataFutures);

  /**
   * Set command consumer callback
   */
  void
  SetConsumer(std::function<void(exchange::core::common::cmd::OrderCommand*, int64_t)> consumer);

private:
  ExchangeTestContainer(
    const exchange::core::common::config::PerformanceConfiguration& perfCfg,
    const exchange::core::common::config::InitialStateConfiguration& initStateCfg,
    const exchange::core::common::config::SerializationConfiguration& serializationCfg);

  int32_t GetRandomTransferId();
  int64_t GetRandomTransactionId();

  void CreateUserAccountsRegular(const std::vector<std::vector<bool>>& userCurrencies,
                                 const std::map<int32_t, int64_t>& amountPerAccount);

  std::unique_ptr<exchange::core::ExchangeCore> exchangeCore_;
  exchange::core::IExchangeApi* api_;
  std::atomic<int64_t> uniqueIdCounterLong_;
  std::atomic<int32_t> uniqueIdCounterInt_;
  std::function<void(exchange::core::common::cmd::OrderCommand*, int64_t)> consumer_;
};

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
