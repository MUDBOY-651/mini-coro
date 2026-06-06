#include "Task.h"

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;

mini_core::Task<void> set_flag_task(bool& flag) {
    flag = true;
    co_return;
}

mini_core::Task<void> increment_task(int& count) {
    ++count;
    co_return;
}

mini_core::Task<void> yielding_task(mini_core::Scheduler& scheduler, std::vector<int>& events) {
    events.push_back(1);
    co_await scheduler.yield();
    events.push_back(3);
    co_return;
}

mini_core::Task<void> simple_event_task(std::vector<int>& events) {
    events.push_back(2);
    co_return;
}

mini_core::Task<void> multi_yield_task(mini_core::Scheduler& scheduler, std::vector<int>& events) {
    events.push_back(1);
    co_await scheduler.yield();
    events.push_back(2);
    co_await scheduler.yield();
    events.push_back(3);
    co_return;
}

mini_core::Task<void> sleep_flag_task(mini_core::Scheduler& scheduler, bool& flag, std::chrono::milliseconds duration) {
    co_await scheduler.sleep_for(duration);
    flag = true;
    co_return;
}

mini_core::Task<void> zero_sleep_task(mini_core::Scheduler& scheduler, std::vector<int>& events) {
    events.push_back(1);
    co_await scheduler.sleep_for(0ms);
    events.push_back(3);
    co_return;
}

mini_core::Task<void> sleep_event_task(
    mini_core::Scheduler& scheduler,
    std::vector<int>& events,
    int event,
    std::chrono::milliseconds duration
) {
    co_await scheduler.sleep_for(duration);
    events.push_back(event);
    co_return;
}

} // namespace

TEST(SchedulerTest, RunOnEmptyQueueDoesNothing) {
    mini_core::Scheduler scheduler;

    EXPECT_NO_THROW(scheduler.run());
}

TEST(SchedulerTest, StartSchedulesTaskAndRunExecutesIt) {
    mini_core::Scheduler scheduler;
    bool flag = false;
    auto task = set_flag_task(flag);

    task.start(scheduler);

    EXPECT_FALSE(flag);
    EXPECT_FALSE(task.done());

    scheduler.run();

    EXPECT_TRUE(flag);
    EXPECT_TRUE(task.done());
}

TEST(SchedulerTest, YieldRequeuesTaskBehindAlreadyScheduledWork) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;
    auto first = yielding_task(scheduler, events);
    auto second = simple_event_task(events);

    first.start(scheduler);
    second.start(scheduler);
    scheduler.run();

    EXPECT_EQ(events, (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(first.done());
    EXPECT_TRUE(second.done());
}

TEST(SchedulerTest, RunContinuesTaskAcrossMultipleYields) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;
    auto task = multi_yield_task(scheduler, events);

    task.start(scheduler);
    scheduler.run();

    EXPECT_EQ(events, (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(task.done());
}

TEST(SchedulerTest, RunSkipsAlreadyCompletedTaskHandle) {
    mini_core::Scheduler scheduler;
    int count = 0;
    auto task = increment_task(count);

    task.start(scheduler);
    scheduler.run();
    ASSERT_TRUE(task.done());
    EXPECT_EQ(count, 1);

    task.start(scheduler);
    scheduler.run();

    EXPECT_EQ(count, 1);
}

TEST(SchedulerTest, SleepForResumesTaskAfterDelay) {
    mini_core::Scheduler scheduler;
    bool flag = false;
    auto task = sleep_flag_task(scheduler, flag, 20ms);

    task.start(scheduler);

    const auto start = std::chrono::steady_clock::now();
    scheduler.run();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(flag);
    EXPECT_TRUE(task.done());
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 15);
}

TEST(SchedulerTest, SleepForZeroDurationDoesNotSuspendTask) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;
    auto first = zero_sleep_task(scheduler, events);
    auto second = simple_event_task(events);

    first.start(scheduler);
    second.start(scheduler);
    scheduler.run();

    EXPECT_EQ(events, (std::vector<int>{1, 3, 2}));
    EXPECT_TRUE(first.done());
    EXPECT_TRUE(second.done());
}

TEST(SchedulerTest, SleepForResumesTimersByEarliestExpiration) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;
    auto slow = sleep_event_task(scheduler, events, 2, 30ms);
    auto fast = sleep_event_task(scheduler, events, 1, 1ms);

    slow.start(scheduler);
    fast.start(scheduler);
    scheduler.run();

    EXPECT_EQ(events, (std::vector<int>{1, 2}));
    EXPECT_TRUE(slow.done());
    EXPECT_TRUE(fast.done());
}
