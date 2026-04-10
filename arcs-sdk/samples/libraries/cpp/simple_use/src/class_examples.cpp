#include <stdio.h>
#include <memory>   // 智能指针支持

class A {
public:
    A() { printf("A 构造函数调用\n"); }
    ~A() { printf("A 析构函数调用\n"); }
    void display() { printf("这是类 A\n"); }
};

class B {
public:
    B() { printf("B 构造函数调用\n"); }
    ~B() { printf("B 析构函数调用\n"); }
    void display() { printf("这是类 B\n"); }
};

class C {
public:
    C() { printf("C 构造函数调用\n"); }
    ~C() { printf("C 析构函数调用\n"); }
    void display() { printf("这是类 C\n"); }
};

// 演示C++的new多个class的特性
void demonstrateNewFeatures() {
    printf("\n=== C++ new 特性演示 ===\n");
    
    // 1. 基本单对象分配
    printf("\n--- 单个对象分配 ---\n");
    A* pA = new A();
    pA->display();
    delete pA;  // 不要忘记释放内存
    
    // 2. 数组分配
    printf("\n--- 数组分配 ---\n");
    B* arrB = new B[3];  // 创建3个B类对象
    for (int i = 0; i < 3; i++) {
        arrB[i].display();
    }
    delete[] arrB;  // 使用delete[]释放数组
    
    // 3. 不同类型对象的动态分配
    printf("\n--- 不同类型对象分配 ---\n");
    A* pA2 = new A();
    B* pB = new B();
    C* pC = new C();
    
    pA2->display();
    pB->display();
    pC->display();
    
    delete pA2;
    delete pB;
    delete pC;
    
    // 4. 使用C++11智能指针
    printf("\n--- 使用智能指针 ---\n");
    std::unique_ptr<A> uptrA(new A());
    uptrA->display();
    // 自动释放，不需要手动delete
    
    std::shared_ptr<B> sptrB1(new B());
    {
        std::shared_ptr<B> sptrB2 = sptrB1;  // 共享所有权
        sptrB2->display();
    }  // sptrB2离开作用域，但对象不会被销毁，因为sptrB1仍持有引用
    sptrB1->display();
    // 当最后一个shared_ptr离开作用域时，对象会自动被销毁
    
    // 5. 二维数组分配
    printf("\n--- 二维数组分配 ---\n");
    int rows = 2, cols = 3;
    
    // 方法1：一维数组指针的数组
    A** matrix1 = new A*[rows];
    for (int i = 0; i < rows; i++) {
        matrix1[i] = new A[cols];
    }
    
    // 使用二维数组
    matrix1[0][1].display();
    matrix1[1][2].display();
    
    // 释放二维数组内存
    for (int i = 0; i < rows; i++) {
        delete[] matrix1[i];
    }
    delete[] matrix1;
    
    // 6. 使用定位new (placement new)
    printf("\n--- 定位new (placement new) ---\n");
    char* buffer = new char[sizeof(C)];  // 分配原始内存
    C* placementC = new (buffer) C();    // 在分配的内存上构造对象
    
    placementC->display();
    
    placementC->~C();  // 手动调用析构函数
    delete[] buffer;   // 释放原始内存
}
