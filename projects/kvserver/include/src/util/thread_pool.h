#pragma once
#include <queue>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>
#include <utility>
#include <stdexcept>

class ThreadPool {
    public:
    explicit ThreadPool(size_t thread_count) : stop(false) {
        workers.reserve(thread_count);
        for (size_t i=0; i<thread_count; ++i) {
            // thread 不支持拷贝，只能移动
            // 必须捕获 this，调用 worker_loop() 需要一个对象
            workers.emplace_back([this] {
                worker_loop();
            });
        }
    }
    ~ThreadPool() {
        // 给 stop 加锁
        {
            std::lock_guard<std::mutex> lg(mtx);
            stop = true;
        }

        // 唤醒所有 worker，notify_all 把等待者队列上的线程全部标记为就绪、放回调度器运行队列
        cv.notify_all();

        for (auto& worker : workers) {
            worker.join();
        }
    }
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(std::forward<Args>(args)...))>; // 外部接口，提交一个任务

    private:
    void worker_loop(); // 每一个线程的工作循环

    private:
    std::queue<std::function<void()>> tasks; // 待处理的任务队列
    std::vector<std::thread> workers; // 池里常驻的工作线程
    bool stop; // 池子是否关闭
    std::mutex mtx;
    std::condition_variable cv;
};

inline void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> fun;

        {
            std::unique_lock<std::mutex> ul(mtx); // 拿锁，给 tasks 和 stop 加锁
            // 开始睡觉（睡觉的时候会解锁），直到有任务了 或者 池子停止了
            // 等价于
            // while (tasks.empty() && !stop) {
            //     // 原子操作
            //     ul.unlock();
            //     cv.wait_queue.push_back(); // cv 会将这个线程挂到等待者队列，那么此时这个线程就被挂起了，指令就停在这一步，直到 cv 再次将它唤醒，while里面的代码才会继续执行
            //     ul.lock();
            // }
            cv.wait(ul, [this] {
                return !tasks.empty() || stop;
            });
            if (stop && tasks.empty()) break; // 池子停止了并且任务都执行完毕了，直接退出，线程结束工作

            // 从任务队列里面拿一个任务
            fun = std::move(tasks.front());
            tasks.pop();
        } // 此时解锁

        // 执行任务
        try {
            fun();
        } catch (...) {

        }
    }
}

template <class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(std::forward<Args>(args)...))> {
    using return_type = std::packaged_task<decltype(f(std::forward<Args>(args)...))()>; // 推导 f(args...) 的返回类型
    auto pack = std::make_shared<return_type>([f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable { // mutable:让闭包的 operator() 不是 const,捕获的成员才不是 const —— 否则既调不动非 const的 f,也 forward 不出去
        return f(std::forward<Args>(args)...);
    }); // 用智能指针将打包结果包起来，因为打包结果的生命周期比submit长，需要延长它的生命周期，必须活到被 worker 执行的那一刻(它不可拷贝,只能靠引用计数共享)
    auto fut = pack->get_future();
    {
        std::lock_guard<std::mutex> lg(mtx);
        if (stop) {
            throw std::runtime_error("submit on stopped ThreadPool");
        }
        tasks.emplace([pack] {
            (*pack)();
        });
    }
    
    cv.notify_one();

    return fut;
}