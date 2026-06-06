#include "Task.h"

#include <iostream>

mini_core::Task<int> make_answer() {
    std::cout << "coroutine body: produce answer\n";
    co_return 42;
}

int main() {
    auto task = make_answer();

    std::cout << "after create, done = " << std::boolalpha << task.done() << '\n';

    task.resume();

    std::cout << "after resume, done = " << task.done() << '\n';
    std::cout << "result = " << task.get_result() << '\n';
    return 0;
}
