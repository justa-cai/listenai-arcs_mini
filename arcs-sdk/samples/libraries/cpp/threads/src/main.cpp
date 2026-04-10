#include <iostream>
#include <thread>
#include "future_promise_test.h"
#include "mutex_condvar_test.h"

void printNumbers(int threadID) {
    for (int i = 1; i <= 5; ++i) {
        std::cout << "Thread " << threadID << ": " << i << std::endl;
    }
}

int main(int argc, char **argv)
{
    try {
        throw std::runtime_error("Error occurred");
    } catch (const std::runtime_error &e) {
        std::cout << "Caught a runtime error: " << e.what() << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Caught a standard exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Caught an unknown exception" << std::endl;
    }

    std::thread t1(printNumbers, 1);
    std::thread t2(printNumbers, 2);

    t1.join();
    t2.join();

    std::cout << "Both threads completed." << std::endl;

    // 运行Future和Promise的测试
    std::cout << "\n\n======= Starting Future/Promise Tests =======\n" << std::endl;
    runFuturePromiseTests();
    std::cout << "\n======= Future/Promise Tests Completed =======\n" << std::endl;

    // 运行Mutex和Condition Variable的测试
    std::cout << "\n\n======= Starting Mutex/Condition Variable Tests =======\n" << std::endl;
    runMutexCondVarTests();
    std::cout << "\n======= Mutex/Condition Variable Tests Completed =======\n" << std::endl;

    return 0;
}
