C++ IO 流示例
==============

功能说明
--------

演示 C++ 标准库中的输入输出流（IO Stream）功能，包括控制台输入输出、文件流操作、字符串流以及格式化输出。本示例展示了如何使用 C++ 流式 API 进行各种 IO 操作。

硬件连接
--------

无需外部连接，IO 流为软件功能。如果使用文件流功能，需要确保文件系统已正确初始化。

示例内容
--------

1. 基本控制台输入输出（``std::cout``、``std::cerr``）
2. 文件输出流（``std::ofstream``）写入文件
3. 文件输入流（``std::ifstream``）读取文件
4. 文件异常处理
5. 字符串流（``std::ostringstream``、``std::istringstream``）
6. 格式化输出（精度、宽度、填充、进制等）
7. 自定义类型的流操作符重载

编译
--------

.. include:: /sample_build.rst

烧录固件
--------

.. include:: /sample_flash.rst

预期输出
--------

.. code-block:: text

   ======= Starting IO Stream Tests =======
   
   === IO Stream Tests ===
   
   === Basic Console I/O Example ===
   这是标准输出流示例。
   数字输出: 42
   这是标准错误流示例。
   嵌入式系统中一般通过其他方式如UART、文件或消息队列获取输入
   
   === File Output Stream Example ===
   before outFile: test_file.txt
   filename: test_file.txt
   成功写入文件: test_file.txt
   
   === File Input Stream Example ===
   文件 test_file.txt 内容:
     这是第一行文本。
     这是第二行文本。
     数字: 12345
     项目 1: 1
     项目 2: 4
     项目 3: 9
     项目 4: 16
     项目 5: 25
   
   === File Exception Handling Example ===
   尝试打开不存在的文件: this_file_does_not_exist.txt
   捕获到文件操作异常: ...
   文件 'this_file_does_not_exist.txt' 可能不存在或无法访问
   
   另一种异常处理方式 - 使用ifstream构造后立即检查状态:
   捕获到运行时异常: 无法打开文件: this_file_does_not_exist.txt
   
   === String Stream Example ===
   ostringstream的内容:
   这是一个字符串流。
   可以像使用其他ostream一样使用它:
   整数: 42
   浮点数: 3.14159
   从istringstream解析的数值:
   a = 12, b = 34, c = 56, d = 78
   总和: 180
   
   === Formatting Output Example ===
   默认精度的π: 3.14159
   设置精度为10的π: 3.141592654
   默认字段宽度:
   42
   设置字段宽度为10:
           42
   设置填充字符为'*'并左对齐:
   42********
   十进制: 255
   十六进制: ff
   八进制: 377
   默认布尔值输出: 1, 0
   字面布尔值输出: true, false
   
   === Custom Type Stream Example ===
   点p1: (10, 20)
   点集合:
     (1, 2)
     (3, 4)
     (5, 6)
   预设的点: (7, 8)
   
   ======= IO Stream Tests Completed =======

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``std::cout``
     - 标准输出流
   * - ``std::cerr``
     - 标准错误流
   * - ``std::ofstream``
     - 文件输出流，用于写入文件
   * - ``std::ifstream``
     - 文件输入流，用于读取文件
   * - ``std::ostringstream``
     - 输出字符串流
   * - ``std::istringstream``
     - 输入字符串流
   * - ``std::setprecision()``
     - 设置浮点数精度
   * - ``std::setw()``
     - 设置字段宽度
   * - ``std::setfill()``
     - 设置填充字符
   * - ``operator<<``
     - 输出操作符重载
   * - ``operator>>``
     - 输入操作符重载

关键代码
--------

.. code-block:: cpp

   // 基本控制台输出
   std::cout << "这是标准输出流示例。" << std::endl;
   std::cout << "数字输出: " << 42 << std::endl;
   std::cerr << "这是标准错误流示例。" << std::endl;
   
   // 文件输出流（示例：函数参数为 filename）
   std::ofstream outFile(filename);
   if (!outFile) {
       std::cerr << "无法打开文件进行写入: " << filename << std::endl;
       return;
   }
   outFile << "这是第一行文本。" << std::endl;
   outFile.close();
   
   // 文件输入流（示例：函数参数为 filename）
   std::ifstream inFile(filename);
   if (!inFile) {
       std::cerr << "无法打开文件进行读取: " << filename << std::endl;
       return;
   }
   std::string line;
   while (std::getline(inFile, line)) {
       std::cout << "  " << line << std::endl;
   }
   inFile.close();
   
   // 字符串流
   std::ostringstream oss;
   oss << "整数: " << 42 << std::endl;
   std::string result = oss.str();
   
   std::istringstream iss("12 34 56 78");
   int a, b, c, d;
   iss >> a >> b >> c >> d;
   
   // 格式化输出
   std::cout << std::setprecision(10) << pi << std::endl;
   std::cout << std::left << std::setfill('*') << std::setw(10) << 42 << std::endl;
   std::cout << std::hex << value << std::endl;
   
   // 自定义类型流操作符重载（简化示例）
   class Point {
       int x, y;
       friend std::ostream& operator<<(std::ostream& os, const Point& p);
   };
   std::ostream& operator<<(std::ostream& os, const Point& p) {
       os << "(" << p.x << ", " << p.y << ")";
       return os;
   }

格式化选项
----------

数值格式
~~~~~~~~

- **精度**: ``std::setprecision(n)`` - 设置浮点数精度
- **宽度**: ``std::setw(n)`` - 设置字段宽度
- **填充**: ``std::setfill(c)`` - 设置填充字符
- **对齐**: ``std::left``、``std::right`` - 设置对齐方式

进制格式
~~~~~~~~

- **十进制**: ``std::dec`` - 默认进制
- **十六进制**: ``std::hex`` - 输出十六进制
- **八进制**: ``std::oct`` - 输出八进制

布尔值格式
~~~~~~~~~~

- **数字格式**: 默认输出 ``1`` 或 ``0``
- **字面格式**: ``std::boolalpha`` - 输出 ``true`` 或 ``false``

注意事项
--------

1. **文件检查**: 打开文件后必须检查文件流状态（``if (!file)``），确保文件成功打开
2. **资源管理**: 文件流在析构时会自动关闭，但显式调用 ``close()`` 更清晰
3. **异常处理**: 可以使用 ``file.exceptions()`` 设置异常掩码，或手动检查文件状态
4. **字符串流**: ``ostringstream`` 和 ``istringstream`` 在内存中操作，不涉及文件系统
5. **格式化状态**: 某些格式化选项（如 ``std::hex``）会持续生效，需要手动重置（``std::dec``）
6. **操作符重载**: 自定义类型的流操作符重载应声明为友元函数，以便访问私有成员
7. **缓冲区**: ``std::endl`` 会刷新缓冲区，在性能敏感场景中可考虑使用 ``'\n'`` 代替
8. **文件系统**: 使用文件流前需要确保文件系统已正确初始化（如调用 ``test_lsfs_init()``）

