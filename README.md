# `__declspec(property)` 跨平台编译兼容性测试套件

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
$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android26-clang++ \
  -std=c++17 -fms-extensions -o test_arm64 declspec_property_test.cpp
# 推送到设备运行: adb push test_arm64 /data/local/tmp/ && adb shell /data/local/tmp/test_arm64
```

### CI 自动化（GitHub Actions）
推送到本仓库后，GitHub Actions 自动在以下环境并行编译和运行：
- ✅ Windows（MSVC 最新版）
- ✅ Ubuntu（Clang 14 / 16 / 18）
- ✅ Ubuntu GCC（反向验证——预期编译失败）
- ✅ macOS（Apple Clang 最新版）
- ✅ Android NDK r27c（ARM64 / ARM32 / x86_64 交叉编译）

## 许可证

MIT
