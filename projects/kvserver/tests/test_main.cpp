#include "tests/test_kit.h"

TEST(test, add) {
    CHECK(1 + 1 == 2);
}

// TEST(test, sub) {
//     CHECK(2 - 1 != 1); // error
// }

TEST(test, mul) {
    CHECK(2 * 3 == 6);
}

void running() {
    for (auto test : tests()) {
        std::cout << "Running test: " << test.first << std::endl;
        test.second(); // 调用函数指针
    }
}

int main() {
    running();
    return failures == 0 ? 0 : 1;
}