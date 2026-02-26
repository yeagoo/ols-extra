# 实施计划：OLS 端到端 CI 测试流水线

## 概述

基于设计文档，将实施分为两大阶段：指令级 E2E 测试基础设施（Docker + Bash 测试框架 + CI 集成）和 PHP 应用集成测试（Docker Compose + 应用安装 + 插件测试 + 覆盖率分析）。所有代码使用 Bash + curl 实现，CI 配置使用 YAML。

## 任务

- [x] 1. 搭建 Docker 基础设施与 OLS 配置
  - [x] 1.1 创建 OLS 虚拟主机配置文件 `tests/e2e/conf/vhconf.conf`
    - 设置 `docRoot` 指向 `/var/www/vhosts/test/html`
    - 启用 `enableRewrite 1`、`allowOverride 255`
    - 设置 `autoIndex 0`（默认禁用目录列表）
    - _需求: 2.5, 8.1, 8.2_

  - [x] 1.2 创建 OLS 主配置文件 `tests/e2e/conf/httpd_config.conf`
    - 加载 `ols_htaccess` 模块
    - 定义 listener 绑定 `*:8088`
    - 映射虚拟主机 `test`
    - _需求: 2.2, 2.4_

  - [x] 1.3 创建指令测试用 Dockerfile `tests/e2e/Dockerfile`
    - 基于 `litespeedtech/openlitespeed:latest` 镜像
    - 复制 `build/ols_htaccess.so` 到模块目录
    - 复制 OLS 配置文件
    - 创建测试文档根目录及静态文件（HTML、图片等）
    - 暴露端口 8088
    - _需求: 2.1, 2.2, 2.6_

- [x] 2. 实现 E2E 测试断言辅助库
  - [x] 2.1 创建 `tests/e2e/lib/assertions.sh` 断言函数库
    - 实现 `assert_status_code()` — 验证 HTTP 状态码
    - 实现 `assert_header_exists()` — 验证响应头存在
    - 实现 `assert_header_value()` — 验证响应头值
    - 实现 `assert_header_absent()` — 验证响应头不存在
    - 实现 `assert_body_contains()` — 验证响应体包含文本
    - 实现 `assert_redirect()` — 验证重定向（状态码 + Location）
    - 实现 `deploy_htaccess()` — 通过 `docker cp` 部署 .htaccess 到容器
    - 实现 `cleanup_htaccess()` — 清理 .htaccess
    - 实现 `run_test()` — 执行单个测试（名称 + 断言函数）
    - 实现 `print_summary()` — 输出测试摘要（通过/失败/总数）
    - 定义全局变量 `OLS_HOST`、`OLS_DOCROOT`、`OLS_CONTAINER`、`PASS_COUNT`、`FAIL_COUNT`
    - 每个断言失败时输出实际 HTTP 响应内容
    - _需求: 9.1, 9.2, 9.3_

  - [ ]* 2.2 编写断言库的参数化测试 — 验证 `print_summary` 计数正确性
    - **Property 13: 测试摘要计数正确性**
    - **验证: 需求 9.3**

- [x] 3. 实现响应阶段指令 E2E 测试
  - [x] 3.1 在 `tests/e2e/test_directives.sh` 中实现 Header 指令测试
    - 实现 `test_header_set()` — 验证 `Header set` 指令设置自定义响应头
    - 实现 `test_header_always_set()` — 验证 `Header always set` 指令
    - 实现 `test_header_unset()` — 验证 `Header unset` 移除响应头
    - 实现 `test_header_append()` — 验证 `Header append` 追加响应头值
    - 每个测试独立部署 .htaccess，测试后清理
    - _需求: 4.1, 4.2, 4.3, 4.4, 9.1_

  - [ ]* 3.2 编写 Header 指令参数化属性测试
    - **Property 1: Header 指令响应头正确性**
    - 遍历多组头名称/值组合验证 set/unset/append 行为
    - **验证: 需求 4.1, 4.2, 4.3, 4.4**

  - [x] 3.3 在 `tests/e2e/test_directives.sh` 中实现 Expires 指令测试
    - 实现 `test_expires_by_type()` — 验证 `ExpiresActive On` + `ExpiresByType` 的 `Cache-Control max-age` 值
    - 实现 `test_expires_default()` — 验证 `ExpiresDefault` 的默认缓存控制头
    - _需求: 4.5, 4.6_

  - [ ]* 3.4 编写 Expires 指令参数化属性测试
    - **Property 2: Expires 指令缓存控制正确性**
    - 遍历多种 MIME 类型和过期时长验证 max-age 值
    - **验证: 需求 4.5, 4.6**

- [x] 4. 实现请求阶段指令 E2E 测试
  - [x] 4.1 在 `tests/e2e/test_directives.sh` 中实现 Redirect 指令测试
    - 实现 `test_redirect_301()` — 验证 `Redirect 301` 返回 301 + 正确 Location
    - 实现 `test_redirect_default()` — 验证 `Redirect`（无状态码）返回 302
    - _需求: 5.1, 5.2_

  - [ ]* 4.2 编写 Redirect 指令参数化属性测试
    - **Property 3: Redirect 指令状态码与 Location 正确性**
    - **验证: 需求 5.1, 5.2**

  - [x] 4.3 在 `tests/e2e/test_directives.sh` 中实现访问控制指令测试
    - 实现 `test_error_document()` — 验证 `ErrorDocument 404` 返回 404
    - 实现 `test_acl_deny()` — 验证 `Order Allow,Deny` + `Deny from all` 返回 403
    - 实现 `test_require_denied()` — 验证 `Require all denied` 返回 403
    - 实现 `test_require_granted()` — 验证 `Require all granted` 返回 200
    - _需求: 5.3, 5.4, 5.5, 5.6_

  - [ ]* 4.4 编写访问控制指令参数化属性测试
    - **Property 4: 访问控制指令拒绝/允许正确性**
    - **Property 5: ErrorDocument 指令状态码正确性**
    - **验证: 需求 5.3, 5.4, 5.5, 5.6**

- [x] 5. 实现容器指令与组合指令 E2E 测试
  - [x] 5.1 在 `tests/e2e/test_directives.sh` 中实现容器指令测试
    - 实现 `test_ifmodule()` — 验证 `<IfModule>` 正向条件执行
    - 实现 `test_ifmodule_negated()` — 验证 `<IfModule !mod_nonexistent.c>` 否定条件执行
    - 实现 `test_files_match()` — 验证 `<FilesMatch>` 匹配/不匹配行为
    - 实现 `test_files()` — 验证 `<Files>` 精确匹配行为
    - _需求: 6.1, 6.2, 6.3, 6.4_

  - [ ]* 5.2 编写容器指令参数化属性测试
    - **Property 6: IfModule 容器条件执行正确性**
    - **Property 7: Files/FilesMatch 容器匹配正确性**
    - **验证: 需求 6.1, 6.2, 6.3, 6.4**

  - [x] 5.3 在 `tests/e2e/test_directives.sh` 中实现环境变量与组合指令测试
    - 实现 `test_setenv()` — 验证 `SetEnv` 变量在 Header 指令中可引用
    - 实现 `test_combined_wordpress()` — 验证 WordPress 风格组合 .htaccess（Header + Expires + ErrorDocument）
    - 实现 `test_combined_security()` — 验证安全头组合（X-Frame-Options + X-Content-Type-Options + X-XSS-Protection）
    - _需求: 7.1, 7.2, 7.3_

  - [ ]* 5.4 编写 SetEnv 参数化属性测试
    - **Property 8: SetEnv 环境变量传递正确性**
    - **验证: 需求 7.1**

  - [x] 5.5 在 `tests/e2e/test_directives.sh` 中实现 Options、Limit、DirectoryIndex 指令测试
    - 实现 `test_options_no_indexes()` — 验证 `Options -Indexes` 返回 403
    - 实现 `test_options_indexes()` — 验证 `Options +Indexes` 返回 200 + 目录列表
    - 实现 `test_limit_post()` — 验证 `<Limit POST>` POST 返回 403、GET 返回 200
    - 实现 `test_limit_except_get()` — 验证 `<LimitExcept GET>` GET 返回 200、POST 返回 403
    - 实现 `test_directory_index()` — 验证 `DirectoryIndex custom.html` 返回自定义文件内容
    - _需求: 8.1, 8.2, 11.1, 11.2, 12.1_

  - [ ]* 5.6 编写 Options/Limit/DirectoryIndex 参数化属性测试
    - **Property 9: Options Indexes 目录列表控制正确性**
    - **Property 10: Limit/LimitExcept HTTP 方法限制正确性**
    - **Property 11: DirectoryIndex 默认文件正确性**
    - **验证: 需求 8.1, 8.2, 11.1, 11.2, 12.1**

- [x] 6. 检查点 — 指令级 E2E 测试完整性验证
  - 确保所有指令测试通过，询问用户是否有疑问。

- [x] 7. 实现本地指令测试脚本与 CI 集成
  - [x] 7.1 创建 `tests/e2e/run_directive_tests.sh` 本地入口脚本
    - 编译模块（`cmake -B build && cmake --build build`）
    - 构建 Docker 镜像
    - 启动 OLS 容器
    - 执行健康检查轮询（最多 60 秒，curl 验证 HTTP 200）
    - 运行 `test_directives.sh`
    - 输出测试摘要
    - 支持 `--keep` 参数保留容器
    - 无 `--keep` 时自动清理容器
    - _需求: 3.1, 3.2, 3.3, 3.4, 22.5, 22.6_

  - [ ]* 7.2 编写健康检查轮询属性测试
    - **Property 12: 健康检查轮询收敛性**
    - **验证: 需求 3.1, 3.2, 3.3**

  - [x] 7.3 在 `.github/workflows/ci.yml` 中添加 `ols-e2e` job
    - 依赖 `build-and-test` job
    - 在 `ubuntu-latest` 上运行
    - 步骤：安装构建依赖 → 编译模块 → 构建 Docker 镜像 → 启动容器 → 健康检查 → 运行指令测试
    - 失败时收集 OLS error.log、access.log、模块列表、容器日志
    - _需求: 1.1, 1.2, 1.3, 1.4, 1.5, 2.3, 10.1, 10.2, 10.3, 10.4_

- [x] 8. 搭建 PHP 应用测试 Docker Compose 环境
  - [x] 8.1 创建 PHP 应用测试用 Dockerfile `tests/e2e/Dockerfile.app`
    - 基于 `litespeedtech/openlitespeed:latest`，预装 LSPHP 8.1+
    - 确保包含 mysqli、pdo_mysql、gd、zip、curl、mbstring、xml 扩展
    - 挂载 `ols_htaccess.so` 并配置自动加载
    - 配置虚拟主机启用 .htaccess 解析，为每个 PHP 应用分配独立子目录
    - _需求: 13.2, 13.3, 13.6_

  - [x] 8.2 创建 `tests/e2e/docker-compose.yml` Docker Compose 配置
    - 定义 OLS、LSPHP、MariaDB 三个服务
    - MariaDB 使用 `mariadb:10.11`，配置健康检查
    - 映射 OLS HTTP 端口 8088 到宿主机
    - _需求: 13.1, 13.4, 13.5_

  - [x] 8.3 创建 `tests/e2e/init-db.sql` 数据库初始化脚本
    - 预创建 `wordpress`、`nextcloud`、`drupal` 三个数据库和对应用户
    - _需求: 13.4_

  - [x] 8.4 创建 `tests/e2e/lib/wait_for_services.sh` 服务健康检查等待脚本
    - 等待 MariaDB 和 OLS 均就绪
    - 设置超时机制
    - _需求: 3.1, 3.2, 13.1_

- [x] 9. 实现指令覆盖率分析工具
  - [x] 9.1 创建 `tests/e2e/lib/coverage.sh` 覆盖率分析库
    - 实现 `extract_directives()` — 从 .htaccess 文件提取指令类型列表
    - 实现 `check_coverage()` — 与模块支持的 59 种指令比对
    - 实现 `print_coverage_report()` — 输出覆盖率统计（支持数/不支持数/百分比）
    - 硬编码 59 种支持指令名称映射表
    - 不支持的指令标记为 WARNING（不阻塞 CI）
    - _需求: 20.1, 20.2, 20.3, 20.4_

  - [ ]* 9.2 编写覆盖率分析工具参数化属性测试
    - **Property 14: 指令覆盖率分析正确性**
    - 使用多个不同的 .htaccess 样本验证提取和比对逻辑
    - **验证: 需求 20.1, 20.2, 20.3, 20.4**

- [x] 10. 检查点 — 基础设施与工具完整性验证
  - 确保 Docker 环境、断言库、覆盖率工具均可正常工作，询问用户是否有疑问。

- [x] 11. 实现 WordPress 安装与测试
  - [x] 11.1 在 `tests/e2e/test_apps.sh` 中实现 WordPress 安装与基础验证
    - 实现 `install_wordpress()` — 使用 WP-CLI 下载安装 WordPress 到 `/wordpress/` 子目录
    - 实现 `test_wp_homepage()` — 验证首页返回 HTTP 200 且包含站点标题
    - 实现 `test_wp_permalinks()` — 验证固定链接 `/wordpress/sample-post/` 返回 200
    - 实现 `test_wp_admin()` — 验证 `/wordpress/wp-admin/` 返回 200 或 302
    - 实现 `test_wp_htaccess_parsed()` — 验证 WordPress 生成的 .htaccess 被模块正确解析
    - _需求: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6_

  - [x] 11.2 在 `tests/e2e/test_apps.sh` 中实现 WordPress 缓存插件测试
    - 实现 `install_wp_super_cache()` — 安装并激活 WP Super Cache
    - 实现 `test_wp_super_cache_htaccess()` — 验证 .htaccess 包含缓存规则
    - 实现 `test_wp_super_cache_behavior()` — 验证缓存命中标识
    - 实现 `install_w3_total_cache()` — 安装并激活 W3 Total Cache
    - 实现 `test_w3tc_htaccess()` — 验证 .htaccess 包含浏览器缓存规则
    - 实现 `test_w3tc_browser_cache()` — 验证静态资源的 Cache-Control 和 Expires 头
    - _需求: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6_

  - [x] 11.3 在 `tests/e2e/test_apps.sh` 中实现 WordPress 安全插件测试
    - 实现 `install_wp_security()` — 安装并激活 All In One WP Security
    - 实现 `test_wp_security_file_protection()` — 验证 `wp-config.php` 返回 403
    - 实现 `test_wp_security_directory_browsing()` — 验证 `/wordpress/wp-includes/` 返回 403
    - 实现 `test_wp_security_headers()` — 验证安全头（X-Content-Type-Options 等）存在
    - _需求: 16.1, 16.2, 16.3, 16.4_

  - [ ]* 11.4 编写 WordPress 插件激活后核心功能稳定性测试
    - **Property 15: 插件激活后应用核心功能稳定性**
    - 每次插件激活/配置变更后重新验证首页和管理后台可访问
    - **验证: 需求 21.4**

- [x] 12. 实现 Nextcloud 安装与测试
  - [x] 12.1 在 `tests/e2e/test_apps.sh` 中实现 Nextcloud 安装与验证
    - 实现 `install_nextcloud()` — 下载 Nextcloud 并通过 OCC CLI 自动安装
    - 实现 `test_nc_login_page()` — 验证登录页返回 HTTP 200 且包含 Nextcloud 标识
    - 实现 `test_nc_htaccess_parsed()` — 验证 Nextcloud .htaccess 被模块正确解析
    - 实现 `test_nc_security_headers()` — 验证安全头（X-Content-Type-Options、X-Frame-Options、X-Robots-Tag）
    - 实现 `test_nc_no_indexes()` — 验证 `/nextcloud/data/` 返回 403
    - _需求: 17.1, 17.2, 17.3, 17.4, 17.5, 17.6_

- [x] 13. 实现 Drupal 安装与测试
  - [x] 13.1 在 `tests/e2e/test_apps.sh` 中实现 Drupal 安装与验证
    - 实现 `install_drupal()` — 使用 Composer/Drush 安装 Drupal 到 `/drupal/` 子目录
    - 实现 `test_drupal_homepage()` — 验证首页返回 HTTP 200
    - 实现 `test_drupal_files_match()` — 验证 `<FilesMatch>` 规则生效（.htaccess、web.config 返回 403）
    - 实现 `test_drupal_clean_urls()` — 验证 Clean URL `/drupal/node/1` 返回 200
    - _需求: 18.1, 18.2, 18.3, 18.4, 18.5, 18.6_

- [x] 14. 实现 Laravel 安装与测试
  - [x] 14.1 在 `tests/e2e/test_apps.sh` 中实现 Laravel 安装与验证
    - 实现 `install_laravel()` — 使用 Composer 创建 Laravel 项目到 `/laravel/` 子目录
    - 配置 OLS Virtual_Host 将 `/laravel/` 文档根指向 `public/`
    - 实现 `test_laravel_welcome()` — 验证欢迎页返回 HTTP 200
    - 实现 `test_laravel_routing()` — 验证路由 `/laravel/api/test` 返回预期响应
    - _需求: 19.1, 19.2, 19.3, 19.4, 19.5_

- [x] 15. 实现插件 .htaccess 差异验证与覆盖率收集
  - [x] 15.1 在 `tests/e2e/test_apps.sh` 中实现插件 .htaccess 差异验证
    - 对比 WP Super Cache 启用前后的 .htaccess 差异，验证新增规则被正确解析
    - 验证安全插件注入的 `<Files>` 和 `<FilesMatch>` 规则生效
    - 验证 Nextcloud `occ maintenance:update:htaccess` 后安全头生效
    - _需求: 21.1, 21.2, 21.3_

  - [x] 15.2 在 `tests/e2e/test_apps.sh` 中实现 `collect_htaccess_coverage()` 覆盖率收集
    - 每个 PHP 应用安装完成后收集所有 .htaccess 文件
    - 调用 `coverage.sh` 进行指令覆盖率分析
    - 输出每个应用的指令统计摘要
    - _需求: 20.1, 20.2, 20.3, 20.4_

- [x] 16. 检查点 — PHP 应用测试完整性验证
  - 确保所有 PHP 应用测试通过，询问用户是否有疑问。

- [x] 17. 实现本地应用测试脚本与 CI 集成
  - [x] 17.1 创建 `tests/e2e/run_app_tests.sh` 本地应用测试入口脚本
    - 编译模块
    - `docker-compose up -d` 启动应用栈
    - 等待 MariaDB 和 OLS 健康检查通过
    - 支持参数选择运行特定应用测试（`--wordpress`、`--nextcloud`、`--drupal`、`--laravel`、`--all`）
    - 输出测试汇总表
    - 支持 `--keep` 参数保留容器
    - 无 `--keep` 时 `docker-compose down -v` 清理
    - _需求: 22.1, 22.2, 22.3, 22.4, 22.6_

  - [ ]* 17.2 编写本地测试脚本参数路由属性测试
    - **Property 16: 本地测试脚本参数路由正确性**
    - 验证各参数仅执行对应应用的测试函数
    - **验证: 需求 22.2**

  - [x] 17.3 在 `.github/workflows/ci.yml` 中添加 `ols-app-e2e` job
    - 依赖 `ols-e2e` job 成功完成
    - 设置超时时间 30 分钟
    - 步骤：安装构建依赖 → 编译模块 → `docker-compose up` → 等待服务 → 运行 `test_apps.sh --all`
    - 失败时收集 OLS 日志、PHP 错误日志和应用日志
    - 测试完成后输出所有应用测试结果汇总表
    - `always()` 步骤执行 `docker-compose down -v` 清理
    - _需求: 23.1, 23.2, 23.3, 23.4, 23.5_

- [x] 18. 最终检查点 — 全流水线验证
  - 确保所有测试通过，CI 配置完整，询问用户是否有疑问。

## 备注

- 标记 `*` 的任务为可选任务，可跳过以加速 MVP 交付
- 每个任务引用了具体的需求编号以确保可追溯性
- 检查点任务确保增量验证
- 属性测试通过参数化 Bash 函数实现，验证通用正确性属性
- 所有代码使用 Bash + curl 实现，CI 配置使用 YAML
