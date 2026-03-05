C++ 基础使用示例
=================

功能说明
--------

演示 C++ 语言的基础特性，包括类、对象、C++11 新特性以及标准库容器的使用。本示例展示了如何在 ARCS SDK 中使用 C++ 进行开发。

硬件连接
--------

无需外部连接，C++ 语言特性为软件功能，不依赖硬件资源。

示例内容
--------

1. 演示 C++ 类和对象的基本使用
2. 演示 C++11 新特性（``constexpr``、智能指针等）
3. 演示 ``std::vector`` 容器的各种操作
4. 演示动态内存分配（``new``/``delete``）
5. 演示标准输入输出流的使用

编译
--------

.. include:: /sample_build.rst

烧录固件
--------

.. include:: /sample_flash.rst

预期输出
--------

.. code-block:: text

   printf: Hello C++11 World!
   cout: Hello World
   
   === C++ new 特性演示 ===
   
   --- 单个对象分配 ---
   A 构造函数调用
   这是类 A
   A 析构函数调用
   
   --- 数组分配 ---
   B 构造函数调用
   B 构造函数调用
   B 构造函数调用
   这是类 B
   这是类 B
   这是类 B
   B 析构函数调用
   B 析构函数调用
   B 析构函数调用
   
   --- 不同类型对象分配 ---
   A 构造函数调用
   B 构造函数调用
   C 构造函数调用
   这是类 A
   这是类 B
   这是类 C
   A 析构函数调用
   B 析构函数调用
   C 析构函数调用
   
   --- 使用智能指针 ---
   A 构造函数调用
   这是类 A
   A 析构函数调用
   B 构造函数调用
   这是类 B
   这是类 B
   B 析构函数调用
   
   --- 二维数组分配 ---
   A 构造函数调用
   A 构造函数调用
   A 构造函数调用
   A 构造函数调用
   A 构造函数调用
   A 构造函数调用
   这是类 A
   这是类 A
   A 析构函数调用
   A 析构函数调用
   A 析构函数调用
   A 析构函数调用
   A 析构函数调用
   A 析构函数调用
   
   --- 定位new (placement new) ---
   C 构造函数调用
   这是类 C
   C 析构函数调用
   
   === Vector Basic Tests ===
     Number: 1
     Number: 2
     Number: 3
   
   === Vector Initialization Tests ===
   vector with list initialization: size=5
   vector with fill constructor: size=5, first value=100
   vector with range constructor: size=3
   
   === Vector Modification Tests ===
   Adding elements to vector...
     After adding 3 elements: size=3
     After inserting at position 1: 1 99 2 3
     After removing last element: size=3
   
   === Vector Capacity Tests ===
   Empty vector: size=0, capacity=0
   After reserve(10): size=0, capacity=10
   After resize(5, 7): size=5, capacity=10
   
   === Vector Algorithm Tests ===
   Sorted vector: 1 1 2 3 4 5 6 9
   Found element 5 at position: 5
   Count of element 1: 2
   
   Simple Use Completed

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``std::vector``
     - C++ 标准库动态数组容器
   * - ``new``/``delete``
     - C++ 动态内存分配和释放
   * - ``std::cout``
     - C++ 标准输出流
   * - ``std::unique_ptr``
     - C++11 智能指针，自动管理内存
   * - ``constexpr``
     - C++11 编译时常量表达式

关键代码
--------

.. code-block:: cpp

   // 使用 C++11 特性的类
   class HelloWorld {
   public:
       void sayHello() const {
           printf("printf: Hello C++11 World!\n");
           std::cout << "cout: Hello World\r\n" << std::endl;
       }
       
       static constexpr int VALUE = 42;
   };
   
   // 使用 vector 容器
   std::vector<int> numbers = {1, 2, 3};
   for (const auto& num : numbers) {
       printf("  Number: %d\n", num);
   }
   
   // 动态内存分配
   A* pA = new A();
   pA->display();
   delete pA;

注意事项
--------

1. **内存管理**: 使用 ``new`` 分配的内存必须使用 ``delete`` 释放，使用 ``new[]`` 分配的内存必须使用 ``delete[]`` 释放
2. **智能指针**: 推荐使用 ``std::unique_ptr`` 等智能指针自动管理内存，避免内存泄漏
3. **容器使用**: ``std::vector`` 等容器会自动管理内存，无需手动释放
4. **C++11 特性**: 本示例使用了 C++11 标准特性，确保编译器支持 C++11 或更高版本
5. **混合使用**: 示例中混合使用了 C 标准库（``printf``）和 C++ 标准库（``std::cout``），两者可以共存使用

