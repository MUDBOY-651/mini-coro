# Task 基础模型

这一阶段关注一个问题：一个返回 `Task<T>` 的协程是怎样被创建、挂起、恢复、返回结果并销毁的。

## 最小示例

```cpp
mini_core::Task<int> make_answer() {
    co_return 42;
}

int main() {
    auto task = make_answer();
    task.resume();
    int value = task.get_result();
}
```

调用 `make_answer()` 时，函数体不会立刻执行。编译器会创建 coroutine frame，并通过 `promise_type::get_return_object()` 返回一个 `Task<int>`。当前实现的 `initial_suspend()` 返回 `std::suspend_always`，所以协程停在入口处，直到外部调用 `resume()`。

## promise_type

`Task<T>` 通过下面这行告诉编译器它的 promise 类型：

```cpp
using promise_type = TaskPromise<T>;
```

当一个函数返回 `Task<T>` 且内部使用 `co_return`、`co_await` 或 `co_yield` 时，编译器会把它改写成协程，并在协程帧中放入 `TaskPromise<T>`。

当前 `TaskPromise<T>` 负责几件事：

- `get_return_object()`：把 promise 包装成 `Task<T>` 返回给调用者。
- `initial_suspend()`：决定协程创建后是否立即执行。
- `final_suspend()`：决定协程结束后如何交还控制权。
- `return_value()` / `return_void()`：保存 `co_return` 的结果。
- `unhandled_exception()`：处理未捕获异常。

## coroutine_handle

`std::coroutine_handle<promise_type>` 是外部控制协程帧的句柄。`Task<T>` 保存这个 handle，并提供：

- `resume()`：继续执行协程。
- `done()`：检查协程是否结束。
- `get_result()`：从 promise 中取出结果。
- 析构函数：销毁协程帧。

这意味着当前 `Task<T>` 是 coroutine frame 的所有者。它不可拷贝，只能移动，避免多个对象同时销毁同一个协程帧。

## 惰性协程

当前 `initial_suspend()` 返回：

```cpp
std::suspend_always initial_suspend() { return {}; }
```

所以：

```cpp
auto task = make_answer();
```

只会创建协程，不会执行函数体。真正执行发生在：

```cpp
task.resume();
```

这让 `Task` 可以先被创建、组合、放入 scheduler，然后再由外部决定何时启动。

## co_return

对 `Task<int>` 来说：

```cpp
co_return 42;
```

会调用：

```cpp
void return_value(T val);
```

当前实现直接把结果保存到 promise 的 `val_` 中。随后外部可以在协程完成后调用 `get_result()` 读取。

对 `Task<void>` 来说，`co_return;` 会调用 `return_void()`。

## final_suspend 与 continuation

`final_suspend()` 很关键：协程执行到末尾时，协程帧不能马上销毁，因为外部还需要读取结果。因此它必须挂起在 final suspend 点。

当前实现还支持 continuation：当 `outer` 里 `co_await inner()` 时，`inner` 的 promise 会保存 `outer` 的 coroutine handle。`inner` 完成后，`final_suspend()` 返回这个 continuation，运行权就回到 `outer`。

简化流程：

```text
outer resume
outer co_await inner
保存 outer handle 到 inner promise
切到 inner handle 执行
inner co_return
inner final_suspend 返回 outer handle
outer 继续执行
```

这就是为什么 continuation 要在 `final_suspend()` 返回：只有此时子协程已经完成，父协程才可以安全地继续读取结果。

## co_await Task

`Task<T>` 自己也是 awaiter，提供三件套：

```cpp
bool await_ready();
std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation);
T await_resume();
```

当前语义是：

- `await_ready()`：如果任务已经完成，就不挂起。
- `await_suspend()`：保存当前协程作为 continuation，并返回被等待任务的 handle，让它开始执行。
- `await_resume()`：被等待任务完成后，从 promise 中取出结果。

所以 `co_await Task<T>` 并不创建线程，它只是把执行权从当前协程切到另一个协程，再在完成时切回来。
