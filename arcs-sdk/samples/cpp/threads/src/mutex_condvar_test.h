#ifndef MUTEX_CONDVAR_TEST_H
#define MUTEX_CONDVAR_TEST_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <vector>

// 基本互斥锁使用示例函数
void basicMutexExample();

// 使用互斥锁保护共享数据的函数
void mutexProtectedCounter(std::mutex& mtx, int& counter, int iterations);

// 使用条件变量的生产者函数
void producer(std::mutex& mtx, std::condition_variable& cv, std::queue<int>& queue, int items);

// 使用条件变量的消费者函数
void consumer(std::mutex& mtx, std::condition_variable& cv, std::queue<int>& queue, bool& done);

// 展示多个线程的条件变量使用
void multipleConsumersExample();

// 运行所有mutex和condition_variable测试
void runMutexCondVarTests();

#endif // MUTEX_CONDVAR_TEST_H
