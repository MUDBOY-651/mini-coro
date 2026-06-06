#pragma once
#include <chrono>
#include <coroutine>
#include <functional>
#include <queue>
#include <thread>

namespace mini_core {


class Scheduler;

class SchedulerAwaiter {

public:
    SchedulerAwaiter(Scheduler& scheduler): scheduler_(scheduler) {}

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept;

    void await_resume() noexcept {}

private:
    Scheduler& scheduler_;
};

class SleepAwaiter {
public:
    SleepAwaiter(Scheduler& scheduler, std::chrono::milliseconds duration): scheduler_(scheduler), duration_(duration) {}

    bool await_ready() noexcept {
        return duration_.count() <= 0;
    }
    void await_suspend(std::coroutine_handle<> h) noexcept;

    void await_resume() noexcept {}

private:
    Scheduler& scheduler_;
    std::chrono::milliseconds duration_;
};


/*
    1. two queues needed：
        - ready queue, deal coroutine handler which is ready to resume.
        - timer priority_queue, deal coroutine handler which need to resume after some time.
    2. run():
        - deal ready queue first, if it is empty, check timer queue if it can pop some coroutine to be resumed.
*/
class Scheduler {
public:
    struct Timer {
        Timer(std::chrono::steady_clock::time_point expire_time, std::coroutine_handle<> handle): expire_time_(expire_time), handle_(handle) {}
        std::chrono::steady_clock::time_point expire_time_;
        std::coroutine_handle<> handle_;

        bool operator > (const Timer& other) const {
            return expire_time_ > other.expire_time_;
        }
    };
    void schedule(std::coroutine_handle<> h) {
        ready_.push(h);
    }

    void schedule_after(std::coroutine_handle<> h, std::chrono::milliseconds duration) {
        timers_.emplace(std::chrono::steady_clock::now() + duration, h);
    }

    void run() {
        while (!ready_.empty() || !timers_.empty()) {
            if (!ready_.empty()) {
                auto h = ready_.front();
                ready_.pop();

                if (!h.done()) {
                    h.resume();
                }
                continue;
            }
            auto [expire_time, h] = timers_.top();
            if (expire_time > std::chrono::steady_clock::now()) {
                std::this_thread::sleep_until(expire_time);
            }
            ready_.push(h);
            timers_.pop();
        }
    }

    SchedulerAwaiter yield() {
        return SchedulerAwaiter{*this};
    }

    SleepAwaiter sleep_for(std::chrono::milliseconds duration) {
        return SleepAwaiter{*this, duration};
    }


private:
    std::queue<std::coroutine_handle<>> ready_;
    std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> timers_;
};

inline void SchedulerAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
    scheduler_.schedule(h);
}

inline void SleepAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
    scheduler_.schedule_after(h, duration_);
}


}
