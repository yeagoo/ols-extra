# 需求文档：OLS 端到端 CI 测试流水线

## 简介

本功能为 OLS .htaccess 模块新增基于 Docker 的 OpenLiteSpeed 端到端集成测试流水线。现有 CI（`.github/workflows/ci.yml`）仅包含单元测试、属性测试、兼容性测试和 Apache httpd 行为对比测试，缺少在真实 OLS 环境中加载编译后的 `ols_htaccess.so` 模块并验证 .htaccess 指令处理行为的能力。本流水线通过 GitHub Actions 启动真实 OLS Docker 容器，挂载编译产物，配置虚拟主机，并使用 curl 断言验证 HTTP 行为，覆盖 v1 和 v2 指令的请求阶段与响应阶段处理。

此外，本流水线还包含真实 PHP 应用集成测试，通过 Docker Compose 搭建完整的 OLS + PHP + MySQL 环境，自动安装 WordPress、Nextcloud 等主流 PHP 应用，激活常用插件（如缓存插件、安全插件），并验证这些应用生成的 .htaccess 文件在模块处理下的正确行为。本地测试脚本同样大量使用真实 PHP 应用进行端到端验证。

## 术语表

- **OLS**：OpenLiteSpeed，开源高性能 Web 服务器
- **E2E_Pipeline**：端到端测试流水线，在 GitHub Actions 中运行的完整集成测试工作流
- **Test_Runner**：测试执行脚本，负责部署 .htaccess 文件、发送 curl 请求并验证 HTTP 响应
- **OLS_Container**：运行 OpenLiteSpeed 的 Docker 容器实例
- **Module_Artifact**：编译生成的 `ols_htaccess.so` 共享库文件
- **Virtual_Host**：OLS 虚拟主机配置，定义文档根目录和模块加载设置
- **Directive**：.htaccess 指令，模块支持的 59 种指令类型（28 个 v1 + 31 个 v2）
- **Request_Phase_Hook**：请求阶段钩子（`LSI_HKPT_RECV_REQ_HEADER`），处理请求头接收时的指令
- **Response_Phase_Hook**：响应阶段钩子（`LSI_HKPT_SEND_RESP_HEADER`），处理响应头发送时的指令
- **Health_Check**：健康检查，验证 OLS 容器已启动并能正常响应 HTTP 请求
- **Test_Fixture**：测试夹具，包含 .htaccess 文件和预期 HTTP 行为的测试用例集合
- **App_Stack**：应用栈，Docker Compose 定义的 OLS + LSPHP + MySQL/MariaDB 完整运行环境
- **WP_Instance**：WordPress 实例，通过 WP-CLI 自动安装并配置的 WordPress 站点
- **NC_Instance**：Nextcloud 实例，通过 OCC CLI 自动安装并配置的 Nextcloud 站点
- **Plugin_Fixture**：插件测试夹具，安装并激活特定插件后生成的 .htaccess 规则及其预期 HTTP 行为
- **App_Health_Check**：应用健康检查，验证 PHP 应用已完成安装并能正常响应动态页面请求

## 需求

### 需求 1：CI 工作流集成

**用户故事：** 作为开发者，我希望 OLS E2E 测试作为 GitHub Actions 工作流的一部分自动运行，以便每次提交都能验证模块在真实 OLS 环境中的行为。

#### 验收标准

1. THE E2E_Pipeline SHALL 定义为 `.github/workflows/ci.yml` 中的一个独立 job，名称为 `ols-e2e`
2. THE E2E_Pipeline SHALL 依赖 `build-and-test` job 成功完成后才执行
3. THE E2E_Pipeline SHALL 在 `ubuntu-latest` runner 上运行
4. WHEN `build-and-test` job 失败时, THE E2E_Pipeline SHALL 跳过执行
5. WHEN E2E_Pipeline 中任一测试步骤失败时, THE E2E_Pipeline SHALL 收集 OLS 错误日志和访问日志并作为工作流输出展示

### 需求 2：Docker 环境搭建

**用户故事：** 作为开发者，我希望 CI 流水线能自动搭建包含 OLS 的 Docker 环境，以便在真实服务器中测试模块。

#### 验收标准

1. THE E2E_Pipeline SHALL 使用 `litespeedtech/openlitespeed` 官方 Docker 镜像启动 OLS_Container
2. THE E2E_Pipeline SHALL 将编译生成的 Module_Artifact 挂载到 OLS_Container 的模块目录中
3. THE E2E_Pipeline SHALL 在启动 OLS_Container 前重新编译 Module_Artifact 以确保产物与当前代码一致
4. THE OLS_Container SHALL 将 HTTP 端口映射到宿主机可访问的端口
5. THE E2E_Pipeline SHALL 配置 OLS_Container 的 Virtual_Host 启用 .htaccess 文件解析（等效于 Apache 的 `AllowOverride All`）
6. THE E2E_Pipeline SHALL 在 OLS_Container 的文档根目录中创建测试所需的静态文件（HTML、图片、PHP 等）

### 需求 3：OLS 容器健康检查

**用户故事：** 作为开发者，我希望流水线在运行测试前确认 OLS 已完全启动，以避免因服务未就绪导致的误报。

#### 验收标准

1. WHEN OLS_Container 启动后, THE E2E_Pipeline SHALL 执行 Health_Check 轮询直到 OLS 返回 HTTP 200 响应
2. THE Health_Check SHALL 设置最大等待超时时间为 60 秒
3. IF Health_Check 在超时时间内未收到 HTTP 200 响应, THEN THE E2E_Pipeline SHALL 输出 OLS 启动日志并以失败状态终止
4. THE Health_Check SHALL 使用 curl 向 OLS_Container 的根路径发送 GET 请求进行验证

### 需求 4：响应阶段指令 E2E 测试

**用户故事：** 作为开发者，我希望验证模块在 OLS 响应阶段钩子中正确处理 Header、Expires 等指令，以确保响应头被正确修改。

#### 验收标准

1. WHEN 包含 `Header set` 指令的 .htaccess 文件部署到文档根目录时, THE Test_Runner SHALL 验证 OLS 响应中包含对应的自定义响应头
2. WHEN 包含 `Header always set` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 响应中包含对应的响应头
3. WHEN 包含 `Header unset` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 响应中不包含被移除的响应头
4. WHEN 包含 `Header append` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 响应头中包含追加的值
5. WHEN 包含 `ExpiresActive On` 和 `ExpiresByType` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 响应中包含正确的 `Cache-Control max-age` 值
6. WHEN 包含 `ExpiresDefault` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 响应中包含正确的默认缓存控制头

### 需求 5：请求阶段指令 E2E 测试

**用户故事：** 作为开发者，我希望验证模块在 OLS 请求阶段钩子中正确处理重定向、访问控制等指令，以确保请求被正确路由或拦截。

#### 验收标准

1. WHEN 包含 `Redirect 301` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 返回 301 状态码和正确的 `Location` 头
2. WHEN 包含 `Redirect`（无状态码）指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 返回 302 状态码
3. WHEN 包含 `ErrorDocument 404` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 对不存在的资源返回 404 状态码
4. WHEN 包含 `Order Allow,Deny` 和 `Deny from all` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 对被拒绝的客户端返回 403 状态码
5. WHEN 包含 `Require all denied` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 返回 403 状态码
6. WHEN 包含 `Require all granted` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 OLS 返回 200 状态码

### 需求 6：容器指令 E2E 测试

**用户故事：** 作为开发者，我希望验证 IfModule、FilesMatch、Files 等容器指令在 OLS 中的嵌套处理行为。

#### 验收标准

1. WHEN 包含 `<IfModule>` 容器指令且模块存在时, THE Test_Runner SHALL 验证容器内的子指令被正确执行
2. WHEN 包含否定形式 `<IfModule !mod_nonexistent.c>` 容器指令时, THE Test_Runner SHALL 验证容器内的子指令被正确执行
3. WHEN 包含 `<FilesMatch>` 容器指令时, THE Test_Runner SHALL 验证匹配文件的请求应用了容器内的指令，不匹配的请求未应用
4. WHEN 包含 `<Files>` 容器指令时, THE Test_Runner SHALL 验证精确匹配文件名的请求应用了容器内的指令

### 需求 7：环境变量与组合指令 E2E 测试

**用户故事：** 作为开发者，我希望验证 SetEnv 指令和多指令组合场景在 OLS 中的行为，以确保复杂 .htaccess 配置正常工作。

#### 验收标准

1. WHEN 包含 `SetEnv` 指令的 .htaccess 文件部署时, THE Test_Runner SHALL 验证环境变量在后续 Header 指令中可被引用
2. WHEN 包含 WordPress 风格组合 .htaccess（Header + Expires + ErrorDocument）部署时, THE Test_Runner SHALL 验证所有指令同时生效
3. WHEN 包含安全头组合（X-Frame-Options + X-Content-Type-Options + X-XSS-Protection）部署时, THE Test_Runner SHALL 验证所有安全头同时存在于响应中

### 需求 8：Options 指令 E2E 测试

**用户故事：** 作为开发者，我希望验证 Options 指令在 OLS 中正确控制目录列表等服务器行为。

#### 验收标准

1. WHEN 包含 `Options -Indexes` 指令的 .htaccess 文件部署到包含文件但无索引文件的目录时, THE Test_Runner SHALL 验证 OLS 对该目录请求返回 403 状态码
2. WHEN 包含 `Options +Indexes` 指令的 .htaccess 文件部署到无索引文件的目录时, THE Test_Runner SHALL 验证 OLS 对该目录请求返回 200 状态码且响应体包含目录列表内容

### 需求 9：测试脚本结构与可维护性

**用户故事：** 作为开发者，我希望 E2E 测试脚本结构清晰、易于扩展，以便后续添加新指令的测试用例。

#### 验收标准

1. THE Test_Runner SHALL 为每个测试场景独立部署 .htaccess 文件，避免测试间的状态污染
2. THE Test_Runner SHALL 在每个测试断言失败时输出实际的 HTTP 响应内容以便调试
3. THE Test_Runner SHALL 在所有测试完成后输出测试结果摘要，包含通过和失败的测试数量
4. THE E2E_Pipeline SHALL 将 E2E 测试的 .htaccess 样本文件和测试脚本存放在 `tests/e2e/` 目录下

### 需求 10：失败诊断与日志收集

**用户故事：** 作为开发者，我希望在 E2E 测试失败时能快速定位问题，以减少调试时间。

#### 验收标准

1. WHEN E2E_Pipeline 中任一测试步骤失败时, THE E2E_Pipeline SHALL 收集并输出 OLS 的 error.log 内容
2. WHEN E2E_Pipeline 中任一测试步骤失败时, THE E2E_Pipeline SHALL 收集并输出 OLS 的 access.log 内容
3. THE E2E_Pipeline SHALL 输出 OLS_Container 中已加载模块的列表以确认 Module_Artifact 被正确加载
4. IF OLS_Container 启动失败, THEN THE E2E_Pipeline SHALL 输出 Docker 容器日志以便诊断启动问题

### 需求 11：Limit 与 LimitExcept 容器指令 E2E 测试

**用户故事：** 作为开发者，我希望验证 Limit 和 LimitExcept 容器指令在 OLS 中正确限制 HTTP 方法的访问。

#### 验收标准

1. WHEN 包含 `<Limit POST>` 容器指令且内含 `Require all denied` 的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 POST 请求返回 403 状态码而 GET 请求返回 200 状态码
2. WHEN 包含 `<LimitExcept GET>` 容器指令且内含 `Require all denied` 的 .htaccess 文件部署时, THE Test_Runner SHALL 验证 GET 请求返回 200 状态码而 POST 请求返回 403 状态码

### 需求 12：DirectoryIndex 指令 E2E 测试

**用户故事：** 作为开发者，我希望验证 DirectoryIndex 指令在 OLS 中正确设置目录默认文件。

#### 验收标准

1. WHEN 包含 `DirectoryIndex custom.html` 指令的 .htaccess 文件部署到包含 `custom.html` 的目录时, THE Test_Runner SHALL 验证目录请求返回 `custom.html` 的内容

### 需求 13：PHP 应用集成测试 — Docker Compose 环境

**用户故事：** 作为开发者，我希望 CI 流水线能搭建包含 OLS + LSPHP + MySQL 的完整应用栈，以便在真实 PHP 应用环境中测试模块。

#### 验收标准

1. THE E2E_Pipeline SHALL 提供 Docker Compose 配置文件（`tests/e2e/docker-compose.yml`），定义 OLS_Container、LSPHP 和 MySQL/MariaDB 三个服务
2. THE App_Stack SHALL 使用 `litespeedtech/openlitespeed` 镜像并预装 LSPHP 8.1+（含 mysqli、pdo_mysql、gd、zip、curl、mbstring、xml 扩展）
3. THE App_Stack SHALL 将 Module_Artifact 挂载到 OLS_Container 的模块目录并配置自动加载
4. THE App_Stack SHALL 配置 MySQL/MariaDB 服务，预创建 `wordpress`、`nextcloud`、`drupal` 三个数据库和对应用户
5. THE App_Stack SHALL 将 OLS HTTP 端口（8088）映射到宿主机可访问的端口
6. THE App_Stack SHALL 配置 Virtual_Host 启用 .htaccess 文件解析，并为每个 PHP 应用分配独立的子目录或虚拟主机

### 需求 14：WordPress 自动安装与基础验证

**用户故事：** 作为开发者，我希望 CI 流水线能自动安装 WordPress 并验证其在 OLS + 模块环境下正常运行。

#### 验收标准

1. THE E2E_Pipeline SHALL 使用 WP-CLI 自动下载并安装最新稳定版 WordPress 到 OLS_Container 的文档根目录下的 `/wordpress/` 子目录
2. THE E2E_Pipeline SHALL 通过 WP-CLI 执行 `wp core install` 完成 WordPress 初始化（设置站点标题、管理员账号、数据库连接）
3. WHEN WordPress 安装完成后, THE App_Health_Check SHALL 验证 WordPress 首页返回 HTTP 200 且响应体包含站点标题
4. WHEN WordPress 安装完成后, THE Test_Runner SHALL 验证 WordPress 自动生成的 `.htaccess` 文件（含 `<IfModule mod_rewrite.c>` 重写规则）被模块正确解析
5. THE Test_Runner SHALL 验证 WordPress 固定链接（Pretty Permalinks）功能正常工作 — 访问 `/wordpress/sample-post/` 格式的 URL 返回 200 而非 404
6. THE Test_Runner SHALL 验证 WordPress 管理后台 `/wordpress/wp-admin/` 返回 200 或 302（重定向到登录页）

### 需求 15：WordPress 缓存插件集成测试

**用户故事：** 作为开发者，我希望验证 WordPress 缓存插件生成的 .htaccess 规则在 OLS + 模块环境下正确生效，以确保模块兼容主流缓存方案。

#### 验收标准

1. THE E2E_Pipeline SHALL 通过 WP-CLI 安装并激活 WP Super Cache 插件（`wp plugin install wp-super-cache --activate`）
2. WHEN WP Super Cache 激活并启用缓存后, THE Test_Runner SHALL 验证 WordPress 根目录 `.htaccess` 文件中包含 WP Super Cache 注入的 `<IfModule mod_rewrite.c>` 缓存规则
3. WHEN WP Super Cache 缓存生效后, THE Test_Runner SHALL 验证首次访问页面后再次访问时响应头中包含缓存命中标识（如 `X-WP-Super-Cache` 头或 HTML 注释中的缓存标记）
4. THE E2E_Pipeline SHALL 通过 WP-CLI 安装并激活 W3 Total Cache 插件（`wp plugin install w3-total-cache --activate`）
5. WHEN W3 Total Cache 激活后, THE Test_Runner SHALL 验证 WordPress 根目录 `.htaccess` 文件中包含 W3TC 注入的浏览器缓存规则（`ExpiresActive On`、`ExpiresByType` 等）
6. WHEN W3 Total Cache 浏览器缓存启用后, THE Test_Runner SHALL 验证静态资源响应中包含正确的 `Cache-Control` 和 `Expires` 头

### 需求 16：WordPress 安全插件集成测试

**用户故事：** 作为开发者，我希望验证 WordPress 安全插件生成的 .htaccess 规则在 OLS + 模块环境下正确生效。

#### 验收标准

1. THE E2E_Pipeline SHALL 通过 WP-CLI 安装并激活 All In One WP Security 插件（`wp plugin install all-in-one-wp-security-and-firewall --activate`）
2. WHEN All In One WP Security 启用文件保护规则后, THE Test_Runner SHALL 验证直接访问 `wp-config.php` 返回 403 状态码
3. WHEN All In One WP Security 启用目录浏览禁止后, THE Test_Runner SHALL 验证访问 `/wordpress/wp-includes/` 目录返回 403 状态码
4. THE Test_Runner SHALL 验证安全插件注入的 `Header always set X-Content-Type-Options "nosniff"` 等安全头在响应中存在

### 需求 17：Nextcloud 自动安装与验证

**用户故事：** 作为开发者，我希望验证 Nextcloud 在 OLS + 模块环境下正常运行，因为 Nextcloud 的 .htaccess 文件包含大量复杂指令。

#### 验收标准

1. THE E2E_Pipeline SHALL 下载最新稳定版 Nextcloud 并解压到 OLS_Container 的文档根目录下的 `/nextcloud/` 子目录
2. THE E2E_Pipeline SHALL 通过 Nextcloud OCC CLI 执行自动安装（`php occ maintenance:install`），配置数据库连接和管理员账号
3. WHEN Nextcloud 安装完成后, THE App_Health_Check SHALL 验证 Nextcloud 登录页返回 HTTP 200 且响应体包含 Nextcloud 标识
4. WHEN Nextcloud 安装完成后, THE Test_Runner SHALL 验证 Nextcloud 自动生成的 `.htaccess` 文件被模块正确解析（Nextcloud 的 .htaccess 包含 `<IfModule mod_headers.c>`、`Header always set` 安全头、`ErrorDocument 403/404`、`Options -Indexes` 等复杂指令组合）
5. THE Test_Runner SHALL 验证 Nextcloud 的安全头在响应中存在 — 包括 `X-Content-Type-Options: nosniff`、`X-Frame-Options: SAMEORIGIN`、`X-Robots-Tag: noindex, nofollow`
6. THE Test_Runner SHALL 验证 Nextcloud 的 `Options -Indexes` 指令生效 — 访问 `/nextcloud/data/` 目录返回 403

### 需求 18：Drupal 自动安装与验证

**用户故事：** 作为开发者，我希望验证 Drupal 在 OLS + 模块环境下正常运行，因为 Drupal 的 .htaccess 文件包含 FilesMatch、Options、ErrorDocument 等多种指令。

#### 验收标准

1. THE E2E_Pipeline SHALL 使用 Composer 或直接下载方式安装最新稳定版 Drupal 到 `/drupal/` 子目录
2. THE E2E_Pipeline SHALL 通过 Drush CLI 或自动安装脚本完成 Drupal 初始化
3. WHEN Drupal 安装完成后, THE App_Health_Check SHALL 验证 Drupal 首页返回 HTTP 200
4. WHEN Drupal 安装完成后, THE Test_Runner SHALL 验证 Drupal 自动生成的 `.htaccess` 文件被模块正确解析（Drupal 的 .htaccess 包含 `<FilesMatch>` 保护敏感文件、`Options -Indexes +FollowSymLinks`、`ErrorDocument 404 /index.php` 等指令）
5. THE Test_Runner SHALL 验证 Drupal 的 `<FilesMatch>` 规则生效 — 访问 `.htaccess`、`web.config` 等敏感文件返回 403
6. THE Test_Runner SHALL 验证 Drupal 的 Clean URL 功能正常 — 访问 `/drupal/node/1` 格式的 URL 返回 200 而非 404

### 需求 19：Laravel 应用集成测试

**用户故事：** 作为开发者，我希望验证 Laravel 框架在 OLS + 模块环境下正常运行，因为 Laravel 的 public/.htaccess 包含关键的路由重写规则。

#### 验收标准

1. THE E2E_Pipeline SHALL 使用 Composer 创建一个 Laravel 示例项目到 `/laravel/` 子目录
2. THE E2E_Pipeline SHALL 配置 OLS Virtual_Host 将 `/laravel/` 的文档根指向 `public/` 子目录
3. WHEN Laravel 项目部署后, THE App_Health_Check SHALL 验证 Laravel 欢迎页返回 HTTP 200
4. THE Test_Runner SHALL 验证 Laravel 的 `public/.htaccess` 文件被模块正确解析（包含 `Options -MultiViews -Indexes`、`<IfModule mod_rewrite.c>` 路由重写规则）
5. THE Test_Runner SHALL 验证 Laravel 路由正常工作 — 访问 `/laravel/api/test` 等路由返回预期响应而非 404

### 需求 20：PHP 应用 .htaccess 指令覆盖率验证

**用户故事：** 作为开发者，我希望确认真实 PHP 应用使用的 .htaccess 指令都在模块支持范围内，以发现潜在的兼容性缺口。

#### 验收标准

1. THE Test_Runner SHALL 在每个 PHP 应用安装完成后，收集所有 .htaccess 文件并提取使用的指令类型列表
2. THE Test_Runner SHALL 将收集到的指令类型与模块支持的 59 种指令类型进行比对，输出覆盖率报告
3. IF 发现应用使用了模块不支持的指令, THEN THE Test_Runner SHALL 在测试报告中标记为 WARNING（不阻塞 CI），并列出不支持的指令名称
4. THE Test_Runner SHALL 输出每个应用的 .htaccess 指令统计摘要（指令总数、支持数、不支持数、覆盖率百分比）

### 需求 21：PHP 应用插件生成的 .htaccess 规则验证

**用户故事：** 作为开发者，我希望验证 PHP 应用插件动态生成的 .htaccess 规则在模块处理下的正确行为。

#### 验收标准

1. WHEN WordPress 安装 WP Super Cache 并启用缓存后, THE Test_Runner SHALL 对比启用前后的 .htaccess 文件差异，验证新增规则被模块正确解析
2. WHEN WordPress 安装安全插件并启用文件保护后, THE Test_Runner SHALL 验证插件注入的 `<Files>` 和 `<FilesMatch>` 规则生效
3. WHEN Nextcloud 执行 `php occ maintenance:update:htaccess` 后, THE Test_Runner SHALL 验证更新后的 .htaccess 文件被模块正确解析且所有安全头生效
4. THE Test_Runner SHALL 在每次插件激活/配置变更后重新验证应用的核心功能（首页可访问、管理后台可访问、API 端点可访问）

### 需求 22：本地 E2E 测试脚本

**用户故事：** 作为开发者，我希望能在本地运行完整的 PHP 应用集成测试，以便在提交前验证模块兼容性。

#### 验收标准

1. THE E2E_Pipeline SHALL 提供 `tests/e2e/run_app_tests.sh` 脚本，支持在本地通过 Docker Compose 启动完整应用栈并运行所有 PHP 应用测试
2. THE `run_app_tests.sh` SHALL 接受参数选择运行特定应用的测试（`--wordpress`、`--nextcloud`、`--drupal`、`--laravel`、`--all`）
3. THE `run_app_tests.sh` SHALL 在测试完成后自动清理 Docker 容器和卷，避免残留资源
4. THE `run_app_tests.sh` SHALL 输出与 CI 一致的测试结果摘要格式
5. THE E2E_Pipeline SHALL 提供 `tests/e2e/run_directive_tests.sh` 脚本，用于运行纯指令级别的 E2E 测试（不依赖 PHP 应用）
6. THE 本地测试脚本 SHALL 支持 `--keep` 参数保留容器以便手动调试

### 需求 23：PHP 应用测试 CI 集成

**用户故事：** 作为开发者，我希望 PHP 应用集成测试作为 CI 的一部分运行，但不阻塞快速反馈循环。

#### 验收标准

1. THE E2E_Pipeline SHALL 将 PHP 应用集成测试定义为 `.github/workflows/ci.yml` 中的独立 job，名称为 `ols-app-e2e`
2. THE `ols-app-e2e` job SHALL 依赖 `ols-e2e` job 成功完成后才执行
3. THE `ols-app-e2e` job SHALL 设置超时时间为 30 分钟（PHP 应用安装和测试耗时较长）
4. WHEN `ols-app-e2e` job 中任一应用测试失败时, THE E2E_Pipeline SHALL 收集该应用的 OLS 日志、PHP 错误日志和应用日志
5. THE `ols-app-e2e` job SHALL 在测试完成后输出所有应用的测试结果汇总表
