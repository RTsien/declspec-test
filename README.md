# `__declspec(property)` 跨平台编译兼容性测试套件

[![CI](https://github.com/RTsien/declspec-test/actions/workflows/test.yml/badge.svg)](https://github.com/RTsien/declspec-test/actions/workflows/test.yml)

> 📊 **最新测试报告**：[查看 GitHub Actions Summary](https://github.com/RTsien/declspec-test/actions/runs/24650957853)

本仓库包含一套完整的测试用例，用于验证 `__declspec(property)` 在 **Windows (MSVC)**、**macOS (Apple Clang)**、**Linux (Clang)**、**Android (NDK Clang)** 下的编译和运行行为。

## 背景

`__declspec(property)` 是微软 C++ 扩展，用于创建"虚拟数据成员"——编译器会将 `obj.Prop` 自动重写为 `obj.GetProp()` / `obj.SetProp()` 调用，实现类似 C# 的属性语法。

**应用场景**：UE（虚幻引擎）项目中的代码生成系统（如 Mirror 同步框架）依赖 `__declspec(property)` 为自动生成的 Getter/Setter/UPROPERTY 提供简洁的语法糖。

## 编译器支持矩阵

| 平台 | 编译器 | 最低版本 | 所需编译选项 | 状态 |
|------|--------|---------|-------------|------|
| **Windows** | MSVC | VS 6.0 (1998) | 无（原生支持） | ✅ |
| **macOS** | Apple Clang | Clang 3.3 / Xcode 5 (2013) | `-fms-extensions` | ✅ |
| **iOS** | Apple Clang | 同 macOS | `-fms-extensions` | ✅ |
| **Linux** | Clang | 3.3 (2013) | `-fms-extensions` 或 `-fdeclspec` | ✅ |
| **Android** | NDK Clang | NDK r10 / Clang 3.4 (2014) | `-fms-extensions` | ✅ |
| **Linux** | GCC | — | — | ❌ 不支持 |

> **注**：UE 构建系统 (UBT) 从 UE 4.20+ 起默认为所有 Clang 目标启用 `-fms-extensions`。

## 测试用例（共 25 个）

| # | 用例名 | 测试内容 | 风险等级 |
|---|--------|---------|---------|
| 1 | `basic_pod_get_set` | POD 类型基本读写 | 🟢 安全 |
| 2 | `readonly_property` | 只读属性（仅 get，无 put） | 🟢 安全 |
| 3 | `struct_whole_assign` | 结构体整体赋值是否触发 Setter | 🟢 安全 |
| 4 | `struct_submember_modify` | ⚠️ 子成员修改是否触发 Setter | 🔴 关键风险 |
| 5 | `pointer_property` | 指针类型读写 | 🟢 安全 |
| 6 | `pointer_deref_modify` | 通过属性指针解引用修改目标对象 | 🟡 需注意 |
| 7 | `inheritance_access` | 子类访问父类属性 | 🟢 安全 |
| 8 | `virtual_getter_setter` | 虚函数 Getter/Setter 多态调用 | 🟢 安全 |
| 9 | `auto_decltype_deduction` | `auto` / `decltype` 类型推导 | 🟡 编译器相关 |
| 10 | `template_usage` | 模板中使用属性 | 🟡 需注意 |
| 11 | `compound_assignment` | `+=`, `-=`, `*=` 复合赋值 | 🟡 展开为 get+put |
| 12 | `increment_decrement` | `++` / `--` 前置和后置 | 🟡 需注意 |
| 13 | `address_of_property` | `&obj.Property` 取地址行为 | 🔴 编译器间有差异 |
| 14 | `multiple_properties` | 同一个类中多个属性 | 🟢 安全 |
| 15 | `name_conflict` | 属性名与方法名冲突 | 🟡 需注意 |
| 16 | `ternary_operator` | 三元运算符 `?:` 交互 | 🟢 安全 |
| 17 | `lambda_capture` | Lambda 值捕获 / 引用捕获 | 🟢 安全 |
| 18 | `sizeof_property` | `sizeof(obj.Property)` 是否触发 Getter | 🟢 安全 |
| 19 | `function_argument` | 属性作为函数参数（值传递 / const引用 / 非const引用） | 🔴 编译器间有差异 |
| 20 | `enum_property` | 枚举类型 + switch | 🟢 安全 |
| 21 | `bool_property` | bool 属性与 if / 逻辑运算 | 🟢 安全 |
| 22 | `int64_with_hook` | int64 带变更回调 + 相同值跳过 | 🟢 安全 |
| 23 | `cast_interaction` | `static_cast` / `reinterpret_cast` 交互 | 🟢 安全 |
| 24 | `initializer_context` | 初始化列表中使用属性 `{obj.Val, ...}` | 🟢 安全 |
| 25 | `loop_condition` | 循环条件中使用属性 | 🟢 安全 |

## 核心结论

### ✅ 全平台安全
- 基本 get/put、继承、虚函数派发、模板、复合赋值、`++/--`、Lambda 捕获、`sizeof`、类型转换——全部正常。

### ⚠️ 已知风险（语义层面，非编译器 bug）
1. **子成员修改绕过 Setter**：`obj.Position.X = 5` **不会**触发 `SetPosition()`。`get` 返回 `const ref` 时，修改子成员要么编译报错（const 保护），要么静默失败。应使用显式的 `AccessXxx()` 方法。
2. **取地址 `&obj.Property`**：不同编译器行为不一致，属于未定义行为。应使用返回 `T&` 的 `AccessXxx()` 方法替代。
3. **非 const 引用传参**：将属性传给 `void fn(int&)` 在 MSVC 上可能编译通过，但在 Clang 上会失败。

### 💡 运行时行为
`__declspec(property)` 是**纯编译期语法糖**。编译器将属性访问重写为函数调用，生成的机器码与手写 `GetX()` / `SetX()` 完全一致。**无运行时开销、无虚表、无 RTTI、无平台相关行为**。

## 使用方法

### 本地编译（macOS / Linux + Clang）
```bash
clang++ -std=c++17 -Wall -Wextra -fms-extensions -o test declspec_property_test.cpp && ./test
```

### 本地编译（Windows + MSVC）
```cmd
cl /EHsc /std:c++17 /W4 /Fe:test.exe declspec_property_test.cpp && test.exe
```

### 本地编译（Android NDK 交叉编译）
```bash
# ARM64 静态链接（需要 bionic_tls_align.S 修复 TLS 对齐）
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions -static \
  -o test_arm64 declspec_property_test.cpp bionic_tls_align.S
# 推送到设备运行: adb push test_arm64 /data/local/tmp/ && adb shell /data/local/tmp/test_arm64

# 动态链接（无需 TLS 修复）
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions \
  -o test_arm64 declspec_property_test.cpp
```

### CI 自动化（GitHub Actions）
推送到本仓库后，GitHub Actions 自动在以下环境并行编译和运行：

| 目标平台 | 宿主机 | 编译器 | 验证方式 |
|----------|--------|--------|---------|
| Windows x86_64 | windows-latest | MSVC | 原生编译+运行 |
| Linux x86_64 | ubuntu-22.04 | **Clang 11** | 原生编译+运行 |
| Linux x86_64 | ubuntu-22.04 | **Clang 13** | 原生编译+运行 |
| Linux x86_64 | ubuntu-latest | Clang 14 | 原生编译+运行 |
| Linux x86_64 | ubuntu-latest | Clang 16 | 原生编译+运行 |
| Linux x86_64 | ubuntu-latest | Clang 18 | 原生编译+运行 |
| Linux x86_64 | ubuntu-latest | GCC | 反向验证（预期编译失败） |
| macOS arm64 | macos-14 | Apple Clang | 原生编译+运行 |
| macOS x86_64 | macos-14 | Apple Clang | 交叉编译 + Rosetta 2 运行 |
| iOS arm64 | macos-14 | Xcode Clang | 编译 + iPhone Simulator 运行 |
| Android ARM64/ARM32/x86_64 | ubuntu-22.04 | **NDK r21e (Clang 9)** | 交叉编译验证（仅编译） |
| Android x86_64 | ubuntu-latest | NDK r27c (Clang 18) | 交叉编译 + KVM 模拟器运行 |
| Android ARM64 | ubuntu-latest | NDK r27c (Clang 18) | 交叉编译 + Docker arm64 容器运行 |
| Android ARM32 | ubuntu-latest | NDK r27c (Clang 18) | 交叉编译验证（仅编译） |

> **版本覆盖说明**：Clang 11 (2020) 和 NDK r21e (Clang 9, 2020) 是 CI 中能稳定测试的最低版本，接近 README 中声明的最低支持版本（Clang 3.3 / NDK r10）。GitHub Actions 无法直接获取更早版本的 Clang/NDK 二进制，但 `__declspec(property)` 的编译器前端支持自 Clang 3.3 起未有过不兼容变更。

#### 特殊验证方式说明

**macOS x86_64 — Rosetta 2**
GitHub Actions 的 `macos-13` (Intel) runner 已进入淘汰期，排队极慢甚至无法分配。因此在 `macos-14` (Apple Silicon) 上使用 `-target x86_64-apple-macos13` 交叉编译，再通过 Rosetta 2 指令级翻译运行，与原生 Intel 行为完全等价。

**iOS arm64 — iPhone Simulator**
使用 `xcrun --sdk iphonesimulator clang++` 编译，通过 `xcrun simctl spawn` 在 iPhone Simulator 中运行。Simulator 在 Apple Silicon 上原生执行 ARM64 代码（非指令翻译），等同真机 CPU 行为。

**Android x86_64 — KVM 模拟器**
在 ubuntu runner 上通过 Android Emulator (API 30) + KVM 硬件虚拟化运行，接近原生性能。

**Android ARM64 — Docker multiarch + QEMU binfmt**
NDK 交叉编译的静态 Bionic 二进制无法直接通过 `qemu-user-static` 运行（Bionic 要求 ARM64 TLS segment 对齐 ≥ 64 字节）。解决方案：
1. 链接时加入 `bionic_tls_align.S`，通过汇编 `.p2align 6` 强制 `.tdata` section 对齐到 64 字节（C 级 `__attribute__((aligned(64)))` 不会传播到 PT_TLS segment alignment）
2. 使用 `docker/setup-qemu-action` 注册 binfmt handler，在 `arm64v8/ubuntu` 容器中运行 NDK 编译的原始二进制

**Android ARM32 — 仅编译**
NDK 交叉编译通过零错误零警告。ARM32 与 ARM64 共享相同的 Clang `-fms-extensions` 编译器前端，`__declspec(property)` 语义一致。

### Bionic TLS 对齐修复

Android NDK 静态链接会包含 Bionic `libc.a` 的 TLS 数据，但链接器默认只生成 8 字节对齐的 PT_TLS segment。Bionic 的 `__libc_init` 启动时检查此对齐值，ARM64 要求 ≥ 64、ARM32 要求 ≥ 32，不满足直接 abort。

本仓库提供 `bionic_tls_align.S` 作为通用修复：

```bash
# 链接时加入此文件即可
aarch64-linux-android26-clang++ -static -o test test.cpp bionic_tls_align.S
```

注意：Clang 的 `__thread __attribute__((aligned(64)))` **不会**影响 PT_TLS segment 的 `p_align` 字段——TLS segment 对齐仅由 `.tdata` section 的 section alignment 决定，只能通过汇编 `.p2align` 指令控制。

## 许可证

MIT
