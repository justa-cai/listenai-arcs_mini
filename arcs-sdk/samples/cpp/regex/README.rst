C++ 正则表达式示例
===================

功能说明
--------

演示 C++ 标准库中正则表达式的使用，包括模式匹配、字符串搜索和分组提取等功能。本示例展示了如何使用 ``std::regex`` 进行文本处理和模式识别。

硬件连接
--------

无需外部连接，正则表达式为软件功能，不依赖硬件资源。

示例内容
--------

1. 基本模式匹配（``regex_search``）
2. 锚点匹配（``^`` 开头、``$`` 结尾）
3. 字符类和量词匹配
4. 分组提取和捕获
5. 多次匹配和迭代搜索

编译
--------

.. include:: /sample_build.rst

烧录固件
--------

.. include:: /sample_flash.rst

预期输出
--------

.. code-block:: text

   === C++ Regex Sample ===
   
   1. Basic pattern matching:
   Match: 'quick' in 'The quick brown fox'
   Match: '^The' in 'The quick brown fox'
   Match: 'fox$' in 'The quick brown fox'
   Match: '\s[a-z]{5}\s' in 'The quick brown fox'
   
   2. Extracting matches with groups:
   Pattern: '([a-z]+)\s+([a-z]+)'
   String: 'hello world'
   Match 0: hello world
   Match 1: hello
   Match 2: world
   Pattern: '(\d{4})-(\d{2})-(\d{2})'
   String: 'Date: 2023-05-26'
   Match 0: 2023-05-26
   Match 1: 2023
   Match 2: 05
   Match 3: 26
   Pattern: '([a-zA-Z]+) (\d+), (\d+)'
   String: 'May 26, 2023'
   Match 0: May 26, 2023
   Match 1: May
   Match 2: 26
   Match 3: 2023
   Regex Sample Completed

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``std::regex``
     - 正则表达式对象，用于存储编译后的模式
   * - ``std::regex_search()``
     - 在字符串中搜索匹配的模式
   * - ``std::smatch``
     - 匹配结果容器，存储匹配的子串
   * - ``std::regex_match()``
     - 检查整个字符串是否完全匹配模式

关键代码
--------

.. code-block:: cpp

   // 基本模式匹配
   void match_pattern(const string& pattern, const string& str) {
       regex re(pattern);
       if (regex_search(str, re)) {
           cout << "Match: '" << pattern << "' in '" << str << "'" << endl;
       } else {
           cout << "No match: '" << pattern << "' in '" << str << "'" << endl;
       }
   }
   
   // 提取匹配和分组
   void extract_matches(const string& pattern, const string& str) {
       regex re(pattern);
       smatch matches;
       
       cout << "Pattern: '" << pattern << "'" << endl;
       cout << "String: '" << str << "'" << endl;
       
       string::const_iterator searchStart(str.cbegin());
       int i = 0;
       while (regex_search(searchStart, str.cend(), matches, re)) {
           for (size_t j = 0; j < matches.size(); ++j) {
               cout << "Match " << i << ": " << matches[j].str() << endl;
               i++;
           }
           searchStart = matches[0].second;
       }
   }
   
   // 使用示例
   match_pattern("quick", "The quick brown fox");
   extract_matches("([a-z]+)\\s+([a-z]+)", "hello world");

正则表达式语法
--------------

常用元字符
~~~~~~~~~~

.. list-table::
   :header-rows: 1

   * - 元字符
     - 说明
   * - ``.``
     - 匹配任意字符（除换行符）
   * - ``^``
     - 匹配字符串开头
   * - ``$``
     - 匹配字符串结尾
   * - ``*``
     - 匹配前一个字符 0 次或多次
   * - ``+``
     - 匹配前一个字符 1 次或多次
   * - ``?``
     - 匹配前一个字符 0 次或 1 次
   * - ``{n}``
     - 匹配前一个字符恰好 n 次
   * - ``{n,m}``
     - 匹配前一个字符 n 到 m 次
   * - ``[abc]``
     - 匹配字符类中的任意字符
   * - ``\d``
     - 匹配数字字符
   * - ``\s``
     - 匹配空白字符
   * - ``()``
     - 捕获分组

转义字符
~~~~~~~~

在 C++ 字符串中，反斜杠需要转义，因此正则表达式中的 ``\d`` 应写作 ``"\\d"``。

注意事项
--------

1. **转义字符**: 在 C++ 字符串字面量中，反斜杠需要转义，正则表达式 ``\d`` 应写作 ``"\\d"``
2. **性能考虑**: ``std::regex`` 对象可以重复使用，避免在循环中重复构造
3. **异常处理**: 无效的正则表达式模式会抛出 ``std::regex_error`` 异常
4. **匹配结果**: ``smatch[0]`` 包含完整匹配，``smatch[1]``、``smatch[2]`` 等包含捕获的分组
5. **迭代器**: 使用迭代器进行多次匹配时，需要更新搜索起始位置（``matches[0].second``）
6. **大小写敏感**: 默认情况下正则表达式区分大小写，如需忽略大小写可使用 ``std::regex_constants::icase`` 标志

