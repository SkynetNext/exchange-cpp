#!/bin/bash

# 进入 Java 项目目录
cd reference/exchange-core

# 注意：pom.xml 中的 delombok 插件已被注释掉，以避免 Java 版本兼容性问题
# 如果将来需要恢复 delombok，请取消注释 pom.xml 中的相应部分

# 方法1: 运行特定的测试方法（推荐）
mvn test -Dtest=OrderBookDirectImplExchangeTest#multipleCommandsCompareTest

# 方法2: 运行整个测试类
# mvn test -Dtest=OrderBookDirectImplExchangeTest

# 方法3: 运行所有测试
# mvn test

# 方法4: 运行测试并显示详细输出（调试用）
# mvn test -Dtest=OrderBookDirectImplExchangeTest#multipleCommandsCompareTest -X

