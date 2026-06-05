#pragma once
#include <cassert>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>

namespace mini_core {

template<typename T>
class Task;

template<typename T, typename = void>
struct TaskPromise {
    // 1. 创建协程返回的对象（就是 Task<T>）
    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }
    // 2. 控制协程一开始是立即执行还是挂起
    std::suspend_always initial_suspend() { return {}; }

    // 3. 控制协程结束前是否挂起（通常必须挂起，让外部取结果）
    std::suspend_always final_suspend() noexcept { return {}; }

    // 4. 处理 co_return 的值
    void return_value(T val) {
        val_ = std::move(val);
    }

    // 5. 处理未捕获异常
    void unhandled_exception() { std::terminate(); }

    T val_;
};

// TaskPromise特化void
template<typename T>
struct TaskPromise<T, std::enable_if_t<std::is_void_v<T>>> {

    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() { std::terminate(); }

    void return_void() {}
};


template <typename T>
class Task {
public:
    // 支持协程的对象，内部需要定义嵌套的promise_type类型
    using promise_type = TaskPromise<T>;
    explicit Task(std::coroutine_handle<promise_type> h): handle_(h) {}
    Task(const Task&) = delete;
    Task& operator =(const Task&) = delete;

    Task(Task&& other) noexcept: handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    bool done() const {
        return !handle_ || handle_.done();
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    T get_result() {
        assert(handle_ && handle_.done());
        return std::move(handle_.promise().val_);
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

}


