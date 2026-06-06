#pragma once
#include <cassert>
#include <type_traits>
#include "Scheduler.h"
#include "Task.h"


namespace mini_core {

template <typename T>
T sync_wait(Scheduler &scheduler, Task<T> task) {
    assert(!task.done());

    task.start(scheduler);
    // TODO: Temporary implementation drains the whole scheduler; replace with run_until(task.done()).
    scheduler.run();

    assert(task.done());

    if constexpr (std::is_void_v<T>) {
        task.get_result();
        return ;
    }
    return task.get_result();
}

}


