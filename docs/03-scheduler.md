# Awaiter 与 Scheduler 总览

这一阶段关注调度：协程挂起后，谁负责把它恢复执行。

文档拆成两块来看：

- [Awaiter 模型](03a-awaiter-model.md)：解释 `await_ready()`、`await_suspend()`、`await_resume()`，以及 `Task` / `SchedulerAwaiter` / `SleepAwaiter` 如何接入 `co_await`。
- [Scheduler 模型](03b-scheduler-model.md)：解释单线程 ready queue、timer queue、`schedule()`、`yield()`、`sleep_for()` 和 `run()`。

二者关系可以简化成一句话：

```text
Awaiter 决定当前协程如何挂起，Scheduler 决定挂起的 coroutine_handle 何时恢复。
```

当前实现仍然是教学版单线程模型。它不创建线程，也不并行执行协程；所有 `resume()` 都发生在调用 `Scheduler::run()` 的线程中。
