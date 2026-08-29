#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <utility>

// inline 全部 cpp 文件（全部编译单元）共享这一个定义，这样就不会导致重复定义的问题。
// 也可也使用 extern 声明，然后在一个 cpp 文件中定义
// 这里不能使用 static，因为 static 会导致每个编译单元都有一个独立的变量，无法在不同编译单元之间共享。static的作用域是文件级别的，而不是全局的。
inline int failures = 0;

// 断言宏
// __FILE__ 和 __LINE__ 是预定义的宏，分别表示当前文件名和行号
// #expr 是一个字符串化操作符，它将表达式转换为字符串
// do { ... } while (0) 是一个常见的宏定义技巧，它允许宏在使用时像一个语句一样使用，并且可以安全地放在 if 语句中而不需要大括号
// 以换行符结尾
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << __FILE__ << ":" << __LINE__ << " : CHECK failed: " << #expr << std::endl; \
            ++failures; \
        } \
    } while (0)

// 测试宏
// ## 是预处理器的连接操作符，它将两个标识符连接成一个标识符。这里需要先进行函数声明，因为宏替换时要保证函数可以使用
// 宏最后后接函数体
#define TEST(suite, name) \
    void suite##_##name(); \
    RegisterTest register_##suite_##name(#suite "_" #name, suite##_##name); \
    void suite##_##name() \

using fun_ptr = void (*)(); // 函数指针： 返回类型 void 指针符号 (*) 函数参数 (void)
// 为什么要使用函数形式而不是变量形式？函数形式是懒加载，在使用到 tests 时才进行初始化，而变量则需要我们主动考虑初始化时机。
inline std::vector<std::pair<std::string, fun_ptr>>& tests() { // inline 保证对每一个编译单元共用这一个
    static std::vector<std::pair<std::string, fun_ptr>> instance; // 懒加载，静态变量，只在第一次使用时初始化，保证每次使用 tests 已经构建了
    return instance;
}

// 注册测试类
class RegisterTest {
public:
    // 每次构造在静态变量中注册一个测试函数
    RegisterTest(std::string name, fun_ptr test) {
        tests().push_back({name, test});
    }
};