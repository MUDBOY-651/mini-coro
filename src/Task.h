#pragma once
#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#include "Scheduler.h"

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
    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            void await_resume() noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept {
                if (h.promise().continuation_) {
                    return h.promise().continuation_;
                }
                return std::noop_coroutine();
            }
        };
        return FinalAwaiter{};
    }

    // 4. 处理 co_return 的值
    template <typename U>
    void return_value(U&& value) {
        value_.emplace(std::forward<U>(value));
    }

    // 5. 处理未捕获异常
    void unhandled_exception() {
        exception_ = std::current_exception();
    }

    void rethrow_if_exception() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    std::optional<T> value_;
    std::coroutine_handle<> continuation_{};
    std::exception_ptr exception_;
};

// TaskPromise特化void
template<typename T>
struct TaskPromise<T, std::enable_if_t<std::is_void_v<T>>> {

    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            void await_resume() noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept {
                if (h.promise().continuation_) {
                    return h.promise().continuation_;
                }
                return std::noop_coroutine();
            }
        };
        return FinalAwaiter{};
    }



    void unhandled_exception() {
        exception_ = std::current_exception();
    }

    void rethrow_if_exception() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    void return_void() {}

    std::coroutine_handle<> continuation_{};
    std::exception_ptr exception_;
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

    /* Awaiter Begin */

    // 协程是否已经完成？完成则不挂起，直接取结果
    bool await_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    // 协程未完成，挂起当前协程，把当前协程的handle存起来
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
        assert(handle_);
        handle_.promise().continuation_ = continuation;
        return handle_;
    }

    T await_resume() {
        assert(handle_ && handle_.done());
        auto& promise = handle_.promise();
        promise.rethrow_if_exception();
        if constexpr (std::is_void_v<T>) {
            return;
        } else {
            assert(promise.value_.has_value());
            return std::move(*promise.value_);
        }
    }
    /* Awaiter End */

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
        auto& promise = handle_.promise();
        promise.rethrow_if_exception();
        if constexpr (std::is_void_v<T>) {
            return;
        } else {
            assert(promise.value_.has_value());
            return std::move(*promise.value_);
        }
    }

    void start(Scheduler &scheduler) {
        if (handle_) {
            scheduler.schedule(handle_);
        }
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

}
