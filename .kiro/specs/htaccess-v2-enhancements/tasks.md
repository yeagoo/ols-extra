# 实施计划：OLS .htaccess 模块 v2 增强

## 概述

将 v2 增强设计分解为增量式编码任务。实施顺序遵循优先级：P0 Bug 修复 → 数据模型扩展 → Mock 层扩展 → P1 面板核心指令 → P2 高级指令 → P3 认证/处理器 → P4 低优先级 → 暴力破解增强 → mod_htaccess 集成 → 生成器扩展 → 属性测试 → 兼容性测试 → CI 更新 → 构建验证。每个任务在前一个任务基础上构建，测试任务紧跟实现任务。所有代码使用 C11（模块核心）和 C++17（测试代码）。

## 任务

- [x] 1. P0 Bug 修复
  - [x] 1.1 修复 php_value/php_flag 黑名单错误
    - 修改 `src/htaccess_exec_php.c` 中的 `php_ini_system_settings[]` 数组
    - 移除 `memory_limit`、`max_input_time`、`post_max_size`、`upload_max_filesize`、`safe_mode` 五个条目
    - 保留真正的 PHP_INI_SYSTEM 级别设置（allow_url_fopen、allow_url_include、disable_classes、disable_functions、engine、expose_php、open_basedir、realpath_cache_size、realpath_cache_ttl、upload_tmp_dir、max_file_uploads、sys_temp_dir）
    - _需求: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7_

  - [x] 1.2 修复 ErrorDocument 文本消息引号保留
    - 在 `src/htaccess_parser.c` 中新增 `rest_of_line_raw()` 函数，不剥离引号
    - 修改 ErrorDocument 解析分支，使用 `rest_of_line_raw()` 获取值部分
    - 修改 `src/htaccess_printer.c` 中 `DIR_ERROR_DOCUMENT` 分支，当 `value[0] == '"'` 时直接输出不额外加引号
    - _需求: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [x] 1.3 编写 Bug 修复单元测试
    - 创建 `tests/unit/test_php_blacklist_fix.cpp`
      - 验证 memory_limit、max_input_time、post_max_size、upload_max_filesize、safe_mode 被接受
      - 验证 disable_functions、expose_php、allow_url_fopen 等仍被拒绝
    - 创建 `tests/unit/test_errordoc_quote_fix.cpp`
      - 验证 `ErrorDocument 404 "Custom message"` 解析后 value 以 `"` 开头
      - 验证执行器检测到引号前缀并返回去引号后的文本作为响应体
      - 验证外部 URL 和本地文件路径模式不受影响
    - _需求: 1.1-1.7, 2.1-2.5_

  - [x] 1.4 编写 Bug 修复属性测试
    - **Property 26: PHP 黑名单正确性**
      - 创建 `tests/property/prop_php_blacklist.cpp`
      - 对任意 PHP 设置名称，验证 exec_php_value() 接受当且仅当名称不在 php_ini_system_settings[] 中
      - **验证: 需求 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7**
    - **Property 27: ErrorDocument 文本消息管道**
      - 创建 `tests/property/prop_errordoc_text.cpp`
      - 对任意以双引号开头的 ErrorDocument 值，验证 parse → exec 管道将去引号后的文本设置为响应体
      - **验证: 需求 2.1, 2.2**

- [x] 2. 数据模型扩展
  - [x] 2.1 扩展 directive_type_t 枚举和 union 字段
    - 修改 `include/htaccess_directive.h`
    - 在 `DIR_BRUTE_FORCE_THROTTLE_DURATION` 之后追加 31 个新枚举值（DIR_IFMODULE 到 DIR_BRUTE_FORCE_PROTECT_PATH，编号 28-58）
    - 在 `data` union 中新增 ifmodule、files、options、header_ext、require_container、limit 等字段
    - 扩展 `htaccess_directives_free()` 以释放新容器类型的 children 链表
    - 修改 `src/htaccess_directive.c` 中的释放逻辑
    - _需求: 17.3_

  - [x] 2.2 编写数据模型单元测试
    - 在 `tests/unit/test_directive.cpp` 中新增测试用例
    - 验证新枚举值的编号正确性（DIR_IFMODULE=28, ..., DIR_BRUTE_FORCE_PROTECT_PATH=58）
    - 验证容器类型 Directive 的 children 链表创建和释放不泄漏内存
    - _需求: 17.3_

- [x] 3. Mock LSIAPI 扩展
  - [x] 3.1 扩展 Mock 层接口
    - 修改 `tests/mock_lsiapi.h` 和 `tests/mock_lsiapi.cpp`
    - 新增 `lsi_session_set_dir_option()` / `lsi_session_get_dir_option()` — Options 相关
    - 新增 `lsi_session_set_uri_internal()` — DirectoryIndex 内部重定向
    - 新增 `lsi_session_file_exists()` — 文件存在性检查
    - 新增 `lsi_session_get_method()` — 请求方法获取
    - 新增 `lsi_session_get_auth_header()` — Authorization 头获取
    - 新增 `lsi_session_set_www_authenticate()` — WWW-Authenticate 头设置
    - 在 `tests/unit/test_mock_lsiapi.cpp` 中新增对应的 Mock 接口验证测试
    - _需求: 4.1, 5.2, 8.3, 9.2, 10.5, 12.2_

- [x] 4. 检查点 - Bug 修复与基础设施验证
  - 确保所有 v1 测试继续通过，v2 Bug 修复测试通过，Mock 层扩展正常工作，如有问题请向用户确认。

- [x] 5. P1 面板核心指令 — IfModule
  - [x] 5.1 实现 IfModule 解析与打印
    - 在 `src/htaccess_parser.c` 中新增 `is_ifmodule_open()` / `is_ifmodule_close()` 函数
    - 实现 IfModule 块解析逻辑：正向条件生成容器节点含 children，否定条件跳过内部指令
    - 支持嵌套 IfModule 块（深度计数器）
    - 未闭合块记录 WARN 日志并丢弃
    - 在 `src/htaccess_printer.c` 中新增 `DIR_IFMODULE` 格式化分支，递归打印 children
    - _需求: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

  - [x] 5.2 编写 IfModule 单元测试
    - 在 `tests/unit/test_parser_v2.cpp` 中新增 IfModule 解析测试
    - 测试正向条件、否定条件、嵌套块、未闭合块
    - 在 `tests/unit/test_printer_v2.cpp` 中新增 IfModule 打印测试
    - _需求: 3.1-3.7_

  - [x] 5.3 编写 IfModule 属性测试
    - **Property 28: IfModule 条件包含**
    - 创建 `tests/property/prop_ifmodule.cpp`
    - 对任意 IfModule 块，验证正向条件执行 children、否定条件跳过 children
    - **验证: 需求 3.3, 3.4**

- [x] 6. P1 面板核心指令 — Options
  - [x] 6.1 实现 Options 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 Options 指令解析（`rest_of_line()` 获取标志字符串，解析为 options 位图）
    - 在 `src/htaccess_printer.c` 中新增 `DIR_OPTIONS` 格式化分支
    - 创建 `include/htaccess_exec_options.h` 和 `src/htaccess_exec_options.c`
    - 实现 `exec_options()` 函数，解析 +/-Indexes、+/-FollowSymLinks 等标志，通过 LSIAPI 设置目录选项
    - 未知标志记录 WARN 日志并忽略
    - _需求: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 6.2 编写 Options 单元测试
    - 创建 `tests/unit/test_exec_options.cpp`
    - 测试 `-Indexes`、`+Indexes`、`+FollowSymLinks`、多标志组合
    - 测试未知标志的忽略行为
    - _需求: 4.1-4.5_

  - [x] 6.3 编写 Options 属性测试
    - **Property 29: Options 标志执行**
    - 创建 `tests/property/prop_options.cpp`
    - 对任意 Options 标志组合，验证执行后 LSIAPI 查询结果与指令一致
    - **验证: 需求 4.1, 4.2, 4.3, 4.4**

- [x] 7. P1 面板核心指令 — Files
  - [x] 7.1 实现 Files 块解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 `is_files_open()` / `is_files_close()` 函数
    - 实现 Files 块解析逻辑，生成 `DIR_FILES` 容器节点，name 存储文件名，children 存储内部指令
    - 在 `src/htaccess_printer.c` 中新增 `DIR_FILES` 格式化分支
    - 执行逻辑复用 FilesMatch 模式：basename 精确匹配时执行 children，不匹配时跳过
    - _需求: 5.1, 5.2, 5.3, 5.4, 5.5_

  - [x] 7.2 编写 Files 单元测试
    - 在 `tests/unit/test_parser_v2.cpp` 中新增 Files 块解析测试
    - 在 `tests/unit/test_printer_v2.cpp` 中新增 Files 块打印测试
    - 测试精确匹配、不匹配、嵌套指令执行顺序
    - _需求: 5.1-5.5_

  - [x] 7.3 编写 Files 属性测试
    - **Property 30: Files 精确匹配条件应用**
    - 创建 `tests/property/prop_files.cpp`
    - 对任意文件名和请求文件名，验证 Files 块内指令当且仅当 basename 精确匹配时被应用
    - **验证: 需求 5.2, 5.3**

- [x] 8. 检查点 - P1 面板核心指令验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 9. P2 高级指令 — Header always
  - [x] 9.1 实现 Header always 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中修改 Header 解析逻辑，检测 `always` 关键字后生成 `DIR_HEADER_ALWAYS_*` 类型
    - 在 `src/htaccess_printer.c` 中新增 5 个 `DIR_HEADER_ALWAYS_*` 格式化分支，输出 `Header always` 前缀
    - 执行器复用现有 `exec_header()` 函数，always 语义差异在 mod_htaccess.c Hook 调度层体现
    - _需求: 6.1, 6.2, 6.3, 6.4, 6.5_

  - [x] 9.2 编写 Header always 单元测试
    - 在 `tests/unit/test_exec_header.cpp` 中新增 Header always 测试用例
    - 测试 always set、always unset、always append、always merge、always add
    - 验证 always 指令在错误响应（4xx、5xx）上也生效
    - _需求: 6.1-6.5_

  - [x] 9.3 编写 Header always 属性测试
    - **Property 31: Header always 全响应覆盖**
    - 创建 `tests/property/prop_header_always.cpp`
    - 对任意 Header always set 指令和任意 HTTP 状态码，验证响应中包含指定头
    - **验证: 需求 6.1, 6.2**

- [x] 10. P2 高级指令 — ExpiresDefault
  - [x] 10.1 实现 ExpiresDefault 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 ExpiresDefault 解析（复用 `parse_expires_duration()`）
    - 在 `src/htaccess_printer.c` 中新增 `DIR_EXPIRES_DEFAULT` 格式化分支
    - 修改 `src/htaccess_exec_expires.c`，在 ExpiresByType 无匹配时使用 ExpiresDefault 作为 fallback
    - _需求: 7.1, 7.2, 7.3, 7.4_

  - [x] 10.2 编写 ExpiresDefault 单元测试
    - 在现有 Expires 测试中新增 ExpiresDefault 测试用例
    - 测试 ExpiresDefault 作为 fallback 的行为
    - 测试 ExpiresByType 优先于 ExpiresDefault
    - _需求: 7.1-7.4_

  - [x] 10.3 编写 ExpiresDefault 属性测试
    - **Property 32: ExpiresDefault 回退**
    - 创建 `tests/property/prop_expires_default.cpp`
    - 对任意 MIME 类型，验证无 ExpiresByType 匹配时使用 ExpiresDefault，有匹配时使用 ExpiresByType
    - **验证: 需求 7.2, 7.3**

- [x] 11. P2 高级指令 — Require 访问控制
  - [x] 11.1 实现 Require 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 Require 系列指令解析：Require all granted/denied、Require ip、Require not ip、Require valid-user
    - 新增 RequireAny/RequireAll 容器块解析（`is_require_any_open/close`、`is_require_all_open/close`）
    - 在 `src/htaccess_printer.c` 中新增所有 Require 相关类型的格式化分支
    - 创建 `include/htaccess_exec_require.h` 和 `src/htaccess_exec_require.c`
    - 实现 `exec_require()` 函数，支持 RequireAny（OR 逻辑）和 RequireAll（AND 逻辑）
    - 当 Require 与 Order/Allow/Deny 共存时，Require 优先，记录 WARN 日志
    - _需求: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

  - [x] 11.2 编写 Require 单元测试
    - 创建 `tests/unit/test_exec_require.cpp`
    - 测试 Require all granted/denied
    - 测试 Require ip 和 Require not ip 的 CIDR 匹配
    - 测试 RequireAny 和 RequireAll 容器块的逻辑组合
    - 测试 Require 与 Order/Allow/Deny 共存时的优先级
    - _需求: 8.1-8.7_

  - [x] 11.3 编写 Require 属性测试
    - **Property 33: Require ip/all 访问控制评估**
      - 创建 `tests/property/prop_require.cpp`
      - 对任意客户端 IP 和 Require 配置，验证评估结果符合 Apache 2.4 语义
      - **验证: 需求 8.1, 8.2, 8.3, 8.4**
    - **Property 34: RequireAny OR / RequireAll AND 逻辑**
      - 验证 RequireAny 为 OR 逻辑、RequireAll 为 AND 逻辑
      - **验证: 需求 8.5, 8.6**
    - **Property 35: Require 优先于 Order/Allow/Deny**
      - 验证共存时仅 Require 指令生效
      - **验证: 需求 8.7**

- [x] 12. P2 高级指令 — Limit/LimitExcept
  - [x] 12.1 实现 Limit/LimitExcept 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 `is_limit_open/close`、`is_limit_except_open/close` 函数
    - 实现 Limit/LimitExcept 块解析，methods 字段存储空格分隔的 HTTP 方法列表
    - 在 `src/htaccess_printer.c` 中新增 `DIR_LIMIT` 和 `DIR_LIMIT_EXCEPT` 格式化分支
    - 创建 `include/htaccess_exec_limit.h` 和 `src/htaccess_exec_limit.c`
    - 实现 `exec_limit()` 函数：Limit 块方法在列表中执行 children，LimitExcept 块方法不在列表中执行 children
    - _需求: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7_

  - [x] 12.2 编写 Limit/LimitExcept 单元测试
    - 创建 `tests/unit/test_exec_limit.cpp`
    - 测试 Limit 块：方法在列表中执行、不在列表中跳过
    - 测试 LimitExcept 块：方法不在列表中执行、在列表中跳过
    - 测试多方法列表
    - _需求: 9.1-9.7_

  - [x] 12.3 编写 Limit/LimitExcept 属性测试
    - **Property 36: Limit/LimitExcept 方法对偶性**
    - 创建 `tests/property/prop_limit.cpp`
    - 对任意 HTTP 方法列表和请求方法，验证 Limit 和 LimitExcept 行为互补
    - **验证: 需求 9.2, 9.3, 9.5, 9.6**

- [x] 13. 检查点 - P2 高级指令验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 14. P3 认证/处理器 — AuthType Basic
  - [x] 14.1 实现 AuthType Basic 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 AuthType、AuthName、AuthUserFile、Require valid-user 解析
    - 在 `src/htaccess_printer.c` 中新增对应格式化分支
    - 创建 `include/htaccess_exec_auth.h` 和 `src/htaccess_exec_auth.c`
    - 实现 `exec_auth_basic()` 函数：从指令链表收集认证配置，验证 Authorization 头
    - 实现 `htpasswd_check()` 函数：支持 crypt、MD5（$apr1$）、bcrypt（$2y$）哈希格式
    - 无凭证或错误凭证返回 401（含 WWW-Authenticate 头），AuthUserFile 不存在返回 500
    - _需求: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9_

  - [x] 14.2 编写 AuthType Basic 单元测试
    - 创建 `tests/unit/test_exec_auth.cpp`
    - 测试完整认证流程：无凭证 → 401、错误凭证 → 401、正确凭证 → 通过
    - 测试 crypt、MD5、bcrypt 三种哈希格式
    - 测试 AuthUserFile 不存在时返回 500
    - _需求: 10.1-10.8_

  - [x] 14.3 编写 AuthType Basic 属性测试
    - **Property 37: AuthType Basic 认证**
      - 创建 `tests/property/prop_auth_basic.cpp`
      - 对任意认证配置，验证无凭证/错误凭证返回 401、正确凭证通过
      - **验证: 需求 10.4, 10.5, 10.6**
    - **Property 38: htpasswd 哈希验证**
      - 对任意密码和支持的哈希格式，验证 htpasswd_check() 返回匹配当且仅当 hash 是 password 的有效哈希
      - **验证: 需求 10.7**

- [x] 15. P3 认证/处理器 — AddHandler/SetHandler/AddType
  - [x] 15.1 实现 Handler/Type 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 AddHandler、SetHandler、AddType 解析
    - 在 `src/htaccess_printer.c` 中新增对应格式化分支
    - 创建 `include/htaccess_exec_handler.h` 和 `src/htaccess_exec_handler.c`
    - 实现 `exec_add_handler()` / `exec_set_handler()`（DEBUG 级别日志记录）
    - 实现 `exec_add_type()` 设置 Content-Type 响应头
    - _需求: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

  - [x] 15.2 编写 Handler/Type 单元测试
    - 创建 `tests/unit/test_exec_handler.cpp`
    - 测试 AddHandler 解析（handler 名 + 扩展名列表）
    - 测试 SetHandler 解析
    - 测试 AddType 执行后 Content-Type 设置
    - _需求: 11.1-11.6_

  - [x] 15.3 编写 AddType 属性测试
    - **Property 39: AddType Content-Type 设置**
    - 创建 `tests/property/prop_add_type.cpp`
    - 对任意 MIME 类型和文件扩展名，验证匹配时 Content-Type 被正确设置
    - **验证: 需求 11.5**

- [x] 16. P3 认证/处理器 — DirectoryIndex
  - [x] 16.1 实现 DirectoryIndex 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 DirectoryIndex 解析（`rest_of_line()` 获取文件名列表）
    - 在 `src/htaccess_printer.c` 中新增 `DIR_DIRECTORY_INDEX` 格式化分支
    - 创建 `include/htaccess_exec_dirindex.h` 和 `src/htaccess_exec_dirindex.c`
    - 实现 `exec_directory_index()` 函数：按顺序检查文件存在性，找到第一个存在的文件后设置内部重定向
    - 所有文件不存在时回退到 OLS 默认行为
    - _需求: 12.1, 12.2, 12.3, 12.4_

  - [x] 16.2 编写 DirectoryIndex 单元测试
    - 创建 `tests/unit/test_exec_dirindex.cpp`
    - 测试单文件、多文件列表、首个存在文件选择
    - 测试所有文件不存在时的回退行为
    - _需求: 12.1-12.4_

  - [x] 16.3 编写 DirectoryIndex 属性测试
    - **Property 40: DirectoryIndex 首个存在文件**
    - 创建 `tests/property/prop_dirindex.cpp`
    - 对任意文件名列表和目录内容，验证执行器选择列表中第一个存在的文件
    - **验证: 需求 12.2**

- [x] 17. 检查点 - P3 认证/处理器验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 18. P4 低优先级指令
  - [x] 18.1 实现 ForceType 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 ForceType 解析
    - 在 `src/htaccess_printer.c` 中新增 `DIR_FORCE_TYPE` 格式化分支
    - 创建 `include/htaccess_exec_forcetype.h` 和 `src/htaccess_exec_forcetype.c`
    - 实现 `exec_force_type()` 函数，设置 Content-Type 响应头
    - _需求: 13.1, 13.2, 13.3_

  - [x] 18.2 实现 AddEncoding/AddCharset 解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 AddEncoding、AddCharset 解析
    - 在 `src/htaccess_printer.c` 中新增 `DIR_ADD_ENCODING` 和 `DIR_ADD_CHARSET` 格式化分支
    - 创建 `include/htaccess_exec_encoding.h` 和 `src/htaccess_exec_encoding.c`
    - 实现 `exec_add_encoding()` 设置 Content-Encoding 响应头
    - 实现 `exec_add_charset()` 追加 charset 参数到 Content-Type
    - _需求: 14.1, 14.2, 14.3, 15.1, 15.2, 15.3_

  - [x] 18.3 编写 P4 指令单元测试
    - 创建 `tests/unit/test_exec_forcetype.cpp`
      - 测试 ForceType 设置 Content-Type
    - 创建 `tests/unit/test_exec_encoding.cpp`
      - 测试 AddEncoding 设置 Content-Encoding
      - 测试 AddCharset 追加 charset 参数
    - _需求: 13.1-13.3, 14.1-14.3, 15.1-15.3_

  - [x] 18.4 编写 P4 指令属性测试
    - **Property 41: ForceType Content-Type 覆盖**
      - 创建 `tests/property/prop_forcetype.cpp`
      - 对任意 MIME 类型，验证 ForceType 执行后 Content-Type 被设置为该类型
      - **验证: 需求 13.2**
    - **Property 42: AddEncoding/AddCharset 头设置**
      - 创建 `tests/property/prop_encoding_charset.cpp`
      - 对任意编码类型和文件扩展名，验证 Content-Encoding 被正确设置
      - 对任意字符集和文件扩展名，验证 Content-Type 包含 charset 参数
      - **验证: 需求 14.2, 15.2**

- [x] 19. 暴力破解防护增强
  - [x] 19.1 实现 BruteForce 增强指令解析、打印与执行
    - 在 `src/htaccess_parser.c` 中新增 BruteForceXForwardedFor、BruteForceWhitelist、BruteForceProtectPath 解析
    - 在 `src/htaccess_printer.c` 中新增对应格式化分支
    - 修改 `src/htaccess_exec_brute_force.c`：
      - 新增 XFF 处理逻辑：启用时从 X-Forwarded-For 头获取最左侧 IP
      - 新增白名单检查：匹配 CIDR 范围的 IP 直接放行
      - 新增保护路径列表：仅对匹配路径生效
    - _需求: 16.1, 16.2, 16.3, 16.4, 16.5, 16.6, 16.7, 16.8_

  - [x] 19.2 编写 BruteForce 增强单元测试
    - 创建 `tests/unit/test_brute_force_v2.cpp`
    - 测试 XFF 启用/禁用时的 IP 获取
    - 测试白名单 CIDR 匹配豁免
    - 测试自定义保护路径
    - 测试多个 BruteForceProtectPath 指令
    - _需求: 16.1-16.6_

  - [x] 19.3 编写 BruteForce 增强属性测试
    - **Property 43: BruteForce XFF IP 解析**
      - 创建 `tests/property/prop_brute_force_v2.cpp`
      - 对任意 XFF 头和配置，验证启用时使用 XFF IP、禁用时使用直连 IP
      - **验证: 需求 16.1, 16.2**
    - **Property 44: BruteForce 白名单豁免**
      - 对任意白名单 CIDR 范围内的 IP，验证不触发封锁
      - **验证: 需求 16.3, 16.4**
    - **Property 45: BruteForce 保护路径范围**
      - 对任意保护路径配置和请求 URI，验证仅匹配路径触发追踪
      - **验证: 需求 16.5, 16.6**

- [x] 20. 检查点 - P4 与暴力破解增强验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 21. mod_htaccess.c 集成
  - [x] 21.1 扩展请求头 Hook 回调
    - 修改 `src/mod_htaccess.c` 中的 `on_recv_req_header()` 函数
    - 在现有执行步骤之间插入新步骤：
      - (a2) Require 访问控制 — 调用 `exec_require()`
      - (a3) Limit/LimitExcept 方法限制 — 遍历指令调用 `exec_limit()`
      - (a4) AuthType Basic 认证 — 调用 `exec_auth_basic()`
      - (f) Options — 遍历指令调用 `exec_options()`
      - (g) DirectoryIndex — 遍历指令调用 `exec_directory_index()`
    - 扩展暴力破解防护步骤以支持 XFF/白名单/保护路径
    - _需求: 3.3, 4.1, 5.2, 8.1, 9.2, 10.4, 12.2, 16.1_

  - [x] 21.2 扩展响应头 Hook 回调
    - 修改 `src/mod_htaccess.c` 中的 `on_send_resp_header()` 函数
    - 新增执行步骤：
      - Header always 指令 — 遍历 `DIR_HEADER_ALWAYS_*` 类型调用 `exec_header()`
      - Files 块 — 遍历 `DIR_FILES` 类型执行精确文件名匹配
      - ExpiresDefault — 在 ExpiresByType 之后作为 fallback
      - AddHandler/SetHandler/AddType — 调用对应执行器
      - ForceType — 调用 `exec_force_type()`
      - AddEncoding/AddCharset — 调用对应执行器
    - _需求: 6.2, 7.2, 5.2, 11.4, 11.5, 13.2, 14.2, 15.2_

  - [x] 21.3 更新 CMakeLists.txt
    - 修改根 `CMakeLists.txt`，添加所有新增 .c 源文件到共享库目标
    - 修改 `tests/CMakeLists.txt`，注册所有新增测试文件
    - 确保 v2 单元测试、属性测试、兼容性测试分别注册为独立 CTest 目标
    - _需求: 17.1, 17.2_

  - [x] 21.4 编写集成测试
    - 在 `tests/unit/test_integration.cpp` 中新增 v2 集成测试用例
    - 使用 Mock 层模拟完整请求处理流程，验证 v2 新增执行步骤的调度顺序
    - 测试 Require → Limit → AuthType → Options → DirectoryIndex 的请求阶段链路
    - 测试 Header always → Files → ExpiresDefault → AddType → ForceType → AddEncoding/AddCharset 的响应阶段链路
    - _需求: 17.1, 17.2_

- [x] 22. RapidCheck 生成器扩展
  - [x] 22.1 扩展现有生成器并创建新生成器
    - 修改 `tests/generators/gen_directive.h` — 扩展覆盖所有 v2 新增 directive_type_t 枚举值，包括容器指令的 children 生成
    - 修改 `tests/generators/gen_htaccess.h` — 扩展生成包含 v2 新指令的 .htaccess 文件文本
    - 创建 `tests/generators/gen_options.h` — Options 标志组合生成器
    - 创建 `tests/generators/gen_http_method.h` — HTTP 方法名生成器（GET/POST/PUT/DELETE/PATCH 等）
    - 创建 `tests/generators/gen_htpasswd.h` — htpasswd 条目生成器（crypt/MD5/bcrypt 哈希）
    - 创建 `tests/generators/gen_require.h` — Require 配置组合生成器（含 RequireAny/RequireAll 容器）
    - 创建 `tests/generators/gen_mime.h` — MIME 类型字符串生成器
    - 创建 `tests/generators/gen_extension.h` — 文件扩展名生成器
    - _需求: 18.1_

  - [x] 22.2 编写生成器冒烟测试
    - 在 `tests/property/prop_generators_smoke.cpp` 中新增 v2 生成器冒烟测试
    - 验证每个新生成器能成功生成至少 100 个有效样本
    - _需求: 18.1_

- [x] 23. v2 Round-Trip 属性测试
  - [x] 23.1 编写 v2 指令 Round-Trip 属性测试
    - **Property 25: v2 指令解析/打印 Round-Trip**
    - 创建 `tests/property/prop_v2_roundtrip.cpp`
    - 对任意包含 v2 新增指令类型的有效 .htaccess 内容，验证 parse → print → parse 产生等价 Directive 列表
    - 覆盖所有 v2 新增指令类型（IfModule、Options、Files、Header always、ExpiresDefault、Require、Limit、AuthType、AddHandler/SetHandler/AddType、DirectoryIndex、ForceType、AddEncoding、AddCharset、BruteForce 增强）
    - 同时覆盖 ErrorDocument 引号保留修复后的 round-trip
    - **验证: 需求 2.5, 3.8, 4.6, 5.6, 6.6, 7.5, 8.9, 9.8, 10.10, 11.7, 12.5, 13.4, 14.4, 15.4, 16.9**

- [x] 24. 检查点 - 生成器与属性测试验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 25. 兼容性测试
  - [x] 25.1 创建 v2 兼容性测试样本文件
    - 创建 `tests/compat/htaccess_samples/wordpress_ifmodule.htaccess` — WordPress IfModule 块样本
    - 创建 `tests/compat/htaccess_samples/cpanel_auth_basic.htaccess` — cPanel AuthType Basic 样本
    - 创建 `tests/compat/htaccess_samples/apache24_require.htaccess` — Apache 2.4 Require 语法样本
    - 创建 `tests/compat/htaccess_samples/security_headers.htaccess` — Header always 安全头样本
    - 创建 `tests/compat/htaccess_samples/laravel_options.htaccess` — Laravel Options 样本
    - 创建 `tests/compat/htaccess_samples/cyberpanel_full.htaccess` — CyberPanel 完整样本
    - _需求: 18.3_

  - [x] 25.2 编写 v2 兼容性测试
    - 创建 `tests/compat/test_compat_v2.cpp`
    - 对每个 v2 新指令类型验证三个方面：(a) 解析完整性、(b) round-trip、(c) 执行正确性
    - 使用样本 .htaccess 文件验证真实 CMS 配置的兼容性
    - 确保可通过 `ctest --test-dir build -R "compat_tests"` 运行
    - _需求: 18.1, 18.2, 18.5_

  - [x] 25.3 编写 CyberPanel 功能对等测试
    - 创建 `tests/compat/test_compat_cyberpanel.cpp`
    - 解析并执行 cyberpanel_full.htaccess 样本
    - 验证所有指令被识别且执行结果正确
    - _需求: 18.4_

- [x] 26. CI 流水线更新
  - [x] 26.1 更新 GitHub CI 配置
    - 修改 `.github/workflows/ci.yml`
    - 新增 v2 单元测试步骤：`ctest --test-dir build -R "v2_unit"`
    - 新增 v2 属性测试步骤：`ctest --test-dir build -R "v2_prop"`
    - 新增 v2 兼容性测试步骤：`ctest --test-dir build -R "compat_v2"`
    - 新增 Apache httpd 对比测试 job：Docker 启动 httpd:2.4，使用 curl 断言验证 v2 新指令行为
    - 确保 v1 所有测试继续运行
    - 验证 .so 在 Debug 和 Release 模式下均能构建
    - 测试结果分类报告：v1 unit / v1 property / v1 compat / v2 unit / v2 property / v2 compat / Apache comparison
    - 任何测试失败时阻止合并
    - _需求: 19.1, 19.2, 19.3, 19.4, 19.5, 19.6_

- [x] 27. 最终构建验证
  - [x] 27.1 全量测试与构建验证
    - 运行所有 v1 测试（单元、属性、兼容性），确认全部通过（向后兼容性验证）
    - 运行所有 v2 测试（单元、属性、兼容性），确认全部通过
    - 验证 `ols_htaccess.so` 在 Debug 和 Release 模式下均能干净构建
    - 确认无编译警告（-Wall -Wextra）
    - 确保所有测试通过，如有问题请向用户确认。
    - _需求: 17.1, 17.2, 17.3, 17.4_

## 备注

- 标记 `*` 的任务为可选任务，可跳过以加速 MVP 开发
- 每个任务引用了具体的需求编号以确保可追溯性
- 检查点任务确保增量验证
- 属性测试验证设计文档中定义的 21 条新增正确性属性（Property 25-45）
- 单元测试验证具体示例和边界情况
- 所有模块核心代码使用 C11，测试代码使用 C++17
- v1 已有的 401 个测试必须继续通过，不做任何修改
