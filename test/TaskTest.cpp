#include "Task.h"

#include <coroutine>
#include <utility>

#include <gtest/gtest.h>

namespace {

mini_core::Task<int> make_value(int value) {
    co_return value;
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

} // namespace

TEST(TaskTest, ValueTaskStartsSuspendedAndReturnsValue) {
    auto task = make_value(7);

    EXPECT_FALSE(task.done());

    task.resume();

    ASSERT_TRUE(task.done());
    EXPECT_EQ(task.get_result(), 7);
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
