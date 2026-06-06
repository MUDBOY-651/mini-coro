#include "SyncWait.h"

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;

mini_core::Task<int> value_task(int value) {
    co_return value;
}

mini_core::Task<void> set_flag_task(bool& flag) {
    flag = true;
    co_return;
}

mini_core::Task<int> yield_then_value_task(
    mini_core::Scheduler& scheduler,
    std::vector<int>& events
) {
    events.push_back(1);
    co_await scheduler.yield();
    events.push_back(2);
    co_return 42;
}

mini_core::Task<int> sleep_then_value_task(
    mini_core::Scheduler& scheduler,
    bool& resumed
) {
    co_await scheduler.sleep_for(1ms);
    resumed = true;
    co_return 7;
}

mini_core::Task<void> push_event_task(std::vector<int>& events, int event) {
    events.push_back(event);
    co_return;
}

} // namespace

TEST(SyncWaitTest, ReturnsValueTaskResult) {
    mini_core::Scheduler scheduler;

    EXPECT_EQ(mini_core::sync_wait(scheduler, value_task(13)), 13);
}

TEST(SyncWaitTest, WaitsForVoidTaskCompletion) {
    mini_core::Scheduler scheduler;
    bool flag = false;

    mini_core::sync_wait(scheduler, set_flag_task(flag));

    EXPECT_TRUE(flag);
}

TEST(SyncWaitTest, RunsTaskAcrossYieldPoints) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;

    const auto result = mini_core::sync_wait(scheduler, yield_then_value_task(scheduler, events));

    EXPECT_EQ(result, 42);
    EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(SyncWaitTest, RunsTaskAcrossSleepFor) {
    mini_core::Scheduler scheduler;
    bool resumed = false;

    const auto result = mini_core::sync_wait(scheduler, sleep_then_value_task(scheduler, resumed));

    EXPECT_EQ(result, 7);
    EXPECT_TRUE(resumed);
}

TEST(SyncWaitTest, TemporaryImplementationDrainsExistingSchedulerWork) {
    mini_core::Scheduler scheduler;
    std::vector<int> events;
    auto unrelated = push_event_task(events, 1);
    unrelated.start(scheduler);

    EXPECT_EQ(mini_core::sync_wait(scheduler, value_task(5)), 5);

    EXPECT_EQ(events, (std::vector<int>{1}));
    EXPECT_TRUE(unrelated.done());
}
