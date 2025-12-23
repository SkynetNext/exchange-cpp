# 缺失功能清单

本文档列出 C++ 版本中缺失的、但在 Java 版本中存在的功能。

## 已完成的改进 ✅

1. **MatchingEngineRouter**:
   - ✅ 添加 `BinaryCommandsProcessor` 集成
   - ✅ 完善命令处理逻辑（所有命令类型）
   - ✅ 添加配置参数支持
   - ✅ 完善 `Reset()` 方法
   - ✅ 添加 `GetShardMask()` 方法
   - ✅ 正确初始化 `logDebug_`（使用 `LoggingConfiguration::Contains`）

2. **RiskEngine**:
   - ✅ 添加 `ObjectsPool` 支持
   - ✅ 验证 `Reset()` 方法完整性
   - ✅ 正确初始化 `logDebug_`（使用 `LoggingConfiguration::Contains`）

3. **UserProfileService**:
   - ✅ 修复 `Reset()` 方法的内存泄漏问题
   - ✅ 使用 `HashingUtils::StateHash()` 与 Java 版本保持一致

4. **SymbolSpecificationProvider**:
   - ✅ 使用 `HashingUtils::StateHash()` 与 Java 版本保持一致

## 待完成的功能 ⚠️

### 高优先级 (P0)

#### 1. 序列化功能（writeMarshallable）
**影响范围**: 多个类需要实现序列化接口

**需要实现的类**:
- `MatchingEngineRouter` - 序列化 `shardId`, `shardMask`, `binaryCommandsProcessor`, `orderBooks`
- `RiskEngine` - 序列化状态数据
- `UserProfileService` - 序列化 `userProfiles`
- `SymbolSpecificationProvider` - 序列化 `symbolSpecs`
- `BinaryCommandsProcessor` - 序列化 `incomingData`
- `Order` - 序列化订单数据
- `UserProfile` - 序列化用户配置数据
- `CoreSymbolSpecification` - 序列化交易对规格

**Java 参考**:
- `MatchingEngineRouter.writeMarshallable()` (line 274-280)
- `RiskEngine.writeMarshallable()` 
- `UserProfileService.writeMarshallable()`
- `SymbolSpecificationProvider.writeMarshallable()` (line 83-86)
- `BinaryCommandsProcessor.writeMarshallable()` (line 198-202)

**实现方式**:
- 需要定义序列化接口（类似 Java 的 `WriteBytesMarshallable`）
- 使用 `SerializationUtils` 辅助方法
- 参考 Java 版本的序列化格式

#### 2. 从快照加载状态功能
**影响范围**: `MatchingEngineRouter`, `RiskEngine`

**需要实现**:
- `ISerializationProcessor::canLoadFromSnapshot()` 检查
- `ISerializationProcessor::loadData()` 加载数据
- 从 `BytesIn` 反序列化构造函数
- 验证 `shardId` 和 `shardMask` 匹配

**Java 参考**:
- `MatchingEngineRouter` 构造函数 (line 120-161)
- `RiskEngine` 构造函数 (line 114-180)

**实现方式**:
- 在构造函数中添加快照加载逻辑
- 使用 lambda 函数作为反序列化回调
- 创建 `DeserializedData` 结构体存储反序列化结果

#### 3. BinaryCommandsProcessor 反序列化构造函数
**需要实现**:
- 从 `BytesIn` 构造 `BinaryCommandsProcessor`
- 反序列化 `incomingData` (TransferRecord map)

**Java 参考**:
- `BinaryCommandsProcessor` 构造函数 (line 81-93)

#### 4. IOrderBook::create 静态方法
**需要实现**:
- 从 `BytesIn` 创建 `IOrderBook` 实例的静态工厂方法
- 根据实现类型（NAIVE/DIRECT）创建对应实例

**Java 参考**:
- `IOrderBook.create()` 方法

### 中优先级 (P1)

#### 5. exchangeId 和 folder 字段
**影响范围**: `MatchingEngineRouter`, `RiskEngine`

**需要添加**:
- `exchangeId` (String/std::string) - 交易所 ID
- `folder` (Path/std::filesystem::path) - 序列化文件夹路径

**Java 参考**:
- `MatchingEngineRouter` (line 72-73, 98-99)
- `RiskEngine` (line 78-79, 100-101)

**注意**: 这些字段主要用于序列化，如果暂时不实现序列化功能，可以标记为 TODO

### 低优先级 (P2)

#### 6. 其他序列化相关
- `Order` 从 `BytesIn` 构造
- `UserProfile` 从 `BytesIn` 构造
- `CoreSymbolSpecification` 从 `BytesIn` 构造
- `SymbolPositionRecord` 序列化支持

## 实现建议

### 序列化接口设计

建议创建一个 C++ 版本的序列化接口：

```cpp
class WriteBytesMarshallable {
public:
  virtual ~WriteBytesMarshallable() = default;
  virtual void WriteMarshallable(BytesOut &bytes) = 0;
};

class BytesMarshallable : public WriteBytesMarshallable {
public:
  BytesMarshallable(BytesIn &bytes); // 反序列化构造函数
};
```

### 序列化工具类

`SerializationUtils` 需要实现：
- `marshallIntHashMap()` - 序列化 int->Object map
- `marshallLongHashMap()` - 序列化 long->Object map
- `marshallIntLongHashMap()` - 序列化 int->long map
- `readIntHashMap()` - 反序列化 int->Object map
- `readLongHashMap()` - 反序列化 long->Object map
- `readIntLongHashMap()` - 反序列化 int->long map
- `marshallLongArray()` - 序列化 long 数组
- `readLongArray()` - 反序列化 long 数组

## 当前完成度

- **核心业务逻辑**: ✅ 95%
- **配置和初始化**: ✅ 95%
- **序列化/持久化**: ⚠️ 30%
- **从快照恢复**: ⚠️ 0%

## 下一步行动

1. **立即可以完成**:
   - ✅ 添加 `GetShardMask()` 方法（已完成）
   - ✅ 完善 `logDebug_` 初始化（已完成）

2. **需要序列化框架支持**:
   - 实现序列化接口
   - 实现 `SerializationUtils` 辅助方法
   - 实现各个类的 `writeMarshallable` 方法

3. **需要完整序列化系统**:
   - 实现从快照加载功能
   - 实现反序列化构造函数

---

*最后更新: 2024-12-23*

