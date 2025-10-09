#include "vector_examples.h"
#include <vector>     // C++11 容器
#include <algorithm>  // For vector algorithms
#include <stdio.h>    // C 标准库

// 演示向量的基本用法
void demonstrateVectorBasic() {
    printf("\n=== Vector Basic Tests ===\n");
    std::vector<int> numbers = {1, 2, 3};
    for (const auto& num : numbers) {
        printf("  Number: %d\n", num);
    }
}

// 演示向量的各种初始化方法
void demonstrateVectorInitialization() {
    printf("\n=== Vector Initialization Tests ===\n");
    // 使用列表初始化
    std::vector<int> vec1 = {10, 20, 30, 40, 50};
    printf("vector with list initialization: size=%zu\n", vec1.size());
    
    // 使用填充构造函数
    std::vector<int> vec2(5, 100);  // 5个元素，每个值为100
    printf("vector with fill constructor: size=%zu, first value=%d\n", 
           vec2.size(), vec2.front());
    
    // 使用范围构造函数
    std::vector<int> vec3(vec1.begin(), vec1.begin() + 3);
    printf("vector with range constructor: size=%zu\n", vec3.size());
}

// 演示向量的修改操作（添加、插入、删除元素）
void demonstrateVectorModification() {
    printf("\n=== Vector Modification Tests ===\n");
    std::vector<int> vec4;
    
    // 添加元素
    printf("Adding elements to vector...\n");
    vec4.push_back(1);
    vec4.push_back(2);
    vec4.push_back(3);
    printf("  After adding 3 elements: size=%zu\n", vec4.size());
    
    // 插入元素
    vec4.insert(vec4.begin() + 1, 99);
    printf("  After inserting at position 1: ");
    for (const auto& n : vec4) {
        printf("%d ", n);
    }
    printf("\n");
    
    // 删除元素
    vec4.pop_back();
    printf("  After removing last element: size=%zu\n", vec4.size());
}

// 演示向量的容量和大小相关操作
void demonstrateVectorCapacity() {
    printf("\n=== Vector Capacity Tests ===\n");
    std::vector<int> vec5;
    printf("Empty vector: size=%zu, capacity=%zu\n", vec5.size(), vec5.capacity());
    
    // 预分配内存
    vec5.reserve(10);
    printf("After reserve(10): size=%zu, capacity=%zu\n", vec5.size(), vec5.capacity());
    
    // 调整大小
    vec5.resize(5, 7);
    printf("After resize(5, 7): size=%zu, capacity=%zu\n", vec5.size(), vec5.capacity());
}

// 演示向量的算法操作（排序、查找、计数）
void demonstrateVectorAlgorithms() {
    printf("\n=== Vector Algorithm Tests ===\n");
    std::vector<int> vec6 = {3, 1, 4, 1, 5, 9, 2, 6};
    
    // 排序
    std::sort(vec6.begin(), vec6.end());
    printf("Sorted vector: ");
    for (const auto& n : vec6) {
        printf("%d ", n);
    }
    printf("\n");
    
    // 查找
    auto it = std::find(vec6.begin(), vec6.end(), 5);
    if (it != vec6.end()) {
        printf("Found element 5 at position: %ld\n", it - vec6.begin());
    } else {
        printf("Element 5 not found\n");
    }
    
    // 计算元素数量
    int count = std::count(vec6.begin(), vec6.end(), 1);
    printf("Count of element 1: %d\n", count);
}

// 运行所有向量示例
void runAllVectorExamples() {
    demonstrateVectorBasic();
    demonstrateVectorInitialization();
    demonstrateVectorModification();
    demonstrateVectorCapacity();
    demonstrateVectorAlgorithms();
}
