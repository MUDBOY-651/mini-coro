#include "Task.h"

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

class TestContinuation {
public:
    struct promise_type {
        TestContinuation get_return_object() {
            return TestContinuation{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    explicit TestContinuation(std::coroutine_handle<promise_type> handle): handle_(handle) {}
    TestContinuation(const TestContinuation&) = delete;
    TestContinuation& operator=(const TestContinuation&) = delete;

    TestContinuation(TestContinuation&& other) noexcept: handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    ~TestContinuation() {
        if (handle_) {
            handle_.destroy();
        }
    }

    std::coroutine_handle<> handle() const noexcept {
        return handle_;
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

TestContinuation suspended_continuation(bool& resumed) {
    co_await std::suspend_always{};
    resumed = true;
}

struct NonDefaultResult {
    explicit NonDefaultResult(int value): value(value) {}
    NonDefaultResult() = delete;
    NonDefaultResult(const NonDefaultResult&) = delete;
    NonDefaultResult& operator=(const NonDefaultResult&) = delete;
    NonDefaultResult(NonDefaultResult&&) noexcept = default;
    NonDefaultResult& operator=(NonDefaultResult&&) noexcept = default;

    int value;
};

static_assert(!std::is_default_constructible_v<NonDefaultResult>);

mini_core::Task<int> make_value(int value) {
    co_return value;
}

mini_core::Task<NonDefaultResult> make_non_default_result(int value) {
    co_return NonDefaultResult{value};
}

mini_core::Task<int> step_value_task(int& step) {
    step = 1;
    co_await std::suspend_always{};
    step = 2;
    co_return 42;
}

mini_core::Task<void> step_void_task(int& step) {
    step = 1;
    co_await std::suspend_always{};
    step = 2;
    co_return;
}

mini_core::Task<int> inner_value_task(int& run_count) {
    ++run_count;
    co_return 42;
}

mini_core::Task<int> outer_awaits_value_task(int& inner_run_count) {
    int value = co_await inner_value_task(inner_run_count);
    co_return value + 10;
}

mini_core::Task<void> inner_void_task(int& run_count) {
    ++run_count;
    co_return;
}

mini_core::Task<int> outer_awaits_void_task(int& inner_run_count) {
    co_await inner_void_task(inner_run_count);
    co_return inner_run_count + 10;
}

mini_core::Task<int> throwing_value_task() {
    throw std::runtime_error("value failed");
    co_return 1;
}

mini_core::Task<void> throwing_void_task() {
    throw std::runtime_error("void failed");
    co_return;
}

mini_core::Task<int> outer_awaits_throwing_value_task() {
    co_await throwing_value_task();
    co_return 1;
}

mini_core::Task<int> outer_awaits_throwing_void_task() {
    co_await throwing_void_task();
    co_return 1;
}

} // namespace

TEST(TaskTest, ValueTaskStartsSuspendedAndReturnsValue) {
    auto task = make_value(7);

    EXPECT_FALSE(task.done());

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_EQ(task.get_result(), 7);
}

TEST(TaskTest, ValueTaskCanReturnNonDefaultConstructibleResult) {
    auto task = make_non_default_result(17);

    task.resume();

    ASSERT_TRUE(task.done());
    auto result = task.get_result();
    EXPECT_EQ(result.value, 17);
}

TEST(TaskTest, ResumeAdvancesCoroutineUntilNextSuspendPoint) {
    int step = 0;
    auto task = step_value_task(step);

    EXPECT_EQ(step, 0);
    EXPECT_FALSE(task.done());

    task.resume();
    EXPECT_EQ(step, 1);
    EXPECT_FALSE(task.done());

    task.resume();
    EXPECT_EQ(step, 2);
    ASSERT_TRUE(task.done());
    EXPECT_EQ(task.get_result(), 42);
}

TEST(TaskTest, VoidTaskCanBeResumedUntilDone) {
    int step = 0;
    auto task = step_void_task(step);

    EXPECT_EQ(step, 0);

    task.resume();
    EXPECT_EQ(step, 1);
    EXPECT_FALSE(task.done());

    task.resume();
    EXPECT_EQ(step, 2);
    EXPECT_TRUE(task.done());
}

TEST(TaskTest, ValueTaskGetResultRethrowsStoredException) {
    auto task = throwing_value_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.get_result(), std::runtime_error);
}

TEST(TaskTest, VoidTaskGetResultRethrowsStoredException) {
    auto task = throwing_void_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.get_result(), std::runtime_error);
}

TEST(TaskTest, CoAwaitValueTaskRunsInnerCoroutineAndReturnsResult) {
    int inner_run_count = 0;
    auto task = outer_awaits_value_task(inner_run_count);

    task.resume();

    EXPECT_EQ(inner_run_count, 1);
    ASSERT_TRUE(task.done());
    EXPECT_EQ(task.get_result(), 52);
}

TEST(TaskTest, CoAwaitVoidTaskRunsInnerCoroutineAndResumesOuterCoroutine) {
    int inner_run_count = 0;
    auto task = outer_awaits_void_task(inner_run_count);

    task.resume();

    EXPECT_EQ(inner_run_count, 1);
    ASSERT_TRUE(task.done());
    EXPECT_EQ(task.get_result(), 11);
}

TEST(TaskTest, CoAwaitValueTaskPropagatesExceptionToOuterTask) {
    auto task = outer_awaits_throwing_value_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.get_result(), std::runtime_error);
}

TEST(TaskTest, CoAwaitVoidTaskPropagatesExceptionToOuterTask) {
    auto task = outer_awaits_throwing_void_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.get_result(), std::runtime_error);
}

TEST(TaskTest, AwaiterReadyReflectsTaskCompletion) {
    auto task = make_value(13);

    EXPECT_FALSE(task.await_ready());

    task.resume();

    ASSERT_TRUE(task.await_ready());
    EXPECT_EQ(task.await_resume(), 13);
}

TEST(TaskTest, ValueTaskAwaitResumeRethrowsStoredException) {
    auto task = throwing_value_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.await_resume(), std::runtime_error);
}

TEST(TaskTest, VoidTaskAwaitResumeRethrowsStoredException) {
    auto task = throwing_void_task();

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_THROW(task.await_resume(), std::runtime_error);
}

TEST(TaskTest, AwaitSuspendReturnsInnerHandleAndInnerCompletionResumesContinuation) {
    int inner_run_count = 0;
    bool continuation_resumed = false;
    auto task = inner_value_task(inner_run_count);
    auto continuation = suspended_continuation(continuation_resumed);
    continuation.resume();

    auto next = task.await_suspend(continuation.handle());

    EXPECT_TRUE(next);
    EXPECT_EQ(inner_run_count, 0);
    EXPECT_FALSE(continuation_resumed);

    next.resume();

    EXPECT_EQ(inner_run_count, 1);
    EXPECT_TRUE(continuation_resumed);
    ASSERT_TRUE(task.await_ready());
    EXPECT_EQ(task.await_resume(), 42);
}

TEST(TaskTest, VoidAwaiterRunsInnerHandleAndResumesContinuation) {
    int inner_run_count = 0;
    bool continuation_resumed = false;
    auto task = inner_void_task(inner_run_count);
    auto continuation = suspended_continuation(continuation_resumed);
    continuation.resume();

    auto next = task.await_suspend(continuation.handle());

    EXPECT_TRUE(next);
    EXPECT_EQ(inner_run_count, 0);
    EXPECT_FALSE(continuation_resumed);

    next.resume();

    EXPECT_EQ(inner_run_count, 1);
    EXPECT_TRUE(continuation_resumed);
    EXPECT_TRUE(task.await_ready());
    EXPECT_NO_THROW(task.await_resume());
}

TEST(TaskTest, MoveConstructionTransfersCoroutineHandle) {
    auto original = make_value(9);
    mini_core::Task<int> moved(std::move(original));

    EXPECT_TRUE(original.done());

    moved.resume();

    ASSERT_TRUE(moved.done());
    EXPECT_EQ(moved.get_result(), 9);
}

TEST(TaskTest, MoveAssignmentReplacesCoroutineHandle) {
    auto target = make_value(1);
    auto source = make_value(2);

    target = std::move(source);

    EXPECT_TRUE(source.done());

    target.resume();

    ASSERT_TRUE(target.done());
    EXPECT_EQ(target.get_result(), 2);
}
