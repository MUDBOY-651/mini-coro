# Awaiter 模型

一个对象能被 `co_await`，核心是提供 awaitable 三件套：

```cpp
bool await_ready();
auto await_suspend(std::coroutine_handle<> h);
auto await_resume();
```

## await_ready()

`await_ready()` 用来判断是否需要挂起当前协程。

- 返回 `true`：不挂起，直接继续执行，并调用 `await_resume()`。
- 返回 `false`：当前协程会挂起，随后调用 `await_suspend()`。

例如 `SleepAwaiter::await_ready()` 会检查 duration：

```cpp
bool await_ready() noexcept {
    return duration_.count() <= 0;
}
```

所以 `sleep_for(0ms)` 不会挂起当前协程。

## await_suspend()

`await_suspend()` 在当前协程确认要挂起时调用。参数 `h` 是当前协程的 `coroutine_handle`。

当前项目里有两类常见行为。

第一类是把当前协程交给调度器：

```cpp
inline void SchedulerAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
    scheduler_.schedule(h);
}
```

这表示当前协程先挂起，然后把自己的 handle 放到 ready queue，等待 scheduler 后续恢复。

第二类是 `Task<T>` 等待另一个 `Task<T>`：

```cpp
std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
    handle_.promise().continuation_ = continuation;
    return handle_;
}
```

这里保存当前协程作为 continuation，并返回被等待任务的 handle。返回 handle 的含义是：当前协程挂起后，马上切去执行这个返回的协程。

## await_resume()

`await_resume()` 在协程恢复之后调用，用来产生 `co_await` 表达式的结果，或者重新抛出异常。

当前 `Task<T>::await_resume()` 会：

- 确认被等待任务已经完成。
- 如果 promise 保存了异常，就重新抛出。
- 对 `Task<T>` 返回保存的结果。
- 对 `Task<void>` 什么也不返回。

这让下面的代码能像同步代码一样写：

```cpp
int value = co_await child_task();
```

如果 `child_task()` 内部抛出异常，异常会在 `await_resume()` 中重新抛给外层协程。

## 当前 Awaiter

当前项目有三种主要 awaiter：

- `Task<T>`：等待另一个任务完成，并通过 continuation 回到父协程。
- `SchedulerAwaiter`：由 `scheduler.yield()` 返回，把当前协程重新排到 ready queue 尾部。
- `SleepAwaiter`：由 `scheduler.sleep_for(duration)` 返回，把当前协程放入 timer queue。

## 小结

`co_await` 不等于新线程。它的本质是：

```text
把当前协程的 handle 交给某个 awaiter，由 awaiter 决定现在是否挂起、挂起后交给谁、恢复后返回什么。
```
