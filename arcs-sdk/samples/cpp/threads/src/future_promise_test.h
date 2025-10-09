#ifndef FUTURE_PROMISE_TEST_H
#define FUTURE_PROMISE_TEST_H

#include <iostream>
#include <thread>
#include <future>
#include <chrono>

// 使用promise设置值，future获取值的示例
void promiseFunction(std::promise<int> intPromise);

// 带有异常处理的promise示例
void promiseFunctionWithException(std::promise<int> intPromise);

// 使用packaged_task封装函数
int calculateValue(int x);

// 展示std::async的使用
void asyncExample();

// 运行所有future和promise测试
void runFuturePromiseTests();

#endif // FUTURE_PROMISE_TEST_H
