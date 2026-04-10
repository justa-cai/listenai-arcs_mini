#include "future_promise_test.h"
#include <future>

// 使用promise设置值，future获取值的示例
void promiseFunction(std::promise<int> intPromise) {
    std::cout << "Promise thread: calculating value..." << std::endl;
    // 模拟一些计算工作
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // 设置promise的值
    intPromise.set_value(42);
    std::cout << "Promise thread: value set" << std::endl;
}

// 另一个简单的promise示例函数
void anotherPromiseFunction(std::promise<int> intPromise) {
    std::cout << "Another promise thread: calculating value..." << std::endl;
    // 模拟一些计算工作
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // 设置promise的值
    intPromise.set_value(100);
    std::cout << "Another promise thread: value set" << std::endl;
}

// 使用packaged_task封装函数
int calculateValue(int x) {
    std::cout << "Packaged task: calculating result for " << x << "..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return x * x;
}

// 展示std::async的使用
void asyncExample() {
    std::cout << "\n=== Async Example ===" << std::endl;
    
    // 使用std::async启动异步任务
    std::cout << "Starting async task with launch::async..." << std::endl;
    std::future<int> asyncResult = std::async(std::launch::async, []{
        std::cout << "Async task: working in separate thread..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 100;
    });
    
    // 可以在此处执行其他工作
    std::cout << "Main thread: doing other work while waiting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 获取结果
    std::cout << "Main thread: getting async result..." << std::endl;
    int result = asyncResult.get();
    std::cout << "Main thread: async result is " << result << std::endl;
    
    // 延迟任务示例
    std::cout << "\nStarting async task with launch::deferred..." << std::endl;
    std::future<int> deferredResult = std::async(std::launch::deferred, []{
        std::cout << "Deferred task: executing in caller's thread when get() is called..." << std::endl;
        return 200;
    });
    
    // 调用get()才会执行延迟任务
    std::cout << "Main thread: calling get() on deferred task..." << std::endl;
    result = deferredResult.get();
    std::cout << "Main thread: deferred result is " << result << std::endl;
}

void runFuturePromiseTests() {
    // 基本的Future/Promise 示例
    std::cout << "=== Basic Future/Promise Example ===" << std::endl;
    
    // 创建promise和对应的future
    std::promise<int> intPromise;
    std::future<int> intFuture = intPromise.get_future();
    
    // 启动线程，并传递promise
    std::thread promiseThread(promiseFunction, std::move(intPromise));
    
    // 主线程等待future的值
    std::cout << "Main thread: waiting for value..." << std::endl;
    int value = intFuture.get(); // 阻塞直到promise设置值
    std::cout << "Main thread: received value " << value << std::endl;
    
    promiseThread.join();
    
    // 另一个Future/Promise 示例
    std::cout << "\n=== Another Future/Promise Example ===" << std::endl;
    
    std::promise<int> anotherPromise;
    std::future<int> anotherFuture = anotherPromise.get_future();
    
    std::thread anotherThread(anotherPromiseFunction, std::move(anotherPromise));
    
    std::cout << "Main thread: waiting for another value..." << std::endl;
    int anotherValue = anotherFuture.get();
    std::cout << "Main thread: received another value " << anotherValue << std::endl;
    
    anotherThread.join();
    
    // Packaged Task示例
    std::cout << "\n=== Packaged Task Example ===" << std::endl;
    
    // 创建一个packaged_task，封装calculateValue函数
    std::packaged_task<int(int)> task(calculateValue);
    
    // 获取future
    std::future<int> taskFuture = task.get_future();
    
    // 在另一个线程中执行task
    std::thread taskThread(std::move(task), 5);
    
    // 等待结果
    std::cout << "Main thread: waiting for packaged task result..." << std::endl;
    int taskResult = taskFuture.get();
    std::cout << "Main thread: packaged task result is " << taskResult << std::endl;
    
    taskThread.join();
    
    // 异步任务示例
    asyncExample();
}
