# `__declspec(property)` Cross-Platform Compatibility Test Suite

This repo contains a comprehensive test suite for verifying `__declspec(property)` behavior across **Windows (MSVC)**, **macOS (Apple Clang)**, **Linux (Clang)**, and **Android (NDK Clang)**.

## Background

`__declspec(property)` is a Microsoft C++ extension that creates "virtual data members" — the compiler rewrites `obj.Prop` into `obj.GetProp()` / `obj.SetProp()` calls. It enables C#-like property syntax in C++.

**Use case**: UE (Unreal Engine) projects using code generation (e.g., Mirror sync system) rely on `__declspec(property)` for clean syntax sugar over generated Getter/Setter/UPROPERTY boilerplate.

## Compiler Support Matrix

| Platform | Compiler | Min Version | Flag Required | Status |
|----------|----------|-------------|---------------|--------|
| **Windows** | MSVC | VS 6.0 (1998) | None (native) | ✅ |
| **macOS** | Apple Clang | Clang 3.3 / Xcode 5 (2013) | `-fms-extensions` | ✅ |
| **iOS** | Apple Clang | Same as macOS | `-fms-extensions` | ✅ |
| **Linux** | Clang | 3.3 (2013) | `-fms-extensions` or `-fdeclspec` | ✅ |
| **Android** | NDK Clang | NDK r10 / Clang 3.4 (2014) | `-fms-extensions` | ✅ |
| **Linux** | GCC | — | — | ❌ Not supported |

> **Note**: UE's build system (`UBT`) enables `-fms-extensions` by default for all Clang targets since UE 4.20+.

## Test Cases (25 total)

| # | Case | What It Tests | Risk Level |
|---|------|---------------|------------|
| 1 | `basic_pod_get_set` | POD type basic read/write via property | 🟢 Safe |
| 2 | `readonly_property` | Read-only property (get only, no put) | 🟢 Safe |
| 3 | `struct_whole_assign` | USTRUCT whole-object assignment triggers Setter | 🟢 Safe |
| 4 | `struct_submember_modify` | ⚠️ Sub-member modification vs Setter trigger | 🔴 Key risk |
| 5 | `pointer_property` | Pointer type get/put | 🟢 Safe |
| 6 | `pointer_deref_modify` | Deref through property pointer, modify target | 🟡 Subtle |
| 7 | `inheritance_access` | Child class accessing parent's property | 🟢 Safe |
| 8 | `virtual_getter_setter` | Virtual Getter/Setter polymorphism | 🟢 Safe |
| 9 | `auto_decltype_deduction` | `auto` / `decltype` type deduction | 🟡 Compiler-dependent |
| 10 | `template_usage` | Property used inside templates | 🟡 Subtle |
| 11 | `compound_assignment` | `+=`, `-=`, `*=` compound ops | 🟡 Expands to get+put |
| 12 | `increment_decrement` | `++` / `--` prefix and postfix | 🟡 Subtle |
| 13 | `address_of_property` | `&obj.Property` behavior | 🔴 Compiler-divergent |
| 14 | `multiple_properties` | Multiple properties in one class | 🟢 Safe |
| 15 | `name_conflict` | Property name vs method name | 🟡 Subtle |
| 16 | `ternary_operator` | Ternary `?:` with properties | 🟢 Safe |
| 17 | `lambda_capture` | Lambda value/reference capture | 🟢 Safe |
| 18 | `sizeof_property` | `sizeof(obj.Property)` — should NOT call Getter | 🟢 Safe |
| 19 | `function_argument` | Pass property as value / const ref / non-const ref | 🔴 Divergent |
| 20 | `enum_property` | Enum class type + switch | 🟢 Safe |
| 21 | `bool_property` | Bool property in `if` / logical ops | 🟢 Safe |
| 22 | `int64_with_hook` | int64 with change hook + same-value skip | 🟢 Safe |
| 23 | `cast_interaction` | `static_cast` / `reinterpret_cast` on property | 🟢 Safe |
| 24 | `initializer_context` | Property in initializer list `{obj.Val, ...}` | 🟢 Safe |
| 25 | `loop_condition` | Property in `for` loop condition | 🟢 Safe |

## Key Findings

### ✅ Safe across all platforms
- Basic get/put, inheritance, virtual dispatch, templates, compound assignment, `++/--`, lambda capture, `sizeof`, casts — all work correctly.

### ⚠️ Known risks (semantic, not compiler)
1. **Sub-member modification bypasses Setter**: `obj.Position.X = 5` does NOT trigger `SetPosition()`. The `get` returns `const ref`, so this either silently fails or triggers a const-correctness error depending on the Getter signature.
2. **`&obj.Property`**: Taking the address of a property is undefined/divergent across compilers. Use an explicit `AccessXxx()` method that returns `T&` instead.
3. **Non-const reference parameter**: Passing a property to `void fn(int&)` may compile on MSVC but fail on Clang.

### 💡 Runtime behavior
`__declspec(property)` is **purely compile-time syntax sugar**. The compiler rewrites property access into function calls. The generated machine code is identical to manually calling `GetX()` / `SetX()`. There is **no runtime overhead, no vtable, no RTTI, no platform-specific behavior**.

## How to Run

### Local (macOS/Linux with Clang)
```bash
clang++ -std=c++17 -Wall -Wextra -fms-extensions -o test declspec_property_test.cpp && ./test
```

### Local (Windows with MSVC)
```cmd
cl /EHsc /std:c++17 /W4 /Fe:test.exe declspec_property_test.cpp && test.exe
```

### Local (Android NDK cross-compile)
```bash
$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions -o test_arm64 declspec_property_test.cpp
# Push to device: adb push test_arm64 /data/local/tmp/ && adb shell /data/local/tmp/test_arm64
```

### CI (GitHub Actions) — Automated
Push to this repo and GitHub Actions will automatically run tests on:
- ✅ Windows (MSVC latest)
- ✅ Ubuntu (Clang 14 / 16 / 18)
- ✅ Ubuntu GCC (negative test — expected compile failure)
- ✅ macOS (Apple Clang latest)
- ✅ Android NDK r27c (ARM64 / ARM32 / x86_64 cross-compile)

## License

MIT
