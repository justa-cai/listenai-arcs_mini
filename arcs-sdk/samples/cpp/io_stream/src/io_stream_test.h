#ifndef IO_STREAM_TEST_H
#define IO_STREAM_TEST_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

// 基本控制台输入输出示例
void basicConsoleIOExample();

// 文件流示例 - 写入文件
void fileOutputExample(const std::string& filename);

// 文件流示例 - 读取文件
void fileInputExample(const std::string& filename);

// 字符串流示例
void stringStreamExample();

// 格式化输出示例
void formattingExample();

// 自定义类型的输入输出流操作符重载
class Point {
private:
    int x, y;
public:
    Point(int x = 0, int y = 0) : x(x), y(y) {}
    
    int getX() const { return x; }
    int getY() const { return y; }
    
    // 友元函数声明，使其能访问私有成员
    friend std::ostream& operator<<(std::ostream& os, const Point& p);
    friend std::istream& operator>>(std::istream& is, Point& p);
};

// 输出运算符重载声明
std::ostream& operator<<(std::ostream& os, const Point& p);

// 输入运算符重载声明
std::istream& operator>>(std::istream& is, Point& p);

// 自定义类型的流示例
void customTypeStreamExample();

// 运行所有IO流测试
void runIOStreamTests();

#endif // IO_STREAM_TEST_H
