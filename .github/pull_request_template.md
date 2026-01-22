## Description

_Describe what this PR does and why. Link to related issues if applicable._

Fixes # <!-- issue number(s) -->

---

## Type of Change

_Select all that apply:_

- [ ] 🐛 Bug fix - change that fixes an issue
- [ ] ✨ New feature - change that adds functionality
- [ ] ⚡ Performance - change that improves performance
- [ ] ♻️ Refactor - code change that neither fixes a bug nor adds a feature
- [ ] 📝 Documentation - documentation only changes
- [ ] 🧪 Tests - adding or updating tests
- [ ] 🔧 Infrastructure - CI/CD, build system, dependencies

---

## Impact Scope

_Select the highest applicable level:_

- [ ] 🔴 **Core Code** - Ring Buffer / Matching Engine / ART Tree / ObjectsPool
- [ ] 🟠 **Hot Path** - Performance-critical code path
- [ ] 🟡 **Public API** - Interfaces used by other modules
- [ ] 🟢 **Internal** - Internal implementation, limited impact

---

## Testing

### Test Coverage

- [ ] Unit tests added/updated
- [ ] Integration tests added/updated
- [ ] Tests not needed (explain below)

### Performance Benchmark

_Required for Hot Path / Performance changes:_

- [ ] ✅ No performance regression (< 5% change)
- [ ] ⬆️ Performance improved (attach benchmark results)
- [ ] ⬇️ Performance regressed (explain justification below)
- [ ] ➖ Not applicable (no hot path impact)

<details>
<summary>Benchmark Results (if applicable)</summary>

```
<!-- Paste benchmark output here -->
```

</details>

---

## Backward Compatibility

- [ ] ✅ Backward compatible
- [ ] ⚠️ Breaking change (describe migration path below)
- [ ] ❓ Unknown (needs review)

---

## Checklist

_Confirm the following before requesting review:_

### Code Quality
- [ ] CI passes (clang-format, clang-tidy, cppcheck, tests)
- [ ] No compiler warnings introduced
- [ ] Code follows project naming conventions

### Memory & Concurrency (C++ specific)
- [ ] No memory leaks (RAII, proper ownership)
- [ ] No use-after-free / dangling pointers
- [ ] Memory order correct for atomics (acquire-release preferred over seq_cst)
- [ ] No data races on shared data

### Hot Path (if applicable)
- [ ] No unnecessary heap allocations
- [ ] No unnecessary copies (use move semantics / string_view)
- [ ] No exceptions in hot path (use `std::expected` if needed)
- [ ] No virtual function calls in hot path (use CRTP if needed)

### Documentation
- [ ] Code comments explain "why", not just "what"
- [ ] Public APIs have documentation comments
- [ ] README/docs updated if needed

---

## Additional Notes

<!-- Any additional context, screenshots, or information for reviewers -->
