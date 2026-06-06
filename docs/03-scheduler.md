# Awaiter 与 Scheduler 模型

这一阶段关注调度：协程挂起后，谁负责把它恢复执行。

当前 `Scheduler` 是单线程教学版调度器，包含两个队列：

- ready queue：已经可以立即恢复的 coroutine handle。
- timer queue：需要等到某个时间点后再恢复的 coroutine handle。

## Awaitable 三件套

一个对象能被 `co_await`，通常需要提供：

```cpp
bool await_ready();
void await_suspend(std::coroutine_handle<> h);
void await_resume();
```

三者含义是：

- `await_ready()`：是否已经准备好。如果返回 `true`，当前协程不会挂起。
- `await_suspend(h)`：当前协程真的要挂起时调用，`h` 是当前协程的 handle。
- `await_resume()`：当前协程恢复后调用，用来返回结果或抛出异常。

`Scheduler` 提供两个 awaiter：`SchedulerAwaiter` 和 `SleepAwaiter`。

## schedule()

`schedule()` 是最底层的入队 API：

```cpp
void schedule(std::coroutine_handle<> h);
```

它把一个 coroutine handle 放入 ready queue。之后 `Scheduler::run()` 会从 ready queue 取出 handle，并调用 `resume()`。

当前 `Task::start(scheduler)` 就是调用 `scheduler.schedule(handle_)`，把惰性任务交给调度器启动。

## yield()

`yield()` 的语义是：当前协程主动让出执行权，并把自己重新排到 ready queue 尾部。

```cpp
co_await scheduler.yield();
```

流程：

```text
协程执行到 co_await scheduler.yield()
await_ready() 返回 false
await_suspend(current_handle)
current_handle 被 schedule 到 ready queue 尾部
调度器之后再次 resume 它
await_resume()
协程继续往下执行
```

这和线程没有关系，只是当前协程把自己的 handle 交还给单线程 scheduler。

## sleep_for()

`sleep_for()` 的语义是：当前协程挂起一段时间，到期后再恢复。

```cpp
co_await scheduler.sleep_for(std::chrono::milliseconds{10});
```

流程：

```text
协程执行到 co_await scheduler.sleep_for(...)
await_ready() 检查 duration 是否 <= 0
如果需要等待，await_suspend(current_handle)
current_handle 被放入 timer queue
run() 在 timer 到期后把它移入 ready queue
之后 resume 该协程
```

`sleep_for(0ms)` 会让 `await_ready()` 返回 `true`，因此不会挂起，协程会同步继续执行。

## run()

`run()` 是当前调度器的主循环：

```text
while ready queue 或 timer queue 非空:
    如果 ready queue 非空:
        取出一个 handle 并 resume
    否则:
        查看最早到期的 timer
        必要时 sleep_until
        到期后移入 ready queue
```

这是一个单线程模型。`resume()` 调用发生在同一个线程中，协程之间不会并行执行。

## 当前限制

当前实现故意保持简单，有几个教学版限制：

- `Scheduler` 不是线程安全的。
- `run()` 会一直运行到 ready queue 和 timer queue 都清空。
- 如果 ready queue 一直有任务，timer 只有在 ready queue 清空后才会被检查。
- `Task` 仍然拥有 coroutine frame；把裸 handle 放入 scheduler 后，要保证 `Task` 对象活到任务完成。

这些限制有助于先看清协程和调度器的基本关系，后续可以再加入 `run_once()`、`run_until()`、取消、异常传播和更完整的生命周期管理。
