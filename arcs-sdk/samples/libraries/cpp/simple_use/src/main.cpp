#include <stdio.h>    // C 标准库
#include <functional> // C++11 功能库
#include <iostream> // 简化版iostream实现
#include "class_examples.h"
#include "vector_examples.h"

// 使用 C++11 特性的类
class HelloWorld {
public:
    void sayHello() const {
        printf("printf: Hello C++11 World!\n");
        std::cout << "cout: Hello World\r\n" << std::endl;
    }
    
    static constexpr int VALUE = 42;
};


int main(int argc, char **argv)
{
    HelloWorld hello;
    hello.sayHello();

    demonstrateNewFeatures();
    
    runAllVectorExamples();
    
    printf("Simple Use Completed\r\n");
    return 0;
}
