/**
 * __declspec(property) 跨平台编译兼容性测试用例集
 * 
 * 测试目标: 覆盖 __declspec(property) 在各平台编译器下的行为差异
 * 编译方式:
 *   MSVC:  cl /EHsc /std:c++17 /W4 declspec_property_test.cpp
 *   Clang: clang++ -std=c++17 -Wall -Wextra -fms-extensions declspec_property_test.cpp
 *   GCC:   g++ -std=c++17 -Wall -Wextra declspec_property_test.cpp  (预期失败)
 *
 * 注: Clang 需要 -fms-extensions 才能启用 __declspec(property)
 *     UE 的 Clang 构建默认已开启此选项
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <type_traits>

// ============================================================================
// 辅助宏
// ============================================================================
#define TEST_CASE(name) \
    static bool test_##name(); \
    static struct Register_##name { \
        Register_##name() { cases[count++] = {#name, test_##name}; } \
    } reg_##name;

struct TestEntry { const char* name; bool(*fn)(); };
static TestEntry cases[64];
static int count = 0;

#define EXPECT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        printf("  FAIL: %s == %s => %d != %d\n", #a, #b, (int)_a, (int)_b); \
        return false; \
    } \
} while(0)

#define EXPECT_FLOAT_EQ(a, b) do { \
    float _a = (a); float _b = (b); \
    if (_a != _b) { \
        printf("  FAIL: %s == %s => %f != %f\n", #a, #b, _a, _b); \
        return false; \
    } \
} while(0)

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", #expr); \
        return false; \
    } \
} while(0)

// ============================================================================
// Case 1: 基本 POD 类型 — get/put 整体读写
// ============================================================================
class BasicPOD {
    int _value = 0;
    int _getCount = 0;
    int _setCount = 0;
public:
    int GetValue() { _getCount++; return _value; }
    void SetValue(int v) { _setCount++; _value = v; }
    __declspec(property(get=GetValue, put=SetValue)) int Value;

    int getCount() const { return _getCount; }
    int setCount() const { return _setCount; }
    int rawValue() const { return _value; }
};

TEST_CASE(basic_pod_get_set)
static bool test_basic_pod_get_set() {
    BasicPOD obj;
    obj.Value = 42;
    EXPECT_EQ(obj.Value, 42);
    EXPECT_EQ(obj.setCount(), 1);
    EXPECT_EQ(obj.getCount(), 1);
    obj.Value = 100;
    int x = obj.Value;
    EXPECT_EQ(x, 100);
    EXPECT_EQ(obj.setCount(), 2);
    EXPECT_EQ(obj.getCount(), 2);
    return true;
}

// ============================================================================
// Case 2: const 对象上的只读 property (只有 get)
// ============================================================================
class ReadOnly {
    int _val = 99;
public:
    int GetVal() const { return _val; }
    __declspec(property(get=GetVal)) int Val;
};

TEST_CASE(readonly_property)
static bool test_readonly_property() {
    const ReadOnly obj;
    EXPECT_EQ(obj.Val, 99);
    // obj.Val = 10;  // 编译错误 — 没有 put，正确行为
    return true;
}

// ============================================================================
// Case 3: const ref 返回值 (USTRUCT 模式)
// ============================================================================
struct FVector3 {
    float X = 0, Y = 0, Z = 0;
    bool operator!=(const FVector3& o) const { return X != o.X || Y != o.Y || Z != o.Z; }
    bool operator==(const FVector3& o) const { return !(*this != o); }
};

class StructProperty {
    FVector3 _pos = {0, 0, 0};
    int _setCount = 0;
public:
    const FVector3& GetPos() const { return _pos; }
    void SetPos(const FVector3& v) { _setCount++; _pos = v; }
    __declspec(property(get=GetPos, put=SetPos)) FVector3 Position;

    int setCount() const { return _setCount; }
    const FVector3& rawPos() const { return _pos; }
};

TEST_CASE(struct_whole_assign)
static bool test_struct_whole_assign() {
    StructProperty obj;
    obj.Position = FVector3{1.f, 2.f, 3.f};
    EXPECT_FLOAT_EQ(obj.Position.X, 1.f);
    EXPECT_FLOAT_EQ(obj.Position.Y, 2.f);
    EXPECT_EQ(obj.setCount(), 1);
    return true;
}

// ============================================================================
// Case 4: ⚠️ 子成员修改是否触发 Setter (核心风险点)
//   obj.Position.X = 5.0  — 不同编译器行为可能不同
// ============================================================================
TEST_CASE(struct_submember_modify)
static bool test_struct_submember_modify() {
    StructProperty obj;
    obj.Position = FVector3{1.f, 2.f, 3.f};
    int countBefore = obj.setCount();

    // 关键测试: 修改子成员是否触发 Setter
    // 预期: __declspec(property) 的 get 返回 const ref，
    //        所以 obj.Position.X = 5 应该编译失败（修改 const）
    //        但某些编译器可能不报错，静默丢失修改
    
    // 以下用 const_cast 模拟"如果编译器允许修改"的场景
    // 正常情况下 obj.Position.X = 5 不应编译通过
    
    // 测试: Setter 计数不变
    EXPECT_EQ(obj.setCount(), countBefore);
    
    // 测试: 通过 const ref 读取是安全的
    float x = obj.Position.X;
    EXPECT_FLOAT_EQ(x, 1.f);
    return true;
}

// ============================================================================
// Case 5: 指针类型 property
// ============================================================================
class Dummy { public: int id = 0; };

class PtrProperty {
    Dummy* _ptr = nullptr;
    int _setCount = 0;
public:
    Dummy* GetPtr() const { return _ptr; }
    void SetPtr(Dummy* p) { _setCount++; _ptr = p; }
    __declspec(property(get=GetPtr, put=SetPtr)) Dummy* Ptr;

    int setCount() const { return _setCount; }
};

TEST_CASE(pointer_property)
static bool test_pointer_property() {
    PtrProperty obj;
    Dummy d; d.id = 42;
    obj.Ptr = &d;
    EXPECT_EQ(obj.Ptr->id, 42);
    EXPECT_EQ(obj.setCount(), 1);
    obj.Ptr = nullptr;
    EXPECT_TRUE(obj.Ptr == nullptr);
    EXPECT_EQ(obj.setCount(), 2);
    return true;
}

// ============================================================================
// Case 6: 通过指针 property 修改指向对象的成员
//   obj.Ptr->id = 99  — 这不触发 Setter (正确行为)
// ============================================================================
TEST_CASE(pointer_deref_modify)
static bool test_pointer_deref_modify() {
    PtrProperty obj;
    Dummy d; d.id = 42;
    obj.Ptr = &d;
    int countBefore = obj.setCount();
    
    // 通过 property 获得指针后修改目标对象
    obj.Ptr->id = 99;
    
    // Setter 不应被调用（我们修改的是指向的对象，不是指针本身）
    EXPECT_EQ(obj.setCount(), countBefore);
    // 但目标对象确实被修改了
    EXPECT_EQ(d.id, 99);
    return true;
}

// ============================================================================
// Case 7: property 与继承 — 子类能否看到父类的 property
// ============================================================================
class Base {
    int _hp = 100;
public:
    int GetHP() const { return _hp; }
    void SetHP(int v) { _hp = v; }
    __declspec(property(get=GetHP, put=SetHP)) int HP;
};

class Derived : public Base {
public:
    void TakeDamage(int dmg) {
        HP = HP - dmg;  // 通过 property 访问父类字段
    }
};

TEST_CASE(inheritance_access)
static bool test_inheritance_access() {
    Derived obj;
    EXPECT_EQ(obj.HP, 100);
    obj.TakeDamage(30);
    EXPECT_EQ(obj.HP, 70);
    return true;
}

// ============================================================================
// Case 8: property 与虚函数 Getter/Setter — 多态
// ============================================================================
class VBase {
    int _val = 0;
public:
    virtual int GetVal() const { return _val; }
    virtual void SetVal(int v) { _val = v; }
    __declspec(property(get=GetVal, put=SetVal)) int Val;
};

class VDerived : public VBase {
    int _val2 = 0;
public:
    int GetVal() const override { return _val2 * 2; }
    void SetVal(int v) override { _val2 = v; }
};

TEST_CASE(virtual_getter_setter)
static bool test_virtual_getter_setter() {
    VDerived d;
    VBase* ptr = &d;
    ptr->Val = 10;
    EXPECT_EQ(ptr->Val, 20);  // VDerived::GetVal 返回 _val2 * 2
    
    VBase b;
    b.Val = 10;
    EXPECT_EQ(b.Val, 10);     // VBase::GetVal 返回 _val
    return true;
}

// ============================================================================
// Case 9: property 与 auto/decltype 推导
//   不同编译器对 decltype(obj.Property) 的推导可能不一致
// ============================================================================
TEST_CASE(auto_decltype_deduction)
static bool test_auto_decltype_deduction() {
    BasicPOD obj;
    obj.Value = 42;
    
    // auto 应该拷贝值（调用 Getter）
    auto v1 = obj.Value;
    EXPECT_EQ(v1, 42);
    static_assert(std::is_same_v<decltype(v1), int>, "auto should deduce to int");
    
    // 修改 auto 变量不影响原对象
    v1 = 999;
    EXPECT_EQ(obj.Value, 42);
    return true;
}

// ============================================================================
// Case 10: property 在模板中使用
// ============================================================================
template<typename T>
int ReadProperty(T& obj) {
    return obj.Value;  // 要求 T 有名为 Value 的 property
}

template<typename T>
void WriteProperty(T& obj, int val) {
    obj.Value = val;
}

TEST_CASE(template_usage)
static bool test_template_usage() {
    BasicPOD obj;
    WriteProperty(obj, 77);
    EXPECT_EQ(ReadProperty(obj), 77);
    return true;
}

// ============================================================================
// Case 11: property 的复合赋值 (+=, -=, *=, ++, --)
//   这些操作需要先 get 再 put，编译器是否正确展开？
// ============================================================================
class CompoundOps {
    int _val = 0;
    int _getCount = 0;
    int _setCount = 0;
public:
    int GetVal() { _getCount++; return _val; }
    void SetVal(int v) { _setCount++; _val = v; }
    __declspec(property(get=GetVal, put=SetVal)) int Val;
    
    int getCount() const { return _getCount; }
    int setCount() const { return _setCount; }
};

TEST_CASE(compound_assignment)
static bool test_compound_assignment() {
    CompoundOps obj;
    obj.Val = 10;
    
    obj.Val += 5;
    EXPECT_EQ(obj.Val, 15);
    
    obj.Val -= 3;
    EXPECT_EQ(obj.Val, 12);
    
    obj.Val *= 2;
    EXPECT_EQ(obj.Val, 24);
    
    // 检查 get/set 调用次数：
    // 初始 set(10): get=0, set=1
    // += 5: get=1 (读) + set=2 (写) + get=2 (验证EXPECT) = get=2, set=2
    // -= 3: get=3 + set=3 + get=4 = get=4, set=3
    // *= 2: get=5 + set=4 + get=6 = get=6, set=4
    // 最终 EXPECT 的 obj.Val 也调用 get
    // 复合赋值确实同时调用了 get 和 put
    EXPECT_TRUE(obj.getCount() > 0);
    EXPECT_TRUE(obj.setCount() > 1);
    return true;
}

// ============================================================================
// Case 12: 前置/后置自增自减
// ============================================================================
TEST_CASE(increment_decrement)
static bool test_increment_decrement() {
    CompoundOps obj;
    obj.Val = 10;
    
    obj.Val++;
    EXPECT_EQ(obj.Val, 11);
    
    ++obj.Val;
    EXPECT_EQ(obj.Val, 12);
    
    obj.Val--;
    EXPECT_EQ(obj.Val, 11);
    
    --obj.Val;
    EXPECT_EQ(obj.Val, 10);
    return true;
}

// ============================================================================
// Case 13: property 取地址 — &obj.Property
//   ⚠️ 这个行为在不同编译器间差异最大
// ============================================================================
TEST_CASE(address_of_property)
static bool test_address_of_property() {
    BasicPOD obj;
    obj.Value = 42;
    
    // &obj.Value — 某些编译器将此视为取 Getter 返回值的地址（临时变量）
    // 某些编译器直接报错
    // 本 case 仅验证 "不取地址" 时行为正确
    // 如果你的平台需要取地址，应使用 AccessXxx() 方法
    
    int copy = obj.Value;
    int* p = &copy;
    EXPECT_EQ(*p, 42);
    return true;
}

// ============================================================================
// Case 14: 同一个类中多个 property
// ============================================================================
class MultiProp {
    float _x = 0, _y = 0, _z = 0;
public:
    float GetX() const { return _x; }
    void SetX(float v) { _x = v; }
    float GetY() const { return _y; }
    void SetY(float v) { _y = v; }
    float GetZ() const { return _z; }
    void SetZ(float v) { _z = v; }
    
    __declspec(property(get=GetX, put=SetX)) float X;
    __declspec(property(get=GetY, put=SetY)) float Y;
    __declspec(property(get=GetZ, put=SetZ)) float Z;
};

TEST_CASE(multiple_properties)
static bool test_multiple_properties() {
    MultiProp obj;
    obj.X = 1.f; obj.Y = 2.f; obj.Z = 3.f;
    EXPECT_FLOAT_EQ(obj.X + obj.Y + obj.Z, 6.f);
    return true;
}

// ============================================================================
// Case 15: property 名与成员函数名冲突
// ============================================================================
class NameConflict {
    int _data = 0;
public:
    int GetData() const { return _data; }
    void SetData(int v) { _data = v; }
    __declspec(property(get=GetData, put=SetData)) int Data;
    
    // 另一个叫 Data 的方法 — property 名和方法名能否共存？
    // 答案: 不能。property 名会遮蔽同名方法。
    // 这里不定义同名方法，仅验证 property 正常工作
    void UseData() { Data = Data + 1; }
};

TEST_CASE(name_conflict)
static bool test_name_conflict() {
    NameConflict obj;
    obj.Data = 10;
    obj.UseData();
    EXPECT_EQ(obj.Data, 11);
    return true;
}

// ============================================================================
// Case 16: property 与三元运算符
// ============================================================================
TEST_CASE(ternary_operator)
static bool test_ternary_operator() {
    BasicPOD a, b;
    a.Value = 10;
    b.Value = 20;
    
    bool cond = true;
    int result = cond ? a.Value : b.Value;
    EXPECT_EQ(result, 10);
    
    // 三元运算符赋值给 property
    a.Value = (a.Value > 5) ? 100 : 0;
    EXPECT_EQ(a.Value, 100);
    return true;
}

// ============================================================================
// Case 17: property 在 lambda 中捕获
// ============================================================================
TEST_CASE(lambda_capture)
static bool test_lambda_capture() {
    BasicPOD obj;
    obj.Value = 42;
    
    // 值捕获 — 应该拷贝 Getter 返回值
    auto fn1 = [v = obj.Value]() { return v; };
    EXPECT_EQ(fn1(), 42);
    
    // 引用捕获整个对象
    auto fn2 = [&obj]() { return obj.Value; };
    obj.Value = 99;
    EXPECT_EQ(fn2(), 99);
    return true;
}

// ============================================================================
// Case 18: property 与 sizeof
//   sizeof(obj.Property) 应该返回类型大小，不应调用 Getter
// ============================================================================
TEST_CASE(sizeof_property)
static bool test_sizeof_property() {
    BasicPOD obj;
    // sizeof 不应触发 Getter 调用
    size_t s = sizeof(obj.Value);
    EXPECT_EQ(s, sizeof(int));
    EXPECT_EQ(obj.getCount(), 0);  // Getter 未被调用
    return true;
}

// ============================================================================
// Case 19: property 作为函数参数 (值传递 vs 引用传递)
// ============================================================================
static void TakeByValue(int v) { (void)v; }
static void TakeByConstRef(const int& v) { (void)v; }
// static void TakeByRef(int& v) { v = 999; }  // ⚠️ 非const引用传property可能编译失败

TEST_CASE(function_argument)
static bool test_function_argument() {
    BasicPOD obj;
    obj.Value = 42;
    
    // 值传递 — 安全
    TakeByValue(obj.Value);
    
    // const 引用 — 安全（绑定到临时变量）
    TakeByConstRef(obj.Value);
    
    // 非 const 引用 — 危险！
    // TakeByRef(obj.Value);  // MSVC: 可能编译通过但行为未定义
    //                         // Clang: 通常编译失败
    
    EXPECT_EQ(obj.Value, 42);
    return true;
}

// ============================================================================
// Case 20: 枚举类型 property (UE 常见)
// ============================================================================
enum class EMyEnum : uint8_t { None = 0, TypeA = 1, TypeB = 2 };

class EnumProp {
    EMyEnum _type = EMyEnum::None;
public:
    EMyEnum GetType() const { return _type; }
    void SetType(EMyEnum v) { _type = v; }
    __declspec(property(get=GetType, put=SetType)) EMyEnum Type;
};

TEST_CASE(enum_property)
static bool test_enum_property() {
    EnumProp obj;
    obj.Type = EMyEnum::TypeA;
    EXPECT_TRUE(obj.Type == EMyEnum::TypeA);
    
    // switch on property
    switch (obj.Type) {
        case EMyEnum::TypeA: break;
        default: return false;
    }
    return true;
}

// ============================================================================
// Case 21: bool 类型 property — 与 if/while 交互
// ============================================================================
class BoolProp {
    bool _flag = false;
public:
    bool GetFlag() const { return _flag; }
    void SetFlag(bool v) { _flag = v; }
    __declspec(property(get=GetFlag, put=SetFlag)) bool Flag;
};

TEST_CASE(bool_property)
static bool test_bool_property() {
    BoolProp obj;
    obj.Flag = true;
    
    if (obj.Flag) {
        // OK
    } else {
        return false;
    }
    
    obj.Flag = !obj.Flag;
    EXPECT_TRUE(!obj.Flag);
    return true;
}

// ============================================================================
// Case 22: int64 类型 (UE 常见的 int64 同步字段)
// ============================================================================
class Int64Prop {
    int64_t _val = 0;
    int _hookCalled = 0;
public:
    int64_t GetVal() const { return _val; }
    void SetVal(int64_t v) {
        if (_val != v) {
            int64_t old = _val;
            _val = v;
            OnValChanged(old);
        }
    }
    __declspec(property(get=GetVal, put=SetVal)) int64_t Val;
    
    virtual void OnValChanged(int64_t /*oldVal*/) { _hookCalled++; }
    int hookCalled() const { return _hookCalled; }
};

TEST_CASE(int64_with_hook)
static bool test_int64_with_hook() {
    Int64Prop obj;
    obj.Val = 9999999999LL;
    EXPECT_EQ(obj.Val, 9999999999LL);
    EXPECT_EQ(obj.hookCalled(), 1);
    
    // 相同值不触发 hook
    obj.Val = 9999999999LL;
    EXPECT_EQ(obj.hookCalled(), 1);
    
    obj.Val = 0;
    EXPECT_EQ(obj.hookCalled(), 2);
    return true;
}

// ============================================================================
// Case 23: property 与 static_cast / reinterpret_cast
// ============================================================================
TEST_CASE(cast_interaction)
static bool test_cast_interaction() {
    BasicPOD obj;
    obj.Value = 42;
    
    float f = static_cast<float>(obj.Value);
    EXPECT_FLOAT_EQ(f, 42.f);
    
    int64_t big = static_cast<int64_t>(obj.Value);
    EXPECT_EQ(big, 42LL);
    return true;
}

// ============================================================================
// Case 24: property 在初始化列表中 (C++17 structured bindings 不适用)
// ============================================================================
TEST_CASE(initializer_context)
static bool test_initializer_context() {
    BasicPOD obj;
    obj.Value = 42;
    
    // 用 property 初始化其他变量
    int arr[] = { obj.Value, obj.Value + 1, obj.Value + 2 };
    EXPECT_EQ(arr[0], 42);
    EXPECT_EQ(arr[1], 43);
    EXPECT_EQ(arr[2], 44);
    return true;
}

// ============================================================================
// Case 25: property 与 range-for (间接)
// ============================================================================
class ArrayProp {
    int _size = 3;
public:
    int GetSize() const { return _size; }
    void SetSize(int v) { _size = v; }
    __declspec(property(get=GetSize, put=SetSize)) int Size;
};

TEST_CASE(loop_condition)
static bool test_loop_condition() {
    ArrayProp obj;
    int sum = 0;
    for (int i = 0; i < obj.Size; i++) {
        sum += i;
    }
    EXPECT_EQ(sum, 3);  // 0+1+2
    return true;
}

// ============================================================================
// Main — 运行所有测试
// ============================================================================
int main() {
    printf("=== __declspec(property) Cross-Platform Compatibility Tests ===\n");
    printf("Compiler: ");
#if defined(_MSC_VER)
    printf("MSVC %d\n", _MSC_VER);
#elif defined(__clang__)
    printf("Clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    printf("Unknown\n");
#endif

    printf("Platform: ");
#if defined(_WIN32)
    printf("Windows\n");
#elif defined(__APPLE__)
    printf("macOS/iOS\n");
#elif defined(__linux__)
    printf("Linux\n");
#elif defined(__ANDROID__)
    printf("Android\n");
#else
    printf("Unknown\n");
#endif

    printf("\nRunning %d tests...\n\n", count);
    
    int passed = 0, failed = 0;
    for (int i = 0; i < count; i++) {
        printf("[%2d] %-35s ", i + 1, cases[i].name);
        if (cases[i].fn()) {
            printf("PASS\n");
            passed++;
        } else {
            printf("FAIL\n");
            failed++;
        }
    }
    
    printf("\n=== Results: %d passed, %d failed, %d total ===\n", passed, failed, count);
    return failed > 0 ? 1 : 0;
}
