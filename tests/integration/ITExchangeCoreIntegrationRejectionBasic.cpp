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

#include "ITExchangeCoreIntegrationRejectionBasic.h"
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gtest/gtest.h>
#include "../util/TestConstants.h"
#include "ITExchangeCoreIntegrationRejection.h"

using namespace exchange::core::common;
using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

ITExchangeCoreIntegrationRejectionBasic::ITExchangeCoreIntegrationRejectionBasic()
  : ITExchangeCoreIntegrationRejection() {}

exchange::core::common::config::PerformanceConfiguration
ITExchangeCoreIntegrationRejectionBasic::GetPerformanceConfiguration() {
  // Note: baseBuilder() equivalent - use Default()
  return exchange::core::common::config::PerformanceConfiguration::Default();
}

// -------------------------- buy no rejection tests
// -----------------------------

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionMarginGtc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::GTC,
               RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionExchangeGtc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::GTC,
               RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionExchangeIoc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::IOC,
               RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionMarginIoc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::IOC,
               RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionExchangeFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
               RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyNoRejectionMarginFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
               RejectionCause::NO_REJECTION);
}

// -------------------------- buy with rejection tests
// -----------------------------

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithRejectionMarginGtc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::GTC,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithRejectionExchangeGtc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::GTC,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithRejectionExchangeIoc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::IOC,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithRejectionMarginIoc) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::IOC,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithSizeRejectionExchangeFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithSizeRejectionMarginFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
               RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithBudgetRejectionExchangeFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
               RejectionCause::REJECTION_BY_BUDGET);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiBuyWithBudgetRejectionMarginFokB) {
  TestMultiBuy(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
               RejectionCause::REJECTION_BY_BUDGET);
}

// -------------------------- sell no rejection tests
// -----------------------------

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionMarginGtc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::GTC,
                RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionExchangeGtc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::GTC,
                RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionMarginIoc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::IOC,
                RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionExchangeIoc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::IOC,
                RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionMarginFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
                RejectionCause::NO_REJECTION);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellNoRejectionExchangeFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
                RejectionCause::NO_REJECTION);
}

// -------------------------- sell with rejection tests
// -----------------------------

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithRejectionMarginGtc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::GTC,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithRejectionExchangeGtc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::GTC,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithRejectionMarginIoc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::IOC,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithRejectionExchangeIoc) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::IOC,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithSizeRejectionMarginFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithSizeRejectionExchangeFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
                RejectionCause::REJECTION_BY_SIZE);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithExpectationRejectionMarginFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_USD_JPY(), OrderType::FOK_BUDGET,
                RejectionCause::REJECTION_BY_BUDGET);
}

TEST_F(ITExchangeCoreIntegrationRejectionBasic, TestMultiSellWithExpectationRejectionExchangeFokB) {
  TestMultiSell(TestConstants::SYMBOLSPECFEE_XBT_LTC(), OrderType::FOK_BUDGET,
                RejectionCause::REJECTION_BY_BUDGET);
}

}  // namespace integration
}  // namespace tests
}  // namespace core
}  // namespace exchange
