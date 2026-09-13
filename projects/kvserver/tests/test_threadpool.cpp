#include "src/util/thread_pool.h"

#include <chrono>
#include <thread>

#include <iostream>

// 接入测试框架时删掉main
int main() {
    ThreadPool pool(4);

    auto f = pool.submit([] {
        return 42;
    });
    std::cout << "result = " << f.get() << "\n";

    auto bad = pool.submit([]() -> int {
        throw std::runtime_error("boom");
    });
    try {
        bad.get();
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << "\n";
    }

    auto g = pool.submit([] {
        return 7;
    });
    std::cout << "still alive: " << g.get() << "\n";
    
    for (int i=0; i<10; ++i) {
        pool.submit([i] {
            std::cout << "task " + std::to_string(i) + "\n";
        });
    }

    return 0;
}