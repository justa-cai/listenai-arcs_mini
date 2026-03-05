C++ 多线程示例
===============

功能说明
--------

演示 C++ 标准库中的多线程编程特性，包括线程创建、异常处理、``std::future``/``std::promise`` 异步编程以及互斥锁和条件变量的使用。本示例展示了如何在多线程环境下安全地共享数据和进行线程同步。

硬件连接
--------

无需外部连接，多线程为软件功能，不依赖硬件资源。

示例内容
--------

1. 演示 C++ 异常处理机制（``try-catch``）
2. 演示基本线程创建和同步（``std::thread``）
3. 演示 ``std::future`` 和 ``std::promise`` 异步编程
4. 演示 ``std::packaged_task`` 和 ``std::async`` 的使用
5. 演示互斥锁（``std::mutex``）保护共享数据
6. 演示条件变量（``std::condition_variable``）实现生产者-消费者模式

编译
--------

.. include:: /sample_build.rst

烧录固件
--------

.. include:: /sample_flash.rst

预期输出
--------

.. code-block:: text

   Caught a runtime error: Error occurred
   Thread 1: 1
   Thread 2: 1
   Thread 1: 2
   Thread 2: 2
   Thread 1: 3
   Thread 2: 3
   Thread 1: 4
   Thread 2: 4
   Thread 1: 5
   Thread 2: 5
   Both threads completed.
   
   
   ======= Starting Future/Promise Tests =======
   
   === Basic Future/Promise Example ===
   Promise thread: calculating value...
   Main thread: waiting for value...
   Promise thread: value set
   Main thread: received value 42
   
   === Another Future/Promise Example ===
   Another promise thread: calculating value...
   Main thread: waiting for another value...
   Another promise thread: value set
   Main thread: received another value 100
   
   === Packaged Task Example ===
   Packaged task: calculating result for 5...
   Main thread: waiting for packaged task result...
   Main thread: packaged task result is 25
   
   === Async Example ===
   Starting async task with launch::async...
   Main thread: doing other work while waiting...
   Async task: working in separate thread...
   Main thread: getting async result...
   Main thread: async result is 100
   Starting async task with launch::deferred...
   Main thread: calling get() on deferred task...
   Deferred task: executing in caller's thread when get() is called...
   Main thread: deferred result is 200
   
   ======= Future/Promise Tests Completed =======
   
   
   ======= Starting Mutex/Condition Variable Tests =======
   
   === Mutex and Condition Variable Tests ===
   
   === Basic Mutex Example ===
   Starting two threads that increment a counter with mutex protection...
   Final counter value: 20000
   Expected value: 20000
   
   === Basic Condition Variable Example ===
   Starting consumer thread...
   Starting producer thread...
   Produced: 1
   Consumed: 1
   Produced: 2
   Consumed: 2
   ...
   
   ======= Mutex/Condition Variable Tests Completed =======

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``std::thread``
     - C++11 线程类，用于创建和管理线程
   * - ``std::future``
     - 异步操作结果的占位符
   * - ``std::promise``
     - 用于在线程间传递值或异常
   * - ``std::mutex``
     - 互斥锁，保护共享资源
   * - ``std::condition_variable``
     - 条件变量，用于线程间同步
   * - ``std::async``
     - 异步执行函数并返回 future
   * - ``std::packaged_task``
     - 封装可调用对象，用于异步执行

关键代码
--------

.. code-block:: cpp

   // 异常处理
   try {
       throw std::runtime_error("Error occurred");
   } catch (const std::runtime_error &e) {
       std::cout << "Caught a runtime error: " << e.what() << std::endl;
   }
   
   // 创建和同步线程
   std::thread t1(printNumbers, 1);
   std::thread t2(printNumbers, 2);
   t1.join();
   t2.join();
   
   // 使用 promise 和 future
   std::promise<int> intPromise;
   std::future<int> intFuture = intPromise.get_future();
   std::thread t(promiseFunction, std::move(intPromise));
   int value = intFuture.get();  // 等待并获取值
   
   // 使用互斥锁保护共享数据（推荐使用 lock_guard）
   std::mutex mtx;
   {
       std::lock_guard<std::mutex> lock(mtx);
       // 访问共享数据（自动加锁和解锁）
       counter++;
   }

注意事项
--------

1. **线程同步**: 多个线程访问共享数据时必须使用互斥锁或其他同步机制保护
2. **线程生命周期**: 必须调用 ``join()`` 或 ``detach()`` 来管理线程生命周期，否则程序会异常终止
3. **异常安全**: 在多线程环境中，异常处理需要特别注意，确保资源正确释放
4. **死锁避免**: 使用多个互斥锁时要注意避免死锁，建议按固定顺序获取锁
5. **条件变量**: 使用条件变量时必须配合互斥锁，等待条件时必须使用 ``std::unique_lock``
6. **Future/Promise**: ``std::promise`` 只能设置一次值，多次设置会导致异常

