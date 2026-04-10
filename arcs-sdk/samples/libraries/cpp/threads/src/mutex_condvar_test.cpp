#include "mutex_condvar_test.h"

// 基本互斥锁使用示例
void basicMutexExample() {
    std::cout << "\n=== Basic Mutex Example ===" << std::endl;
    
    std::mutex mtx;
    int counter = 0;
    
    std::cout << "Starting two threads that increment a counter with mutex protection..." << std::endl;
    
    // 创建两个线程，都会增加计数器
    std::thread t1(mutexProtectedCounter, std::ref(mtx), std::ref(counter), 10000);
    std::thread t2(mutexProtectedCounter, std::ref(mtx), std::ref(counter), 10000);
    
    t1.join();
    t2.join();
    
    std::cout << "Final counter value: " << counter << std::endl;
    std::cout << "Expected value: 20000" << std::endl;
}

// 使用互斥锁保护共享数据的函数
void mutexProtectedCounter(std::mutex& mtx, int& counter, int iterations) {
    for (int i = 0; i < iterations; i++) {
        // 锁定互斥锁，确保只有一个线程可以访问计数器
        std::lock_guard<std::mutex> lock(mtx);
        counter++; // 临界区：安全地修改共享变量
    }
}

// 使用条件变量的生产者函数
void producer(std::mutex& mtx, std::condition_variable& cv, std::queue<int>& queue, int items) {
    for (int i = 1; i <= items; i++) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(i);
            std::cout << "Produced: " << i << std::endl;
        }
        // 通知等待的消费者
        cv.notify_one();
        // 生产速度稍慢于消费
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 使用条件变量的消费者函数
void consumer(std::mutex& mtx, std::condition_variable& cv, std::queue<int>& queue, bool& done) {
    while (!done) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 等待队列非空或完成标志
        cv.wait(lock, [&queue, &done]() { 
            return !queue.empty() || done; 
        });
        
        // 检查是否只是因为done标志而被唤醒
        if (queue.empty() && done) {
            break;
        }
        
        // 处理队列中的元素
        int value = queue.front();
        queue.pop();
        
        std::cout << "Consumed: " << value << std::endl;
        
        // 解锁互斥锁，以便生产者可以继续
        lock.unlock();
        
        // 模拟处理数据的时间
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// 展示多个线程的条件变量使用
void multipleConsumersExample() {
    std::cout << "\n=== Multiple Consumers Example ===" << std::endl;
    
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<int> queue;
    std::vector<std::thread> consumers;
    bool done = false;
    
    // 创建多个消费者线程
    std::cout << "Starting 3 consumer threads..." << std::endl;
    for (int i = 0; i < 3; i++) {
        consumers.emplace_back(consumer, std::ref(mtx), std::ref(cv), std::ref(queue), std::ref(done));
    }
    
    // 创建生产者线程
    std::cout << "Starting producer thread..." << std::endl;
    std::thread producerThread(producer, std::ref(mtx), std::ref(cv), std::ref(queue), 15);
    
    // 等待生产者完成
    producerThread.join();
    
    // 通知所有消费者已经没有更多数据了
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();
    
    // 等待所有消费者完成
    for (auto& t : consumers) {
        t.join();
    }
    
    std::cout << "All threads completed successfully" << std::endl;
}

void runMutexCondVarTests() {
    std::cout << "=== Mutex and Condition Variable Tests ===" << std::endl;
    
    // 运行基本互斥锁示例
    basicMutexExample();
    
    // 简单的条件变量示例
    std::cout << "\n=== Basic Condition Variable Example ===" << std::endl;
    
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<int> queue;
    bool done = false;
    
    // 创建消费者线程
    std::cout << "Starting consumer thread..." << std::endl;
    std::thread consumerThread(consumer, std::ref(mtx), std::ref(cv), std::ref(queue), std::ref(done));
    
    // 创建生产者线程
    std::cout << "Starting producer thread..." << std::endl;
    std::thread producerThread(producer, std::ref(mtx), std::ref(cv), std::ref(queue), 5);
    
    // 等待生产者完成
    producerThread.join();
    
    // 通知消费者已经没有更多数据了
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_one();
    
    // 等待消费者完成
    consumerThread.join();
    
    // 运行多消费者示例
    multipleConsumersExample();
}
