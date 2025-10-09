#include <iostream>
#include <thread>
#include "io_stream_test.h"
#include "fs.h"

void printNumbers(int threadID) {
    for (int i = 1; i <= 5; ++i) {
        std::cout << "Thread " << threadID << ": " << i << std::endl;
    }
}

int main(int argc, char **argv)
{
    test_lsfs_init();

    // 运行IO Stream的测试
    std::cout << "\n\n======= Starting IO Stream Tests =======\n" << std::endl;
    runIOStreamTests();
    std::cout << "\n======= IO Stream Tests Completed =======\n" << std::endl;

    return 0;
}
