#include "io_stream_test.h"

// 基本控制台输入输出示例
void basicConsoleIOExample() {
    std::cout << "\n=== Basic Console I/O Example ===" << std::endl;
    
    // 基本输出
    std::cout << "这是标准输出流示例。" << std::endl;
    
    // 也可以使用std::endl来刷新缓冲区
    std::cout << "数字输出: " << 42 << std::endl;
    
    // 错误输出流
    std::cerr << "这是标准错误流示例。" << std::endl;
    
    // 在嵌入式环境中我们一般不使用交互式输入
    std::cout << "嵌入式系统中一般通过其他方式如UART、文件或消息队列获取输入" << std::endl;
}

// 文件流示例 - 写入文件
void fileOutputExample(const std::string& filename) {
    std::cout << "\n=== File Output Stream Example ===" << std::endl;
    
    printf("before outFile: %s\n", filename.c_str());
    // 创建输出文件流
    std::ofstream outFile(filename);

    printf("filename: %s\n", filename.c_str());
    
    if (!outFile) {
        std::cerr << "无法打开文件进行写入: " << filename << std::endl;
        return;
    }
    
    // 写入文本内容
    outFile << "这是第一行文本。" << std::endl;
    outFile << "这是第二行文本。" << std::endl;
    outFile << "数字: " << 12345 << std::endl;
    
    // 写入一个数字列表
    for (int i = 1; i <= 5; i++) {
        outFile << "项目 " << i << ": " << (i * i) << std::endl;
    }
    
    // 文件流会在销毁时自动关闭，但显式关闭更清晰
    outFile.close();
    
    std::cout << "成功写入文件: " << filename << std::endl;
}

// 文件流示例 - 读取文件
void fileInputExample(const std::string& filename) {
    std::cout << "\n=== File Input Stream Example ===" << std::endl;
    
    // 创建输入文件流
    std::ifstream inFile(filename);
    
    if (!inFile) {
        std::cerr << "无法打开文件进行读取: " << filename << std::endl;
        return;
    }
    
    std::cout << "文件 " << filename << " 内容:" << std::endl;
    
    // 逐行读取文件
    std::string line;
    while (std::getline(inFile, line)) {
        std::cout << "  " << line << std::endl;
    }
    
    inFile.close();
}

// 字符串流示例
void stringStreamExample() {
    std::cout << "\n=== String Stream Example ===" << std::endl;
    
    // 输出字符串流
    std::ostringstream oss;
    oss << "这是一个字符串流。" << std::endl;
    oss << "可以像使用其他ostream一样使用它:" << std::endl;
    oss << "整数: " << 42 << std::endl;
    oss << "浮点数: " << 3.14159 << std::endl;
    
    // 获取流中的字符串
    std::string result = oss.str();
    std::cout << "ostringstream的内容:\n" << result << std::endl;
    
    // 输入字符串流
    std::string data = "12 34 56 78";
    std::istringstream iss(data);
    
    int a, b, c, d;
    iss >> a >> b >> c >> d;
    
    std::cout << "从istringstream解析的数值:" << std::endl;
    std::cout << "a = " << a << ", b = " << b << ", c = " << c << ", d = " << d << std::endl;
    std::cout << "总和: " << (a + b + c + d) << std::endl;
}

// 格式化输出示例
void formattingExample() {
    std::cout << "\n=== Formatting Output Example ===" << std::endl;
    
    // 设置浮点数精度
    double pi = 3.1415926535897932384626;
    
    std::cout << "默认精度的π: " << pi << std::endl;
    std::cout << "设置精度为10的π: " << std::setprecision(10) << pi << std::endl;
    
    // 字段宽度
    std::cout << "默认字段宽度:" << std::endl;
    std::cout << 42 << std::endl;
    std::cout << "设置字段宽度为10:" << std::endl;
    std::cout << std::setw(10) << 42 << std::endl;
    
    // 填充字符
    std::cout << "设置填充字符为'*'并左对齐:" << std::endl;
    std::cout << std::left << std::setfill('*') << std::setw(10) << 42 << std::endl;
    
    // 恢复默认格式
    std::cout.unsetf(std::ios::left);
    std::cout << std::setfill(' ');
    
    // 不同数值进制
    int value = 255;
    std::cout << "十进制: " << std::dec << value << std::endl;
    std::cout << "十六进制: " << std::hex << value << std::endl;
    std::cout << "八进制: " << std::oct << value << std::endl;
    
    // 重置为十进制
    std::cout << std::dec;
    
    // 布尔值格式化
    std::cout << "默认布尔值输出: " << true << ", " << false << std::endl;
    std::cout << "字面布尔值输出: " << std::boolalpha << true << ", " << false << std::endl;
    
    // 恢复默认布尔值格式
    std::cout << std::noboolalpha;
}

// 输出运算符重载实现
std::ostream& operator<<(std::ostream& os, const Point& p) {
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

// 输入运算符重载实现
std::istream& operator>>(std::istream& is, Point& p) {
    char dummy;
    is >> dummy >> p.x >> dummy >> p.y >> dummy;
    return is;
}

// 自定义类型的流示例
void customTypeStreamExample() {
    std::cout << "\n=== Custom Type Stream Example ===" << std::endl;
    
    // 使用重载的输出运算符
    Point p1(10, 20);
    std::cout << "点p1: " << p1 << std::endl;
    
    // 点集合
    std::vector<Point> points = {Point(1, 2), Point(3, 4), Point(5, 6)};
    std::cout << "点集合:" << std::endl;
    for (const auto& p : points) {
        std::cout << "  " << p << std::endl;
    }
    
    // 使用预设数据创建点
    Point p2(7, 8);
    std::cout << "预设的点: " << p2 << std::endl;
}

// 文件异常处理示例 - 尝试打开不存在的文件
void fileExceptionExample() {
    std::cout << "\n=== File Exception Handling Example ===" << std::endl;
    
    // 定义一个不存在的文件名
    std::string nonExistentFile = "this_file_does_not_exist.txt";
    
    std::cout << "尝试打开不存在的文件: " << nonExistentFile << std::endl;
    
    try {
        // 尝试打开文件，并使用exceptions方法设置异常掩码
        std::ifstream file(nonExistentFile);
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        
        // 如果文件打开失败，上面的行将抛出异常，以下代码不会执行
        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << std::endl;
        }
        file.close();
    }
    catch (const std::ios_base::failure& e) {
        std::cerr << "捕获到文件操作异常: " << e.what() << std::endl;
        std::cerr << "文件 '" << nonExistentFile << "' 可能不存在或无法访问" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "捕获到标准异常: " << e.what() << std::endl;
    }
    
    std::cout << "\n另一种异常处理方式 - 使用ifstream构造后立即检查状态:" << std::endl;
    
    try {
        std::ifstream file2(nonExistentFile);
        
        if (!file2.is_open()) {
            throw std::runtime_error("无法打开文件: " + nonExistentFile);
        }
        
        // 如果文件打开失败，以下代码不会执行
        std::string content;
        file2 >> content;
        std::cout << "文件内容: " << content << std::endl;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "捕获到运行时异常: " << e.what() << std::endl;
    }
}

void runIOStreamTests() {
    std::cout << "=== IO Stream Tests ===" << std::endl;
    
    // 运行基本控制台输入输出示例
    basicConsoleIOExample();
    
    // 运行文件流示例
    std::string testFile = "test_file.txt";
    fileOutputExample(testFile);
    fileInputExample(testFile);
    
    // 运行文件异常处理示例
    fileExceptionExample();
    
    // 运行字符串流示例
    stringStreamExample();
    
    // 运行格式化输出示例
    formattingExample();
    
    // 运行自定义类型的流示例
    customTypeStreamExample();
}
