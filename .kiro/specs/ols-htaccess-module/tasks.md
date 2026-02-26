# 实施计划：OLS .htaccess 模块

## 概述

将 OLS .htaccess 模块的设计分解为增量式编码任务。每个任务在前一个任务的基础上构建，从项目脚手架开始，逐步实现核心组件（Parser、Cache、DirWalker）、各指令执行器、共享内存管理，最后连接模块入口点。测试任务紧跟对应的实现任务，确保尽早发现错误。

## 任务

- [x] 1. 项目脚手架与基础设施
  - [x] 1.1 创建项目目录结构和 CMakeLists.txt
    - 创建 `src/`、`include/`、`tests/unit/`、`tests/property/`、`tests/generators/` 目录
    - 编写根 CMakeLists.txt，配置 C/C++ 编译选项（C11/C++17）、输出 .so 共享库
    - 配置 Google Test 和 RapidCheck 作为测试依赖（FetchContent 或 find_package）
    - 编写 `tests/CMakeLists.txt` 配置测试目标
    - _需求: 1.1_

  - [x] 1.2 创建 LSIAPI Mock 层
    - 创建 `tests/mock_lsiapi.h` 和 `tests/mock_lsiapi.cpp`
    - 模拟 `lsi_session_t` 结构体，支持设置/获取请求头、响应头、环境变量、响应状态码
    - 模拟 `lsi_module_t` 和 Hook 注册接口
    - 提供 PHP 配置调用记录功能
    - 所有操作记录在内存中供测试断言使用
    - _需求: 1.1, 1.3_

  - [x] 1.3 创建 LSIAPI 头文件桩（stub headers）
    - 创建 `include/ls.h`（或对应的 LSIAPI 头文件路径）
    - 定义 `lsi_module_t`、`lsi_session_t`、Hook 常量（`LSI_HKPT_RECV_REQ_HEADER`、`LSI_HKPT_SEND_RESP_HEADER`）等类型和宏
    - 确保 src/ 下的模块代码可以编译
    - _需求: 1.1_

- [x] 2. 数据模型与基础类型
  - [x] 2.1 定义 Directive 结构体和枚举类型
    - 创建 `include/htaccess_directive.h`
    - 实现 `directive_type_t` 枚举（所有 28 种指令类型）
    - 实现 `acl_order_t`、`bf_action_t` 枚举
    - 实现 `htaccess_directive_t` 结构体（含 union 和链表指针）
    - 实现 `htaccess_directives_free()` 释放链表函数
    - _需求: 2.2, 4.1-4.7, 5.1-5.4, 6.1-6.4, 7.1-7.4, 8.1-8.3, 9.1, 10.1-10.4, 11.1-11.6, 12.1-12.8_

  - [x] 2.2 实现 CIDR 解析与匹配
    - 创建 `include/htaccess_cidr.h` 和 `src/htaccess_cidr.c`
    - 实现 `cidr_v4_t` 结构体
    - 实现 `cidr_parse()` 解析 CIDR 字符串（如 "192.168.1.0/24"）
    - 实现 `cidr_match()` 检查 IP 是否在 CIDR 范围内
    - 支持 "all" 关键字匹配所有 IP
    - _需求: 6.3, 6.4, 6.5_

  - [x] 2.3 编写 CIDR 匹配属性测试
    - **Property 13: CIDR 匹配正确性**
    - 使用 CIDR 生成器生成随机 CIDR 范围和 IP 地址
    - 验证 cidr_match 返回 true 当且仅当 IP & mask == network & mask
    - **验证: 需求 6.3, 6.4**

  - [x] 2.4 编写 CIDR 单元测试
    - 创建 `tests/unit/test_cidr.cpp`
    - 测试有效 CIDR 解析（/8, /16, /24, /32）
    - 测试无效 CIDR 格式的错误处理
    - 测试边界 IP 地址匹配
    - _需求: 6.3, 6.4_

  - [x] 2.5 实现 Expires 时长解析
    - 创建 `include/htaccess_expires.h` 和 `src/htaccess_expires.c`
    - 实现 `parse_expires_duration()` 解析 "access plus N seconds/minutes/hours/days/months/years" 格式
    - 支持组合格式（如 "access plus 1 month 2 days"）
    - _需求: 10.4_

  - [x] 2.6 编写 Expires 时长解析属性测试
    - **Property 20: Expires 时长解析**
    - 使用 Expires 时长生成器生成随机有效格式字符串
    - 验证 parse_expires_duration 返回正确的秒数值
    - **验证: 需求 10.4**

  - [x] 2.7 编写 Expires 时长解析单元测试
    - 创建 `tests/unit/test_expires_parse.cpp`
    - 测试各时间单位的正确转换
    - 测试无效格式返回 -1
    - _需求: 10.4_

- [x] 3. 检查点 - 基础设施与数据模型验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 4. Parser 与 Printer
  - [x] 4.1 实现 .htaccess 解析器
    - 创建 `include/htaccess_parser.h` 和 `src/htaccess_parser.c`
    - 实现 `htaccess_parse()` 函数，逐行解析 .htaccess 内容
    - 支持所有 28 种指令类型的解析
    - 支持 `<FilesMatch>` 块的嵌套解析
    - 语法错误时记录警告（含文件路径和行号）并跳过该行
    - 保持指令的原始顺序
    - _需求: 2.1, 2.2, 2.3, 9.1_

  - [x] 4.2 实现 .htaccess 格式化输出器
    - 创建 `include/htaccess_printer.h` 和 `src/htaccess_printer.c`
    - 实现 `htaccess_print()` 函数，将 Directive 链表格式化为 .htaccess 文本
    - 支持所有指令类型的输出
    - 支持 FilesMatch 块的嵌套输出
    - _需求: 2.5_

  - [x] 4.3 编写 Parser/Printer Round-Trip 属性测试
    - **Property 1: .htaccess 解析/打印 Round-Trip**
    - 使用 .htaccess 内容生成器生成随机有效文件内容
    - 验证 parse → print → parse 产生等价的 Directive 列表
    - **验证: 需求 2.5, 2.6**

  - [x] 4.4 编写解析保序性属性测试
    - **Property 2: 解析保序性**
    - 生成包含 N 条随机有效指令的 .htaccess 内容
    - 验证解析后产生恰好 N 个 Directive 对象且顺序一致
    - **验证: 需求 2.2, 9.3**

  - [x] 4.5 编写 Parser 单元测试
    - 创建 `tests/unit/test_parser.cpp`
    - 测试各指令类型的解析正确性
    - 测试空文件、语法错误行、未闭合 FilesMatch 块等边界情况
    - 测试多行指令文件的解析
    - _需求: 2.1, 2.2, 2.3, 2.4_

- [x] 5. Cache 缓存组件
  - [x] 5.1 实现哈希表缓存
    - 创建 `include/htaccess_cache.h` 和 `src/htaccess_cache.c`
    - 实现 `htaccess_cache_init()`、`htaccess_cache_get()`、`htaccess_cache_put()`、`htaccess_cache_destroy()`
    - 哈希表以文件绝对路径为键
    - 支持 mtime 比较，mtime 不匹配时返回 NULL
    - 跟踪每条目内存占用（≤ 2KB）
    - _需求: 3.1, 3.2, 3.3, 3.4, 3.6_

  - [x] 5.2 编写缓存 Round-Trip 属性测试
    - **Property 3: 缓存 Round-Trip**
    - 验证 cache_put 后 cache_get（相同 mtime）返回等价数据
    - **验证: 需求 3.1, 3.2, 3.3**

  - [x] 5.3 编写缓存 mtime 失效属性测试
    - **Property 4: 缓存 mtime 失效**
    - 验证以不同 mtime 调用 cache_get 时返回 NULL
    - **验证: 需求 3.4**

  - [x] 5.4 编写缓存单元测试
    - 创建 `tests/unit/test_cache.cpp`
    - 测试缓存初始化、存取、更新、销毁
    - 测试缓存未命中场景
    - 测试内存分配失败时的降级行为
    - _需求: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

- [x] 6. 检查点 - 核心组件验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 7. RapidCheck 自定义生成器
  - [x] 7.1 实现测试生成器
    - 创建 `tests/generators/gen_directive.h` — Directive 生成器（覆盖所有枚举值）
    - 创建 `tests/generators/gen_htaccess.h` — .htaccess 内容生成器
    - 创建 `tests/generators/gen_cidr.h` — CIDR 范围和 IP 地址生成器
    - 创建 `tests/generators/gen_header.h` — HTTP Header 名称和值生成器
    - 创建 `tests/generators/gen_directory.h` — 目录层级生成器
    - 创建 `tests/generators/gen_expires.h` — Expires 时长字符串生成器
    - 创建 `tests/generators/gen_regex.h` — 简单正则表达式生成器
    - _需求: 2.6, 6.3, 10.4, 13.1_

- [x] 8. 指令执行器 — Header
  - [x] 8.1 实现 Header 和 RequestHeader 执行器
    - 创建 `include/htaccess_exec_header.h` 和 `src/htaccess_exec_header.c`
    - 实现 `exec_header()` 支持 set、unset、append、merge、add 五种操作
    - 实现 `exec_request_header()` 支持 set、unset 操作
    - merge 操作需检查值是否已存在（幂等性）
    - _需求: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7_

  - [x] 8.2 编写 Header set 替换语义属性测试
    - **Property 5: Header set 替换语义**
    - 验证执行 Header set 后响应中该头恰好有一个值且等于指定值
    - **验证: 需求 4.1**

  - [x] 8.3 编写 Header unset 移除语义属性测试
    - **Property 6: Header unset 移除语义**
    - 验证执行 Header unset 后响应中不再包含该头
    - **验证: 需求 4.2**

  - [x] 8.4 编写 Header append 追加语义属性测试
    - **Property 7: Header append 追加语义**
    - 验证执行 Header append 后头值包含原值和追加值，以逗号分隔
    - **验证: 需求 4.3**

  - [x] 8.5 编写 Header merge 幂等性属性测试
    - **Property 8: Header merge 幂等性**
    - 验证连续两次 Header merge 同一值，结果与一次相同
    - **验证: 需求 4.4**

  - [x] 8.6 编写 Header add 累加语义属性测试
    - **Property 9: Header add 累加语义**
    - 验证执行 Header add 后该名称的响应头数量比执行前多 1
    - **验证: 需求 4.5**

  - [x] 8.7 编写 RequestHeader set/unset 属性测试
    - **Property 10: RequestHeader set/unset 语义**
    - 验证 RequestHeader set 设置请求头，unset 移除请求头
    - **验证: 需求 4.6, 4.7**

  - [x] 8.8 编写 Header 执行器单元测试
    - 创建 `tests/unit/test_exec_header.cpp`
    - 测试各操作的具体示例
    - 测试空头值、特殊字符等边界情况
    - _需求: 4.1-4.7_

- [x] 9. 指令执行器 — PHP 配置
  - [x] 9.1 实现 PHP 配置执行器
    - 创建 `include/htaccess_exec_php.h` 和 `src/htaccess_exec_php.c`
    - 实现 `exec_php_value()`、`exec_php_flag()`、`exec_php_admin_value()`、`exec_php_admin_flag()`
    - admin 级别设置需标记为不可覆盖
    - PHP_INI_SYSTEM 级别设置被 php_value 引用时记录警告并忽略
    - _需求: 5.1, 5.2, 5.3, 5.4, 5.5_

  - [x] 9.2 编写 PHP admin 不可覆盖属性测试
    - **Property 11: PHP admin 级别设置不可覆盖**
    - 验证父目录 php_admin_value 不被子目录 php_value 覆盖
    - **验证: 需求 5.3, 5.4**

- [x] 10. 指令执行器 — 访问控制
  - [x] 10.1 实现访问控制执行器
    - 创建 `include/htaccess_exec_acl.h` 和 `src/htaccess_exec_acl.c`
    - 实现 `exec_access_control()` 函数
    - 支持 Order Allow,Deny 和 Order Deny,Allow 两种模式
    - 集成 CIDR 匹配进行 IP 范围检查
    - 支持 "all" 关键字
    - 被拒绝时返回 403 Forbidden
    - _需求: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

  - [x] 10.2 编写 ACL 评估正确性属性测试
    - **Property 12: ACL 评估正确性**
    - 生成随机 Order 类型、Allow/Deny 规则集合和客户端 IP
    - 验证评估结果符合 Apache ACL 语义
    - **验证: 需求 6.1, 6.2, 6.3, 6.4, 6.6**

  - [x] 10.3 编写访问控制单元测试
    - 创建 `tests/unit/test_exec_acl.cpp`
    - 测试 Allow,Deny 和 Deny,Allow 的具体示例
    - 测试 "all" 关键字、单 IP、CIDR 范围
    - _需求: 6.1-6.6_

- [x] 11. 指令执行器 — 重定向
  - [x] 11.1 实现重定向执行器
    - 创建 `include/htaccess_exec_redirect.h` 和 `src/htaccess_exec_redirect.c`
    - 实现 `exec_redirect()` 支持指定状态码和默认 302
    - 实现 `exec_redirect_match()` 支持正则匹配和 $N 反向引用替换
    - 匹配后立即返回重定向响应，停止后续指令处理
    - _需求: 7.1, 7.2, 7.3, 7.4, 7.5_

  - [x] 11.2 编写 Redirect 响应正确性属性测试
    - **Property 14: Redirect 响应正确性**
    - 验证响应状态码和 Location 头与指令指定值一致
    - **验证: 需求 7.1**

  - [x] 11.3 编写 RedirectMatch 捕获组替换属性测试
    - **Property 15: RedirectMatch 捕获组替换**
    - 验证 Location 头中 $N 被替换为对应捕获值
    - **验证: 需求 7.3, 7.4**

  - [x] 11.4 编写 Redirect 短路执行属性测试
    - **Property 16: Redirect 短路执行**
    - 验证匹配 Redirect 后后续指令不被执行
    - **验证: 需求 7.5**

  - [x] 11.5 编写重定向单元测试
    - 创建 `tests/unit/test_exec_redirect.cpp`
    - 测试默认 302、指定状态码、正则匹配、反向引用
    - _需求: 7.1-7.5_

- [x] 12. 指令执行器 — ErrorDocument
  - [x] 12.1 实现 ErrorDocument 执行器
    - 创建 `include/htaccess_exec_error_doc.h` 和 `src/htaccess_exec_error_doc.c`
    - 实现 `exec_error_document()` 函数
    - 支持本地文件路径、外部 URL（302 重定向）、带引号文本消息三种模式
    - 本地文件不存在时回退到 OLS 默认错误页并记录警告
    - _需求: 8.1, 8.2, 8.3, 8.4_

  - [x] 12.2 编写 ErrorDocument 外部 URL 重定向属性测试
    - **Property 17: ErrorDocument 外部 URL 重定向**
    - 验证外部 URL 产生 302 重定向
    - **验证: 需求 8.2**

  - [x] 12.3 编写 ErrorDocument 文本消息属性测试
    - **Property 18: ErrorDocument 文本消息**
    - 验证带引号文本作为响应体返回
    - **验证: 需求 8.3**

- [x] 13. 指令执行器 — FilesMatch
  - [x] 13.1 实现 FilesMatch 执行器
    - 创建 `include/htaccess_exec_files_match.h` 和 `src/htaccess_exec_files_match.c`
    - 实现 `exec_files_match()` 函数
    - 文件名匹配正则时执行嵌套指令，不匹配时跳过
    - 嵌套指令按原始顺序执行
    - _需求: 9.1, 9.2, 9.3_

  - [x] 13.2 编写 FilesMatch 条件应用属性测试
    - **Property 19: FilesMatch 条件应用**
    - 验证嵌套指令当且仅当文件名匹配正则时被应用
    - **验证: 需求 9.1, 9.2**

- [x] 14. 指令执行器 — Expires
  - [x] 14.1 实现 Expires 执行器
    - 创建 `include/htaccess_exec_expires.h` 和 `src/htaccess_exec_expires.c`
    - 实现 `exec_expires()` 函数
    - 支持 ExpiresActive On/Off 控制
    - 支持 ExpiresByType 按 MIME 类型设置 Expires 和 Cache-Control: max-age 头
    - _需求: 10.1, 10.2, 10.3_

  - [x] 14.2 编写 ExpiresByType 头设置属性测试
    - **Property 21: ExpiresByType 头设置**
    - 验证 ExpiresActive On 时响应包含正确的 Expires 和 Cache-Control 头
    - **验证: 需求 10.3**

- [x] 15. 指令执行器 — 环境变量
  - [x] 15.1 实现环境变量执行器
    - 创建 `include/htaccess_exec_env.h` 和 `src/htaccess_exec_env.c`
    - 实现 `exec_setenv()` 设置环境变量
    - 实现 `exec_setenvif()` 条件设置（支持 Remote_Addr、Request_URI、User-Agent 属性）
    - 实现 `exec_browser_match()` 基于 User-Agent 匹配设置变量
    - _需求: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

  - [x] 15.2 编写 SetEnvIf 条件设置属性测试
    - **Property 22: SetEnvIf 条件设置**
    - 验证环境变量当且仅当属性值匹配正则时被设置
    - **验证: 需求 11.2, 11.6**

- [x] 16. 检查点 - 指令执行器验证
  - 确保所有测试通过，如有问题请向用户确认。

- [x] 17. 共享内存与暴力破解防护
  - [x] 17.1 实现共享内存管理
    - 创建 `include/htaccess_shm.h` 和 `src/htaccess_shm.c`
    - 实现 `shm_init()` 在 `/dev/shm/ols/` 下创建共享内存区域
    - 实现 `shm_get_record()`、`shm_update_record()` 进行 IP 记录查询和更新
    - 实现 `shm_cleanup_expired()` 清理过期记录
    - 实现 `shm_destroy()` 销毁共享内存
    - 使用文件锁实现进程间同步
    - _需求: 12.2, 12.3, 12.4_

  - [x] 17.2 实现暴力破解防护执行器
    - 创建 `include/htaccess_exec_brute_force.h` 和 `src/htaccess_exec_brute_force.c`
    - 实现 `exec_brute_force()` 函数
    - 支持 BruteForceProtection On/Off
    - 追踪每 IP 失败尝试次数，超过阈值后触发 block（403）或 throttle（延迟响应）
    - 默认值：10 次尝试、300 秒窗口
    - 共享内存分配失败时禁用防护并继续处理请求
    - _需求: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

  - [x] 17.3 编写暴力破解防护触发属性测试
    - **Property 23: 暴力破解防护触发**
    - 验证窗口内第 N+1 次失败后 IP 被标记为受保护状态
    - 验证窗口外的尝试不计入
    - **验证: 需求 12.2, 12.3, 12.4, 12.5**

  - [x] 17.4 编写暴力破解防护单元测试
    - 创建 `tests/unit/test_exec_brute_force.cpp`
    - 测试 block 和 throttle 两种动作
    - 测试默认值（10 次、300 秒）
    - 测试窗口过期后计数重置
    - _需求: 12.1-12.8_

- [x] 18. DirWalker 目录层级遍历
  - [x] 18.1 实现 DirWalker
    - 创建 `include/htaccess_dirwalker.h` 和 `src/htaccess_dirwalker.c`
    - 实现 `htaccess_dirwalk()` 函数
    - 从文档根目录遍历到目标目录，逐层查找 .htaccess 文件
    - 通过 Cache 获取每层的 Directive 列表
    - 合并指令：子目录同类指令覆盖父目录指令
    - 无 .htaccess 文件的目录不影响继承链
    - _需求: 13.1, 13.2, 13.3_

  - [x] 18.2 编写目录层级继承属性测试
    - **Property 24: 目录层级继承**
    - 验证子目录同类指令覆盖父目录、无 .htaccess 目录不影响继承、处理顺序从根到目标
    - **验证: 需求 13.1, 13.2, 13.3**

  - [x] 18.3 编写 DirWalker 单元测试
    - 创建 `tests/unit/test_dirwalker.cpp`
    - 测试单层目录、多层目录、空目录的遍历
    - 测试指令覆盖行为
    - _需求: 13.1, 13.2, 13.3_

- [x] 19. 模块入口与 Hook 注册
  - [x] 19.1 实现模块入口点
    - 创建 `src/mod_htaccess.c`
    - 实现 `lsi_module_t MNAME` 模块描述符
    - 实现 `mod_htaccess_init()` 初始化函数：初始化 Cache、SHM，注册 Hook 回调
    - 实现 `mod_htaccess_cleanup()` 清理函数：释放 Cache、SHM
    - _需求: 1.1, 1.2, 1.4_

  - [x] 19.2 实现请求头 Hook 回调
    - 实现 `on_recv_req_header()` 函数
    - 获取文档根和请求 URI，调用 DirWalker 收集指令
    - 按顺序执行请求阶段指令：访问控制 → 重定向 → PHP 配置 → 环境变量 → 暴力破解防护
    - 访问被拒绝或重定向时立即返回
    - _需求: 1.3, 2.1, 2.4, 6.6, 7.5_

  - [x] 19.3 实现响应头 Hook 回调
    - 实现 `on_send_resp_header()` 函数
    - 执行响应阶段指令：Header 设置 → Expires 设置 → ErrorDocument 处理
    - 支持 FilesMatch 条件过滤
    - _需求: 1.3, 4.1-4.7, 8.1-8.4, 9.1-9.3, 10.1-10.3_

  - [x] 19.4 实现日志与调试支持
    - 在所有组件中集成 OLS 日志接口
    - 指令成功应用时记录 DEBUG 日志（指令类型、文件路径、行号）
    - 指令失败时记录 WARN 日志（含失败原因）
    - 支持可配置的日志级别参数
    - _需求: 14.1, 14.2, 14.3, 14.4_

- [x] 20. 集成与连接
  - [x] 20.1 连接所有组件
    - 确保 mod_htaccess.c 正确调用 DirWalker → Cache → Parser → Executors 的完整链路
    - 确保请求阶段和响应阶段的指令分发逻辑正确
    - 确保错误处理遵循优雅降级原则（单条指令错误不影响其他指令）
    - 验证模块可编译为 .so 文件
    - _需求: 1.1, 1.3, 2.1, 2.4_

  - [x] 20.2 编写集成测试
    - 使用 LSIAPI Mock 层模拟完整请求处理流程
    - 测试请求阶段指令执行链路（ACL → Redirect → PHP → Env → BruteForce）
    - 测试响应阶段指令执行链路（Header → Expires → ErrorDocument）
    - 测试 FilesMatch 条件过滤与嵌套指令执行
    - _需求: 1.3, 2.1, 9.1_

- [x] 21. 最终检查点 - 全部测试通过
  - 确保所有测试通过，如有问题请向用户确认。

## 备注

- 标记 `*` 的任务为可选任务，可跳过以加速 MVP 开发
- 每个任务引用了具体的需求编号以确保可追溯性
- 检查点任务确保增量验证
- 属性测试验证设计文档中定义的 24 条正确性属性
- 单元测试验证具体示例和边界情况
- 所有代码使用 C（模块核心）和 C++（测试代码）编写
