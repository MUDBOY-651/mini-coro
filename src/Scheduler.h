#pragma once
#include <coroutine>
#include <queue>

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



class Scheduler {
public:
    void schedule(std::coroutine_handle<> h) {
        ready_.push(h);
    }

    void run() {
        while (!ready_.empty()) {
            auto h = ready_.front();
            ready_.pop();

            if (!h.done()) {
                h.resume();
            }
        }
    }

    SchedulerAwaiter yield() {
        return SchedulerAwaiter{*this};
    }


private:
    std::queue<std::coroutine_handle<>> ready_;

};

inline void SchedulerAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
    scheduler_.schedule(h);
}


}
