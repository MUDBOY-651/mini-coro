# mini-coro

`mini-coro` 是一个学习型 C++20 协程框架。它当前只追求把协程最核心的几块积木讲清楚：`Task<T>`、惰性启动、`co_await Task` continuation、单线程 `Scheduler`、`yield()`、`sleep_for()` 和 `sync_wait()`。

## Build

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

运行最小示例：

```bash
./build/mini_coro
```

## Reading Path

- [Task 基础模型](docs/01-task-basics.md)
- [Awaiter 与 Scheduler 模型](docs/03-scheduler.md)

## Current Scope

- `Task<T>` / `Task<void>` 持有并销毁 coroutine frame。
- `initial_suspend()` 使用 `std::suspend_always`，所以任务默认惰性启动。
- `final_suspend()` 返回 continuation，让被 `co_await` 的子任务完成后恢复父协程。
- `Scheduler` 是单线程 FIFO ready queue 加 timer queue 的教学版调度器。
- `sync_wait()` 可以作为顶层入口启动任务。

当前实现还没有生产级异常传播、取消、线程安全或 I/O reactor；这些都留给后续阶段逐步加入。
