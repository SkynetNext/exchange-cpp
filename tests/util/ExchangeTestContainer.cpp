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

#include "ExchangeTestContainer.h"
#include "TestConstants.h"
#include "UserCurrencyAccountsGenerator.h"
#include <chrono>
#include <exchange/core/ExchangeApi.h>
#include <exchange/core/ExchangeCore.h>
#include <exchange/core/common/VectorBytesIn.h>
#include <exchange/core/common/VectorBytesOut.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiBinaryDataCommand.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiNop.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/api/ApiReset.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>
#include <exchange/core/common/api/reports/SingleUserReportQuery.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/common/api/reports/StateHashReportQuery.h>
#include <exchange/core/common/api/reports/StateHashReportResult.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportQuery.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/common/config/OrdersProcessingConfiguration.h>
#include <exchange/core/common/config/ReportsQueriesConfiguration.h>
#include <exchange/core/utils/AffinityThreadFactory.h>
#include <random>
#include <unordered_map>

namespace exchange {
namespace core {
namespace tests {
namespace util {

// Static helper for CHECK_SUCCESS callback
void ExchangeTestContainer::CheckSuccess(
    const exchange::core::common::cmd::OrderCommand &cmd) {
  if (cmd.resultCode !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Command failed with code: " +
                             std::to_string(static_cast<int>(cmd.resultCode)));
  }
}

// Static factory methods
std::unique_ptr<ExchangeTestContainer> ExchangeTestContainer::Create(
    const exchange::core::common::config::PerformanceConfiguration &perfCfg) {
  return Create(
      perfCfg,
      exchange::core::common::config::InitialStateConfiguration::Default(),
      exchange::core::common::config::SerializationConfiguration::Default());
}

std::unique_ptr<ExchangeTestContainer> ExchangeTestContainer::Create(
    const exchange::core::common::config::PerformanceConfiguration &perfCfg,
    const exchange::core::common::config::InitialStateConfiguration
        &initStateCfg,
    const exchange::core::common::config::SerializationConfiguration
        &serializationCfg) {
  return std::unique_ptr<ExchangeTestContainer>(
      new ExchangeTestContainer(perfCfg, initStateCfg, serializationCfg));
}

// Constructor
ExchangeTestContainer::ExchangeTestContainer(
    const exchange::core::common::config::PerformanceConfiguration &perfCfg,
    const exchange::core::common::config::InitialStateConfiguration
        &initStateCfg,
    const exchange::core::common::config::SerializationConfiguration
        &serializationCfg)
    : uniqueIdCounterLong_(0), uniqueIdCounterInt_(0) {

  // Create ExchangeConfiguration
  // Match Java: ExchangeCore uses perfCfg.getThreadFactory(), so we directly
  // use the same threadFactory instance via shared_ptr (matching Java's
  // reference semantics)
  exchange::core::common::config::PerformanceConfiguration perfCfgCopy(
      perfCfg.ringBufferSize, perfCfg.matchingEnginesNum,
      perfCfg.riskEnginesNum, perfCfg.msgsInGroupLimit,
      perfCfg.maxGroupDurationNs, perfCfg.sendL2ForEveryCmd,
      perfCfg.l2RefreshDepth, perfCfg.waitStrategy, perfCfg.threadFactory,
      perfCfg.orderBookFactory);

  exchange::core::common::config::ExchangeConfiguration exchangeConfiguration(
      exchange::core::common::config::OrdersProcessingConfiguration::Default(),
      std::move(perfCfgCopy), initStateCfg,
      exchange::core::common::config::ReportsQueriesConfiguration::Default(),
      exchange::core::common::config::LoggingConfiguration::Default(),
      serializationCfg);

  // Create results consumer
  auto resultsConsumer = [this](exchange::core::common::cmd::OrderCommand *cmd,
                                int64_t seq) {
    if (consumer_) {
      consumer_(cmd, seq);
    }
  };

  // Create ExchangeCore
  exchangeCore_ = std::make_unique<exchange::core::ExchangeCore>(
      resultsConsumer, &exchangeConfiguration);

  // Startup
  exchangeCore_->Startup();

  // Get API
  api_ = exchangeCore_->GetApi();
  if (!api_) {
    throw std::runtime_error("GetApi() returned nullptr");
  }
}

// Destructor
ExchangeTestContainer::~ExchangeTestContainer() {
  if (exchangeCore_) {
    exchangeCore_->Shutdown(3000);
  }
}

// Move constructor
ExchangeTestContainer::ExchangeTestContainer(
    ExchangeTestContainer &&other) noexcept
    : exchangeCore_(std::move(other.exchangeCore_)), api_(other.api_),
      uniqueIdCounterLong_(other.uniqueIdCounterLong_.load()),
      uniqueIdCounterInt_(other.uniqueIdCounterInt_.load()),
      consumer_(std::move(other.consumer_)) {
  other.api_ = nullptr;
}

// Move assignment
ExchangeTestContainer &
ExchangeTestContainer::operator=(ExchangeTestContainer &&other) noexcept {
  if (this != &other) {
    exchangeCore_ = std::move(other.exchangeCore_);
    api_ = other.api_;
    uniqueIdCounterLong_ = other.uniqueIdCounterLong_.load();
    uniqueIdCounterInt_ = other.uniqueIdCounterInt_.load();
    consumer_ = std::move(other.consumer_);
    other.api_ = nullptr;
  }
  return *this;
}

// Initialize basic symbols
void ExchangeTestContainer::InitBasicSymbols() {
  AddSymbol(TestConstants::GetSymbolSpecEurUsd());
  AddSymbol(TestConstants::GetSymbolSpecEthXbt());
}

// Initialize fee symbols
void ExchangeTestContainer::InitFeeSymbols() {
  AddSymbol(TestConstants::GetSymbolSpecFeeXbtLtc());
  AddSymbol(TestConstants::GetSymbolSpecFeeUsdJpy());
}

// Initialize basic users
void ExchangeTestContainer::InitBasicUsers() {
  InitBasicUser(TestConstants::UID_1);
  InitBasicUser(TestConstants::UID_2);
  InitBasicUser(TestConstants::UID_3);
  InitBasicUser(TestConstants::UID_4);
}

// Initialize fee users
void ExchangeTestContainer::InitFeeUsers() {
  InitFeeUser(TestConstants::UID_1);
  InitFeeUser(TestConstants::UID_2);
  InitFeeUser(TestConstants::UID_3);
  InitFeeUser(TestConstants::UID_4);
}

// Initialize a single basic user
void ExchangeTestContainer::InitBasicUser(int64_t uid) {
  auto addUserCmd =
      std::make_unique<exchange::core::common::api::ApiAddUser>(uid);
  auto result1 = api_->SubmitCommandAsync(addUserCmd.release());
  if (result1.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to add user");
  }

  auto adjust1 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_USD, 10'000'00L,
          GetRandomTransactionId());
  auto result2 = api_->SubmitCommandAsync(adjust1.release());
  if (result2.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance USD");
  }

  auto adjust2 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_XBT, 1'0000'0000L,
          GetRandomTransactionId());
  auto result3 = api_->SubmitCommandAsync(adjust2.release());
  if (result3.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance XBT");
  }

  auto adjust3 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_ETH, 1'0000'0000L,
          GetRandomTransactionId());
  auto result4 = api_->SubmitCommandAsync(adjust3.release());
  if (result4.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance ETH");
  }
}

// Initialize a single fee user
void ExchangeTestContainer::InitFeeUser(int64_t uid) {
  auto addUserCmd =
      std::make_unique<exchange::core::common::api::ApiAddUser>(uid);
  auto result1 = api_->SubmitCommandAsync(addUserCmd.release());
  if (result1.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to add user");
  }

  auto adjust1 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_USD, 10'000'00L,
          GetRandomTransactionId());
  auto result2 = api_->SubmitCommandAsync(adjust1.release());
  if (result2.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance USD");
  }

  auto adjust2 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_JPY, 10'000'000L,
          GetRandomTransactionId());
  auto result3 = api_->SubmitCommandAsync(adjust2.release());
  if (result3.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance JPY");
  }

  auto adjust3 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_XBT, 1'0000'0000L,
          GetRandomTransactionId());
  auto result4 = api_->SubmitCommandAsync(adjust3.release());
  if (result4.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance XBT");
  }

  auto adjust4 =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          uid, TestConstants::CURRENCY_LTC, 1000'0000'0000L,
          GetRandomTransactionId());
  auto result5 = api_->SubmitCommandAsync(adjust4.release());
  if (result5.get() !=
      exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Failed to adjust balance LTC");
  }
}

// Create user with money
void ExchangeTestContainer::CreateUserWithMoney(int64_t uid, int32_t currency,
                                                int64_t amount) {
  std::vector<exchange::core::common::api::ApiCommand *> cmds;
  cmds.push_back(new exchange::core::common::api::ApiAddUser(uid));
  cmds.push_back(new exchange::core::common::api::ApiAdjustUserBalance(
      uid, currency, amount, GetRandomTransactionId()));
  api_->SubmitCommandsSync(cmds);
  // Note: ApiCommand pointers are owned by ExchangeApi, don't delete them
}

// Add money to existing user
void ExchangeTestContainer::AddMoneyToUser(int64_t uid, int32_t currency,
                                           int64_t amount) {
  std::vector<exchange::core::common::api::ApiCommand *> cmds;
  cmds.push_back(new exchange::core::common::api::ApiAdjustUserBalance(
      uid, currency, amount, GetRandomTransactionId()));
  api_->SubmitCommandsSync(cmds);
}

// Add symbol
void ExchangeTestContainer::AddSymbol(
    const exchange::core::common::CoreSymbolSpecification &symbol) {
  auto batchCmd = std::make_unique<
      exchange::core::common::api::binary::BatchAddSymbolsCommand>(&symbol);
  SendBinaryDataCommandSync(std::move(batchCmd), 5000);
}

// Add multiple symbols
void ExchangeTestContainer::AddSymbols(
    const std::vector<exchange::core::common::CoreSymbolSpecification>
        &symbols) {
  // Split into chunks of 10000 (matching Java version)
  const size_t chunkSize = 10000;
  for (size_t i = 0; i < symbols.size(); i += chunkSize) {
    size_t end = std::min(i + chunkSize, symbols.size());
    // Convert to vector of pointers
    std::vector<const exchange::core::common::CoreSymbolSpecification *>
        chunkPtrs;
    chunkPtrs.reserve(end - i);
    for (size_t j = i; j < end; j++) {
      chunkPtrs.push_back(&symbols[j]);
    }
    auto batchCmd = std::make_unique<
        exchange::core::common::api::binary::BatchAddSymbolsCommand>(chunkPtrs);
    SendBinaryDataCommandSync(std::move(batchCmd), 5000);
  }
}

// Send binary data command synchronously
void ExchangeTestContainer::SendBinaryDataCommandSync(
    std::unique_ptr<exchange::core::common::api::binary::BinaryDataCommand>
        data,
    int timeOutMs) {
  auto binaryCmd =
      std::make_unique<exchange::core::common::api::ApiBinaryDataCommand>(
          GetRandomTransferId(), std::move(data));
  auto future = api_->SubmitCommandAsync(binaryCmd.release());
  auto status = future.wait_for(std::chrono::milliseconds(timeOutMs));
  if (status == std::future_status::timeout) {
    throw std::runtime_error("Binary data command timeout");
  }
  auto result = future.get();
  if (result != exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Binary data command failed with code: " +
                             std::to_string(static_cast<int>(result)));
  }
}

// Initialize user accounts from currency BitSets
void ExchangeTestContainer::UserAccountsInit(
    const std::vector<std::vector<bool>> &userCurrencies) {
  // Calculate max amount per account to avoid overflow
  std::unordered_map<int32_t, int64_t> accountsNumPerCurrency;
  for (const auto &currencies : userCurrencies) {
    for (size_t currencyIdx = 0; currencyIdx < currencies.size();
         currencyIdx++) {
      if (currencies[currencyIdx]) {
        int32_t currency = static_cast<int32_t>(currencyIdx);
        accountsNumPerCurrency[currency]++;
      }
    }
  }

  std::map<int32_t, int64_t> amountPerAccount;
  for (const auto &pair : accountsNumPerCurrency) {
    amountPerAccount[pair.first] = INT64_MAX / (pair.second + 1);
  }

  CreateUserAccountsRegular(userCurrencies, amountPerAccount);
}

// Initialize users with currencies
void ExchangeTestContainer::UsersInit(int numUsers,
                                      const std::set<int32_t> &currencies) {
  std::vector<exchange::core::common::api::ApiCommand *> cmds;
  for (int64_t uid = 1; uid <= numUsers; uid++) {
    cmds.push_back(new exchange::core::common::api::ApiAddUser(uid));
    int64_t transactionId = 1L;
    for (int32_t currency : currencies) {
      cmds.push_back(new exchange::core::common::api::ApiAdjustUserBalance(
          uid, currency, 10'0000'0000L, transactionId++));
    }
  }
  api_->SubmitCommand(new exchange::core::common::api::ApiNop());
  api_->SubmitCommandsSync(cmds);
}

// Reset exchange core
void ExchangeTestContainer::ResetExchangeCore() {
  auto resetCmd = std::make_unique<exchange::core::common::api::ApiReset>();
  auto future = api_->SubmitCommandAsync(resetCmd.release());
  auto result = future.get();
  if (result != exchange::core::common::cmd::CommandResultCode::SUCCESS) {
    throw std::runtime_error("Reset failed with code: " +
                             std::to_string(static_cast<int>(result)));
  }
}

// Submit command synchronously with expected result code
void ExchangeTestContainer::SubmitCommandSync(
    std::unique_ptr<exchange::core::common::api::ApiCommand> apiCommand,
    exchange::core::common::cmd::CommandResultCode expectedResultCode) {
  auto future = api_->SubmitCommandAsync(apiCommand.release());
  auto result = future.get();
  if (result != expectedResultCode) {
    throw std::runtime_error(
        "Command failed: expected " +
        std::to_string(static_cast<int>(expectedResultCode)) + ", got " +
        std::to_string(static_cast<int>(result)));
  }
}

// Submit command synchronously with validator
void ExchangeTestContainer::SubmitCommandSync(
    std::unique_ptr<exchange::core::common::api::ApiCommand> apiCommand,
    std::function<void(const exchange::core::common::cmd::OrderCommand &)>
        validator) {
  // Matches Java:
  // validator.accept(api.submitCommandAsyncFullResponse(apiCommand).join())
  validator(api_->SubmitCommandAsyncFullResponse(apiCommand.release()).get());
}

// Request current order book
std::unique_ptr<exchange::core::common::L2MarketData>
ExchangeTestContainer::RequestCurrentOrderBook(int32_t symbol) {
  return api_->RequestOrderBookAsync(symbol, -1).get();
}

// Validate user state
void ExchangeTestContainer::ValidateUserState(
    int64_t uid,
    std::function<void(
        const exchange::core::common::api::reports::SingleUserReportResult &)>
        resultValidator) {
  auto profile = GetUserProfile(uid);
  if (profile) {
    resultValidator(*profile);
  } else {
    throw std::runtime_error("Failed to get user profile");
  }
}

// Get user profile
// Match Java: return api.processReport(new SingleUserReportQuery(clientId),
// getRandomTransferId()).get();
std::unique_ptr<exchange::core::common::api::reports::SingleUserReportResult>
ExchangeTestContainer::GetUserProfile(int64_t clientId) {
  auto query = std::make_unique<
      exchange::core::common::api::reports::SingleUserReportQuery>(clientId);

  // Use ProcessReportHelper to match Java api.processReport behavior
  // ProcessReportHelper handles serialization, ProcessReportAny, and
  // CreateResult
  try {
    auto future = exchange::core::ProcessReportHelper<
        exchange::core::common::api::reports::SingleUserReportQuery,
        exchange::core::common::api::reports::SingleUserReportResult>(
        api_, std::move(query), GetRandomTransferId());
    return future.get();
  } catch (const std::future_error &e) {
    if (e.code() == std::future_errc::no_state) {
      throw std::runtime_error("GetUserProfile failed for clientId " +
                               std::to_string(clientId) + ": " +
                               std::string(e.what()) +
                               " - This usually means the promise was "
                               "destroyed before set_value() was called");
    }
    throw;
  }
}

// Helper function to check if all balances are zero
static bool IsAllBalancesZero(
    const exchange::core::common::api::reports::TotalCurrencyBalanceReportResult
        &result) {
  // Check account balances
  if (result.accountBalances) {
    for (const auto &pair : *result.accountBalances) {
      if (pair.second != 0) {
        return false;
      }
    }
  }

  // Check fees
  if (result.fees) {
    for (const auto &pair : *result.fees) {
      if (pair.second != 0) {
        return false;
      }
    }
  }

  // Check adjustments
  if (result.adjustments) {
    for (const auto &pair : *result.adjustments) {
      if (pair.second != 0) {
        return false;
      }
    }
  }

  // Check suspends
  if (result.suspends) {
    for (const auto &pair : *result.suspends) {
      if (pair.second != 0) {
        return false;
      }
    }
  }

  // Check orders balances
  if (result.ordersBalances) {
    for (const auto &pair : *result.ordersBalances) {
      if (pair.second != 0) {
        return false;
      }
    }
  }

  // Check open interest (long and short should balance)
  if (result.openInterestLong && result.openInterestShort) {
    for (const auto &pair : *result.openInterestLong) {
      auto it = result.openInterestShort->find(pair.first);
      int64_t diff = pair.second -
                     (it != result.openInterestShort->end() ? it->second : 0);
      if (diff != 0) {
        return false;
      }
    }
    for (const auto &pair : *result.openInterestShort) {
      if (result.openInterestLong->find(pair.first) ==
          result.openInterestLong->end()) {
        if (pair.second != 0) {
          return false;
        }
      }
    }
  }

  return true;
}

// Get total balance report
std::unique_ptr<
    exchange::core::common::api::reports::TotalCurrencyBalanceReportResult>
ExchangeTestContainer::TotalBalanceReport() {
  auto query = std::make_unique<
      exchange::core::common::api::reports::TotalCurrencyBalanceReportQuery>();

  // Use ProcessReportHelper to properly handle serialization and section
  // merging This matches the Java api.processReport behavior
  auto result =
      exchange::core::ProcessReportHelper<
          exchange::core::common::api::reports::TotalCurrencyBalanceReportQuery,
          exchange::core::common::api::reports::
              TotalCurrencyBalanceReportResult>(api_, std::move(query),
                                                GetRandomTransferId())
          .get();

  // Verify open interest balance (matching Java version behavior)
  if (result && result->openInterestLong && result->openInterestShort) {
    for (const auto &pair : *result->openInterestLong) {
      auto it = result->openInterestShort->find(pair.first);
      int64_t diff = pair.second -
                     (it != result->openInterestShort->end() ? it->second : 0);
      if (diff != 0) {
        throw std::runtime_error("Open Interest balance check failed");
      }
    }
    for (const auto &pair : *result->openInterestShort) {
      if (result->openInterestLong->find(pair.first) ==
          result->openInterestLong->end()) {
        if (pair.second != 0) {
          throw std::runtime_error("Open Interest balance check failed");
        }
      }
    }
  }

  return result;
}

// Request state hash
int32_t ExchangeTestContainer::RequestStateHash() {
  auto query = std::make_unique<
      exchange::core::common::api::reports::StateHashReportQuery>();

  // Serialize query using WriteMarshallable
  std::vector<uint8_t> queryBytesVec;
  exchange::core::common::VectorBytesOut queryBytesOut(queryBytesVec);
  query->WriteMarshallable(queryBytesOut);
  std::vector<uint8_t> queryBytes = queryBytesOut.GetData();

  auto future = api_->ProcessReportAny(
      query->GetReportTypeCode(), std::move(queryBytes), GetRandomTransferId());
  auto resultBytes = future.get();
  if (resultBytes.empty() || resultBytes[0].empty()) {
    throw std::runtime_error("Failed to get state hash");
  }

  // Deserialize result using constructor from BytesIn
  exchange::core::common::VectorBytesIn resultBytesIn(resultBytes[0]);
  exchange::core::common::api::reports::StateHashReportResult result(
      resultBytesIn);
  return result.GetStateHash();
}

// Helper function to convert OrderCommand to ApiCommand*
static exchange::core::common::api::ApiCommand *ConvertOrderCommandToApiCommand(
    const exchange::core::common::cmd::OrderCommand &cmd) {
  switch (cmd.command) {
  case exchange::core::common::cmd::OrderCommandType::PLACE_ORDER:
    return new exchange::core::common::api::ApiPlaceOrder(
        cmd.price, cmd.size, cmd.orderId, cmd.action, cmd.orderType, cmd.uid,
        cmd.symbol, cmd.userCookie, cmd.reserveBidPrice);
  case exchange::core::common::cmd::OrderCommandType::MOVE_ORDER:
    return new exchange::core::common::api::ApiMoveOrder(cmd.orderId, cmd.price,
                                                         cmd.uid, cmd.symbol);
  case exchange::core::common::cmd::OrderCommandType::CANCEL_ORDER:
    return new exchange::core::common::api::ApiCancelOrder(cmd.orderId, cmd.uid,
                                                           cmd.symbol);
  case exchange::core::common::cmd::OrderCommandType::REDUCE_ORDER:
    return new exchange::core::common::api::ApiReduceOrder(
        cmd.orderId, cmd.uid, cmd.symbol, cmd.size);
  default:
    return nullptr;
  }
}

// Load symbols, users and prefill orders
void ExchangeTestContainer::LoadSymbolsUsersAndPrefillOrders(
    const TestDataFutures &testDataFutures) {
  // Load symbols
  auto coreSymbolSpecifications = const_cast<TestDataFutures &>(testDataFutures)
                                      .coreSymbolSpecifications.get();
  AddSymbols(coreSymbolSpecifications);

  // Create accounts and deposit initial funds
  auto userAccounts =
      const_cast<TestDataFutures &>(testDataFutures).usersAccounts.get();
  UserAccountsInit(userAccounts);

  // Prefill orders - get fill commands from genResult
  auto genResult =
      const_cast<TestDataFutures &>(testDataFutures).genResult.get();
  auto fillCommandsFuture = genResult->GetApiCommandsFill();
  auto fillCommands = fillCommandsFuture.get();

  if (!fillCommands.empty()) {
    api_->SubmitCommandsSync(fillCommands);
    // Note: ApiCommand pointers are owned by ExchangeApi, don't delete them
  }
}

// Load symbols, users and prefill orders (no logging)
void ExchangeTestContainer::LoadSymbolsUsersAndPrefillOrdersNoLog(
    const TestDataFutures &testDataFutures) {
  // Same implementation as LoadSymbolsUsersAndPrefillOrders, but without
  // logging
  this->LoadSymbolsUsersAndPrefillOrders(testDataFutures);
}

// Set command consumer callback
void ExchangeTestContainer::SetConsumer(
    std::function<void(exchange::core::common::cmd::OrderCommand *, int64_t)>
        consumer) {
  consumer_ = std::move(consumer);
}

// Private helper methods
int32_t ExchangeTestContainer::GetRandomTransferId() {
  return uniqueIdCounterInt_.fetch_add(1) + 1;
}

int64_t ExchangeTestContainer::GetRandomTransactionId() {
  return uniqueIdCounterLong_.fetch_add(1) + 1;
}

void ExchangeTestContainer::CreateUserAccountsRegular(
    const std::vector<std::vector<bool>> &userCurrencies,
    const std::map<int32_t, int64_t> &amountPerAccount) {
  const int numUsers = static_cast<int>(userCurrencies.size()) - 1;

  std::vector<exchange::core::common::api::ApiCommand *> cmds;
  for (int uid = 1; uid <= numUsers; uid++) {
    cmds.push_back(new exchange::core::common::api::ApiAddUser(uid));
    for (size_t currencyIdx = 0; currencyIdx < userCurrencies[uid].size();
         currencyIdx++) {
      if (userCurrencies[uid][currencyIdx]) {
        int32_t currency = static_cast<int32_t>(currencyIdx);
        auto it = amountPerAccount.find(currency);
        if (it != amountPerAccount.end()) {
          cmds.push_back(new exchange::core::common::api::ApiAdjustUserBalance(
              uid, currency, it->second, GetRandomTransactionId()));
        }
      }
    }
  }

  api_->SubmitCommand(new exchange::core::common::api::ApiNop());
  api_->SubmitCommandsSync(cmds);
}

// Static helper methods
TestDataFutures ExchangeTestContainer::PrepareTestDataAsync(
    const util::TestDataParameters &parameters, int seed) {
  TestDataFutures futures;

  // Prepare symbols asynchronously
  // Convert std::future to std::shared_future to allow multiple get() calls
  futures.coreSymbolSpecifications =
      std::async(std::launch::async, [&parameters]() {
        return ExchangeTestContainer::GenerateRandomSymbols(
            parameters.numSymbols, parameters.currenciesAllowed,
            parameters.allowedSymbolTypes);
      }).share(); // Convert to shared_future

  // Prepare user accounts asynchronously
  futures.usersAccounts =
      std::async(std::launch::async, [&parameters]() {
        return UserCurrencyAccountsGenerator::GenerateUsers(
            parameters.numAccounts, parameters.currenciesAllowed);
      }).share(); // Convert to shared_future

  // Prepare test orders generator result
  // Wrap in shared_ptr because MultiSymbolGenResult contains non-copyable
  // OrderCommand
  futures.genResult =
      std::async(std::launch::async, [&parameters, seed]() {
        // Wait for symbols and users to be ready
        // Note: In a real async implementation, we'd combine the futures
        // For now, we'll generate symbols and users synchronously here
        auto symbols = ExchangeTestContainer::GenerateRandomSymbols(
            parameters.numSymbols, parameters.currenciesAllowed,
            parameters.allowedSymbolTypes);
        auto users = UserCurrencyAccountsGenerator::GenerateUsers(
            parameters.numAccounts, parameters.currenciesAllowed);

        // Create TestOrdersGeneratorConfig
        util::TestOrdersGeneratorConfig config;
        config.coreSymbolSpecifications = symbols;
        config.totalTransactionsNumber = parameters.totalTransactionsNumber;
        config.usersAccounts = users;
        config.targetOrderBookOrdersTotal =
            parameters.targetOrderBookOrdersTotal;
        config.seed = seed;
        config.avalancheIOC = parameters.avalancheIOC;
        config.preFillMode = parameters.preFillMode;

        // Generate multiple symbols and wrap in shared_ptr
        return std::make_shared<
            util::TestOrdersGenerator::MultiSymbolGenResult>(
            util::TestOrdersGenerator::GenerateMultipleSymbols(config));
      }).share(); // Convert to shared_future

  return futures;
}

std::vector<exchange::core::common::CoreSymbolSpecification>
ExchangeTestContainer::GenerateRandomSymbols(
    int num, const std::set<int32_t> &currenciesAllowed,
    util::AllowedSymbolTypes allowedSymbolTypes) {
  std::vector<exchange::core::common::CoreSymbolSpecification> result;
  std::mt19937 rng(1L); // Fixed seed matching Java version
  std::vector<int32_t> currencies(currenciesAllowed.begin(),
                                  currenciesAllowed.end());

  std::function<exchange::core::common::SymbolType()> symbolTypeSupplier;
  switch (allowedSymbolTypes) {
  case util::AllowedSymbolTypes::FUTURES_CONTRACT:
    symbolTypeSupplier = []() {
      return exchange::core::common::SymbolType::FUTURES_CONTRACT;
    };
    break;
  case util::AllowedSymbolTypes::CURRENCY_EXCHANGE_PAIR:
    symbolTypeSupplier = []() {
      return exchange::core::common::SymbolType::CURRENCY_EXCHANGE_PAIR;
    };
    break;
  case util::AllowedSymbolTypes::BOTH:
  default:
    symbolTypeSupplier = [&rng]() {
      return (rng() % 2 == 0)
                 ? exchange::core::common::SymbolType::FUTURES_CONTRACT
                 : exchange::core::common::SymbolType::CURRENCY_EXCHANGE_PAIR;
    };
    break;
  }

  for (int i = 0; i < num;) {
    int baseCurrency = currencies[rng() % currencies.size()];
    int quoteCurrency = currencies[rng() % currencies.size()];
    if (baseCurrency != quoteCurrency) {
      exchange::core::common::CoreSymbolSpecification symbol;
      symbol.symbolId = TestConstants::SYMBOL_AUTOGENERATED_RANGE_START + i;
      symbol.type = symbolTypeSupplier();
      symbol.baseCurrency = baseCurrency;
      symbol.quoteCurrency = quoteCurrency;
      symbol.baseScaleK = 100;
      symbol.quoteScaleK = 10;
      symbol.takerFee = rng() % 1000;
      symbol.makerFee = symbol.takerFee + (rng() % 500);
      result.push_back(symbol);
      i++;
    }
  }

  return result;
}

std::string ExchangeTestContainer::TimeBasedExchangeId() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%012llX",
           static_cast<unsigned long long>(ms));
  return std::string(buffer);
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
