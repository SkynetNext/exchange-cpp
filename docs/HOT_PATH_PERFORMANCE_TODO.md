# Hot Path Performance Issues - TODO

This document lists performance issues in hot paths that need optimization.

---

## Hot Path Overview

Order processing pipeline:
```
Gateway → Grouping (G) → Risk Pre-hold (R1) → Matching Engine (ME) → Risk Release (R2) → Results (E)
```

All orders pass through these paths. At 4.8M TPS, these issues are amplified.

---

## TODO: Exception Handling Issues

### 1. TwoStepMasterProcessor (R1/R2 Risk Engine)

**Location**: `src/exchange/core/processors/TwoStepMasterProcessor.cpp`

**Issue**: 
- Inner try-catch (lines 118-130) wrapping `eventHandler_->OnEvent()`
- Outer try-catch (lines 150-175) catching all exceptions

**Impact**: R1 and R2 hot paths, every order passes through

**Cost**: Exception handling ~1.7-3.3μs (5K-10K cycles)

**Status**: ⚠️ **TODO** - Aligned with Java version (removed inner try-catch), but outer try-catch remains

---

### 2. RiskEngine::PostProcessCommand (R2 Risk Release)

**Location**: `src/exchange/core/processors/RiskEngine.cpp:278`

**Issue**: 
```cpp
if (spec == nullptr) {
    throw std::runtime_error("Symbol not found: " + std::to_string(symbol));
}
```

**Impact**: R2 hot path, called for every executed order

**Cost**: If thrown, exception handling ~1.7-3.3μs

**Status**: ⚠️ **TODO** - Replace with error code return

---

## TODO: Virtual Function Issues

### 1. IOrderBook Interface (Critical)

**Location**: `include/exchange/core/orderbook/IOrderBook.h`

**Issue**: 
- Multiple virtual functions called frequently in matching engine hot path:
  - `NewOrder()` - most frequent
  - `CancelOrder()`, `ReduceOrder()`, `MoveOrder()`
  - `GetL2MarketDataSnapshot()`

**Call Path**:
```
MatchingEngineRouter::ProcessMatchingCommand()
  → IOrderBook::ProcessCommand()
    → orderBook->NewOrder() / orderBook->CancelOrder() (virtual calls)
```

**Impact**: Matching engine core path, every order calls these

**Cost**: Virtual function call ~10-20ns (30-60 cycles), 2-3x slower than direct call

**Status**: ⚠️ **TODO** - Consider CRTP or template-based optimization

**Note**: Java version uses JIT devirtualization to eliminate interface call overhead. C++ needs compile-time optimization.

---

## Performance Impact Summary

| Issue | Severity | Impact Path | Cost |
|-------|----------|-------------|------|
| IOrderBook virtual calls | ⭐⭐⭐ Critical | ME (core) | 10-20ns × call frequency |
| TwoStepMasterProcessor exceptions | ⭐⭐ Medium | R1/R2 | 1.7-3.3μs (if thrown) |
| RiskEngine::PostProcessCommand | ⭐⭐ Medium | R2 | 1.7-3.3μs (if thrown) |

---

## Optimization Options (Not Implemented)

### Virtual Function Optimization

1. **CRTP + Template MatchingEngineRouter**
   - Make `MatchingEngineRouter` a class template with `OrderBookImplType` as template parameter
   - Store concrete types instead of base pointers
   - Zero virtual function overhead in hot path

2. **Switch-case Dispatch**
   - Use `GetImplementationType()` to dispatch in `ProcessCommand()`
   - Minimal changes, eliminates virtual calls

3. **Function Pointer Table**
   - Initialize function pointers at startup
   - Avoids vtable lookup but still indirect call

### Exception Handling Optimization

1. **Replace exceptions with error codes** in hot paths
2. **Move exception handling** to non-hot paths (initialization, configuration)

---

## Reference Data

From `LATENCY_REFERENCE_DATA.md`:
- **C++ exception throw+catch**: 1.7-3.3μs (5K-10K cycles)
- **C++ virtual function call**: 10-20ns (30-60 cycles)
- **C direct function call**: 5-10ns (15-30 cycles)

At 4.8M TPS, these costs are significantly amplified.

---

**Version**: 2.0  
**Last Updated**: 2025-01-02  
**Status**: TODO - Not implemented  
**Maintainer**: Exchange-CPP Team

