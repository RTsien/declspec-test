# `__declspec(property)` Cross-Platform Compiler Compatibility Test Suite

English | [简体中文](README.zh-CN.md)

[![CI](https://github.com/RTsien/declspec-test/actions/workflows/test.yml/badge.svg)](https://github.com/RTsien/declspec-test/actions/workflows/test.yml)

> 📊 **Latest test report:** [View the GitHub Actions summary](https://github.com/RTsien/declspec-test/actions/runs/24657048908)

This repository provides a comprehensive test suite for the compile-time and runtime behavior of `__declspec(property)` on **Windows (MSVC)**, **macOS (Apple Clang)**, **Linux (Clang)**, and **Android (NDK Clang)**.

## Background

`__declspec(property)` is a Microsoft C++ extension for creating "virtual data members." The compiler rewrites `obj.Prop` as calls to `obj.GetProp()` or `obj.SetProp()`, providing property syntax similar to C#.

**Use case:** Code-generation systems in Unreal Engine projects, such as the Mirror synchronization framework, use `__declspec(property)` to provide concise syntax for generated getters, setters, and UPROPERTY fields.

## Compiler support matrix

| Platform | Compiler | Minimum version | Required option | Status |
| --- | --- | --- | --- | --- |
| **Windows** | MSVC | VS 6.0 (1998) | None (natively supported) | ✅ |
| **macOS** | Apple Clang | Clang 3.3 / Xcode 5 (2013) | `-fms-extensions` | ✅ |
| **iOS** | Apple Clang | Same as macOS | `-fms-extensions` | ✅ |
| **Linux** | Clang | 3.3 (2013) | `-fms-extensions` or `-fdeclspec` | ✅ |
| **Android** | NDK Clang | NDK r10 / Clang 3.4 (2014) | `-fms-extensions` | ✅ |
| **Linux** | GCC | — | — | ❌ Unsupported |

> **Note:** Unreal Build Tool (UBT) enables `-fms-extensions` by default for all Clang targets starting with UE 4.20.

## Test cases (25 total)

| # | Test name | Coverage | Risk level |
| --- | --- | --- | --- |
| 1 | `basic_pod_get_set` | Basic POD reads and writes | 🟢 Safe |
| 2 | `readonly_property` | Read-only property (`get` without `put`) | 🟢 Safe |
| 3 | `struct_whole_assign` | Whether whole-struct assignment invokes the setter | 🟢 Safe |
| 4 | `struct_submember_modify` | Whether modifying a submember invokes the setter | 🔴 Critical risk |
| 5 | `pointer_property` | Pointer reads and writes | 🟢 Safe |
| 6 | `pointer_deref_modify` | Modifying a target through a property pointer | 🟡 Use caution |
| 7 | `inheritance_access` | Accessing a base-class property from a subclass | 🟢 Safe |
| 8 | `virtual_getter_setter` | Polymorphic calls to virtual getters and setters | 🟢 Safe |
| 9 | `auto_decltype_deduction` | `auto` / `decltype` type deduction | 🟡 Compiler-dependent |
| 10 | `template_usage` | Properties in templates | 🟡 Use caution |
| 11 | `compound_assignment` | Compound assignment with `+=`, `-=`, and `*=` | 🟡 Expands to get + put |
| 12 | `increment_decrement` | Prefix and postfix `++` / `--` | 🟡 Use caution |
| 13 | `address_of_property` | Address-of behavior for `&obj.Property` | 🔴 Differs by compiler |
| 14 | `multiple_properties` | Multiple properties in one class | 🟢 Safe |
| 15 | `name_conflict` | Conflicts between property and method names | 🟡 Use caution |
| 16 | `ternary_operator` | Interaction with the ternary operator `?:` | 🟢 Safe |
| 17 | `lambda_capture` | Lambda capture by value and by reference | 🟢 Safe |
| 18 | `sizeof_property` | Whether `sizeof(obj.Property)` invokes the getter | 🟢 Safe |
| 19 | `function_argument` | Passing a property by value, const reference, or non-const reference | 🔴 Differs by compiler |
| 20 | `enum_property` | Enum properties and `switch` | 🟢 Safe |
| 21 | `bool_property` | Boolean properties with `if` and logical operators | 🟢 Safe |
| 22 | `int64_with_hook` | `int64` with a change callback and unchanged-value skipping | 🟢 Safe |
| 23 | `cast_interaction` | Interaction with `static_cast` and `reinterpret_cast` | 🟢 Safe |
| 24 | `initializer_context` | Properties in initializer lists such as `{obj.Val, ...}` | 🟢 Safe |
| 25 | `loop_condition` | Properties in loop conditions | 🟢 Safe |

## Key findings

### ✅ Safe across all tested platforms

Basic get/put operations, inheritance, virtual dispatch, templates, compound assignments, `++` / `--`, lambda captures, `sizeof`, and casts all behave as expected.

### ⚠️ Known semantic risks (not compiler bugs)

1. **Submember modification bypasses the setter:** `obj.Position.X = 5` does **not** invoke `SetPosition()`. When `get` returns a const reference, modifying a submember either fails to compile because of const protection or silently fails. Use an explicit `AccessXxx()` method instead.
2. **Taking the address with `&obj.Property`:** Behavior differs between compilers and is undefined. Use an `AccessXxx()` method that returns `T&` instead.
3. **Passing by non-const reference:** Passing a property to `void fn(int&)` may compile with MSVC but fail with Clang.

### 💡 Runtime behavior

`__declspec(property)` is **pure compile-time syntax sugar**. The compiler rewrites property access as function calls, producing the same machine code as handwritten `GetX()` / `SetX()` calls. It adds **no runtime overhead, vtable, RTTI, or platform-specific runtime behavior**.

## Usage

### Local build: macOS / Linux with Clang

```bash
clang++ -std=c++17 -Wall -Wextra -fms-extensions -o test declspec_property_test.cpp && ./test
```

### Local build: Windows with MSVC

```cmd
cl /EHsc /std:c++17 /W4 /Fe:test.exe declspec_property_test.cpp && test.exe
```

### Android NDK cross-compilation

```bash
# Statically linked ARM64 build (bionic_tls_align.S fixes TLS alignment)
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions -static \
  -o test_arm64 declspec_property_test.cpp bionic_tls_align.S
# Run on a device: adb push test_arm64 /data/local/tmp/ && adb shell /data/local/tmp/test_arm64

# Dynamically linked build (no TLS fix required)
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions \
  -o test_arm64 declspec_property_test.cpp
```

### GitHub Actions CI

Every push triggers parallel builds and tests in the following environments:

| Target platform | Runner | Compiler | Verification |
| --- | --- | --- | --- |
| Windows x86_64 | windows-2022 | MSVC (VS 2022) | Native build and run |
| Linux x86_64 | ubuntu-22.04 | **Clang 11** | Native build and run |
| Linux x86_64 | ubuntu-22.04 | **Clang 13** | Native build and run |
| Linux x86_64 | ubuntu-latest | Clang 14 | Native build and run |
| Linux x86_64 | ubuntu-latest | Clang 16 | Native build and run |
| Linux x86_64 | ubuntu-latest | Clang 18 | Native build and run |
| Linux x86_64 | ubuntu-latest | GCC | Negative test (expected compilation failure) |
| macOS arm64 | macos-14 | Apple Clang | Native build and run |
| macOS x86_64 | macos-14 | Apple Clang | Cross-build and run with Rosetta 2 |
| iOS arm64 | macos-14 | Xcode Clang | Build and run in iPhone Simulator |
| Android ARM64 | ubuntu-22.04 | **NDK r21e (Clang 9)** | Cross-build and run in an ARM64 Docker container |
| Android x86_64 | ubuntu-22.04 | **NDK r21e (Clang 9)** | Cross-build and run in a KVM emulator |
| Android ARM32 | ubuntu-22.04 | **NDK r21e (Clang 9)** | Cross-build only |
| Android x86_64 | ubuntu-latest | NDK r27c (Clang 18) | Cross-build and run in a KVM emulator |
| Android ARM64 | ubuntu-latest | NDK r27c (Clang 18) | Cross-build and run in an ARM64 Docker container |
| Android ARM32 | ubuntu-latest | NDK r27c (Clang 18) | Cross-build only |

> **Version coverage:** Clang 11 (2020) and NDK r21e (Clang 9, 2020) are the oldest versions that can be tested reliably in CI and are close to the minimum supported versions documented above (Clang 3.3 / NDK r10). GitHub Actions no longer offers the `macos-13` (Intel) and `windows-2019` (VS 2019) runners, so earlier compiler binaries are not directly available. However, the `__declspec(property)` compiler frontend support introduced in Clang 3.3 and MSVC VS 6.0 has had no known incompatible changes.

#### Special verification methods

**macOS x86_64 — Rosetta 2**

Because the `macos-13` Intel runner has been retired and is extremely slow or impossible to allocate, the test cross-compiles on `macos-14` (Apple Silicon) with `-target x86_64-apple-macos13`, then runs through Rosetta 2 instruction translation. This is behaviorally equivalent to running natively on Intel.

**iOS arm64 — iPhone Simulator**

The test builds with `xcrun --sdk iphonesimulator clang++` and runs through `xcrun simctl spawn` in iPhone Simulator. On Apple Silicon, the simulator executes ARM64 code natively rather than through instruction translation, matching physical-device CPU behavior.

**Android x86_64 — KVM emulator**

The Android Emulator (API 30) runs with KVM hardware virtualization on an Ubuntu runner, providing near-native performance.

**Android ARM64 — Docker multiarch + QEMU binfmt**

A statically linked Bionic binary cross-compiled with the NDK cannot run directly through `qemu-user-static`, because Bionic requires the ARM64 TLS segment alignment to be at least 64 bytes. The test works around this by:

1. Linking `bionic_tls_align.S`, whose assembly `.p2align 6` directive forces the `.tdata` section to 64-byte alignment. A C-level `__attribute__((aligned(64)))` does not propagate to the PT_TLS segment alignment.
2. Registering a binfmt handler with `docker/setup-qemu-action` and running the original NDK-compiled binary in an `arm64v8/ubuntu` container.

**Android ARM32 — build only**

The NDK cross-compilation completes without errors or warnings. ARM32 and ARM64 use the same Clang `-fms-extensions` frontend, so their `__declspec(property)` semantics are identical.

### Bionic TLS alignment fix

Static Android NDK linking includes TLS data from Bionic's `libc.a`, but the linker produces a PT_TLS segment aligned to only 8 bytes by default. Bionic's `__libc_init` validates this alignment at startup: ARM64 requires at least 64 bytes and ARM32 requires at least 32 bytes. The process aborts if the requirement is not met.

This repository provides `bionic_tls_align.S` as a general-purpose fix:

```bash
# Add this file to the link command
aarch64-linux-android26-clang++ -static -o test test.cpp bionic_tls_align.S
```

Note that Clang's `__thread __attribute__((aligned(64)))` does **not** affect the PT_TLS segment's `p_align` field. TLS segment alignment is determined only by the `.tdata` section alignment and can only be controlled with an assembly `.p2align` directive.

## License

MIT
