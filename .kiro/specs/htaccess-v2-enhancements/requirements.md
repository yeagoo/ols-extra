# 需求文档

## 简介

本文档定义 OLS .htaccess 模块 v2 增强版的需求。v2 基于已有的 v1 模块（28 种指令类型、401 个测试通过），通过修复已知 Bug 并新增指令支持，提升对主流 CMS（WordPress、Laravel、Drupal、Magento、Nextcloud、Discuz）和面板环境（cPanel/Plesk）的兼容性。所有新增功能必须保持与 v1 已有 28 种指令类型的向后兼容。

## 术语表

- **Module**: OLS .htaccess 兼容模块（编译后的 ols_htaccess.so 文件）
- **Parser**: 模块中负责解析 .htaccess 文件内容的组件
- **Printer**: 模块中负责将解析后的指令结构体格式化输出为 .htaccess 文本的组件
- **Executor**: 模块中负责执行已解析指令的组件
- **Directive**: .htaccess 文件中的单条配置指令
- **PHP_INI_PERDIR**: PHP 配置级别，允许在 .htaccess 中通过 php_value/php_flag 设置的配置项
- **PHP_INI_SYSTEM**: PHP 配置级别，仅允许在 php.ini 或 httpd.conf 中设置的配置项
- **IfModule_Block**: `<IfModule>` / `</IfModule>` 条件块，根据模块是否加载来决定是否执行内部指令
- **Files_Block**: `<Files>` / `</Files>` 精确文件名匹配块，对匹配的文件应用内部指令
- **Require_Directive**: Apache 2.4 标准访问控制指令（Require all granted/denied、Require ip 等）
- **RequireAny_Block**: `<RequireAny>` 容器块，内部任一 Require 条件满足即允许访问
- **RequireAll_Block**: `<RequireAll>` 容器块，内部所有 Require 条件满足才允许访问
- **Limit_Block**: `<Limit>` / `<LimitExcept>` HTTP 方法限制块
- **AuthType_Basic**: HTTP Basic 认证机制
- **Htpasswd_File**: Apache 格式的密码文件，支持 crypt、MD5（$apr1$）和 bcrypt（$2y$）哈希格式
- **CIDR**: 无类别域间路由，用于表示 IP 地址范围的记法

## 需求

### 需求 1：修复 php_value/php_flag 黑名单错误

**用户故事：** 作为 CMS 管理员（WordPress/Magento/Nextcloud），我希望能通过 .htaccess 中的 php_value 设置 memory_limit、post_max_size 等 PHP_INI_PERDIR 级别配置项，以便为不同站点定制 PHP 运行参数。

#### 验收标准

1. WHEN a php_value directive references "memory_limit", THE Executor SHALL pass the setting to the PHP handler without blocking
2. WHEN a php_value directive references "max_input_time", THE Executor SHALL pass the setting to the PHP handler without blocking
3. WHEN a php_value directive references "post_max_size", THE Executor SHALL pass the setting to the PHP handler without blocking
4. WHEN a php_value directive references "upload_max_filesize", THE Executor SHALL pass the setting to the PHP handler without blocking
5. WHEN a php_value directive references "safe_mode", THE Executor SHALL pass the setting to the PHP handler without blocking, because "safe_mode" is a deprecated setting and no longer a PHP_INI_SYSTEM setting
6. WHEN a php_value directive references a true PHP_INI_SYSTEM setting such as "disable_functions" or "expose_php", THE Executor SHALL log a warning and ignore the directive
7. THE Module SHALL maintain the existing blacklist entries for settings that are genuinely PHP_INI_SYSTEM level (allow_url_fopen, allow_url_include, disable_classes, disable_functions, engine, expose_php, open_basedir, realpath_cache_size, realpath_cache_ttl, upload_tmp_dir, max_file_uploads, sys_temp_dir)

### 需求 2：修复 ErrorDocument 文本消息模式不兼容

**用户故事：** 作为网站开发者，我希望 `ErrorDocument 404 "Custom not found"` 能正确显示自定义文本消息，以便为用户提供友好的错误提示。

#### 验收标准

1. WHEN an ErrorDocument directive specifies a quoted text message (e.g., `ErrorDocument 404 "Custom message"`), THE Parser SHALL preserve the leading quote character in the parsed value so that the Executor can identify text message mode
2. WHEN the Executor receives an ErrorDocument directive whose value starts with a double-quote character, THE Executor SHALL return the unquoted text as the response body
3. WHEN an ErrorDocument directive specifies an external URL, THE Executor SHALL return a 302 redirect to the specified URL
4. WHEN an ErrorDocument directive specifies a local file path starting with "/", THE Executor SHALL serve the specified file as the response body
5. FOR ALL valid ErrorDocument directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 3：`<IfModule>` 条件块解析

**用户故事：** 作为从 Apache 迁移到 OLS 的管理员，我希望模块能正确处理 `<IfModule mod_rewrite.c>` 等条件块，以便 CMS 和面板生成的 .htaccess 文件无需手动修改即可工作。

#### 验收标准

1. WHEN the Parser encounters an `<IfModule module_name>` opening tag, THE Parser SHALL enter IfModule block parsing mode
2. WHEN the Parser encounters an `</IfModule>` closing tag, THE Parser SHALL exit IfModule block parsing mode
3. WHILE in IfModule block parsing mode with a positive condition (no "!" prefix), THE Parser SHALL parse and include all enclosed directives as if the module is loaded
4. WHILE in IfModule block parsing mode with a negated condition ("!" prefix, e.g., `<IfModule !mod_xxx.c>`), THE Parser SHALL skip all enclosed directives
5. WHEN IfModule blocks are nested, THE Parser SHALL correctly handle each nesting level independently
6. IF an IfModule block is not closed before end of file, THEN THE Parser SHALL log a warning with the file path and line number and discard the unclosed block
7. THE Printer SHALL format IfModule blocks back into valid `<IfModule>` / `</IfModule>` text
8. FOR ALL valid .htaccess files containing IfModule blocks, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 4：`Options` 指令

**用户故事：** 作为 cPanel 用户，我希望能通过 .htaccess 中的 `Options -Indexes` 禁止目录列表，以便保护目录内容不被浏览。

#### 验收标准

1. WHEN an Options directive with "-Indexes" is encountered, THE Module SHALL disable directory listing for the current directory scope via LSIAPI
2. WHEN an Options directive with "+Indexes" is encountered, THE Module SHALL enable directory listing for the current directory scope via LSIAPI
3. WHEN an Options directive with "+FollowSymLinks" or "-FollowSymLinks" is encountered, THE Module SHALL set the corresponding symlink following behavior via LSIAPI
4. WHEN an Options directive contains multiple flags (e.g., `Options -Indexes +FollowSymLinks`), THE Parser SHALL parse all flags into a single Directive object
5. THE Printer SHALL format Options directives back into valid text preserving all flags
6. FOR ALL valid Options directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 5：`<Files>` 精确匹配块

**用户故事：** 作为 WordPress/Drupal 管理员，我希望能通过 `<Files wp-config.php>` 保护敏感文件，以便阻止对配置文件的直接访问。

#### 验收标准

1. WHEN the Parser encounters a `<Files filename>` opening tag, THE Parser SHALL create a Files block Directive with the specified filename
2. WHEN a request URI targets a file whose basename exactly matches the Files block filename, THE Module SHALL apply the enclosed directives to that request
3. WHEN a request URI targets a file whose basename does not match the Files block filename, THE Module SHALL skip all directives enclosed within that Files block
4. IF a Files block contains nested directives (such as Deny from all or Require all denied), THEN THE Module SHALL evaluate those nested directives in their original order
5. THE Printer SHALL format Files blocks back into valid `<Files>` / `</Files>` text
6. FOR ALL valid .htaccess files containing Files blocks, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 6：`Header always` 修饰符

**用户故事：** 作为安全工程师，我希望能通过 `Header always set Strict-Transport-Security` 在所有响应（包括错误响应）中设置安全头，以便确保 HSTS 和 CSP 策略始终生效。

#### 验收标准

1. WHEN a Header directive includes the "always" modifier (e.g., `Header always set X-Header value`), THE Parser SHALL parse the directive and record the "always" flag
2. WHEN the Executor processes a Header directive with the "always" flag, THE Executor SHALL set the header on all responses including error responses (4xx, 5xx)
3. WHEN the Executor processes a Header directive without the "always" flag, THE Executor SHALL set the header only on successful responses (as per existing v1 behavior)
4. THE Parser SHALL support the "always" modifier with all Header actions: set, unset, append, merge, add
5. THE Printer SHALL format Header directives with the "always" modifier back into valid text
6. FOR ALL valid Header directives with or without the "always" modifier, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 7：`ExpiresDefault` 指令

**用户故事：** 作为网站开发者，我希望能通过 `ExpiresDefault` 设置所有 MIME 类型的默认过期时间，以便简化缓存配置。

#### 验收标准

1. WHEN an ExpiresDefault directive specifies an expiration duration, THE Parser SHALL parse the directive into a Directive object with the duration in seconds
2. WHILE ExpiresActive is On, WHEN a response MIME type does not match any ExpiresByType rule, THE Executor SHALL apply the ExpiresDefault duration to set the Expires and Cache-Control max-age headers
3. WHILE ExpiresActive is On, WHEN a response MIME type matches an ExpiresByType rule, THE Executor SHALL use the ExpiresByType duration instead of ExpiresDefault
4. THE Printer SHALL format ExpiresDefault directives back into valid text
5. FOR ALL valid ExpiresDefault directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 8：Apache 2.4 `Require` 访问控制语法

**用户故事：** 作为使用 Apache 2.4 配置的管理员，我希望模块支持 `Require all granted`、`Require ip` 等标准访问控制语法，以便面板生成的配置文件能直接工作。

#### 验收标准

1. WHEN a "Require all granted" directive is encountered, THE Module SHALL allow access to all clients
2. WHEN a "Require all denied" directive is encountered, THE Module SHALL deny access to all clients and return a 403 Forbidden response
3. WHEN a "Require ip" directive specifies one or more CIDR ranges or IP addresses, THE Module SHALL allow access only to clients whose IP matches the specified ranges
4. WHEN a "Require not ip" directive specifies one or more CIDR ranges or IP addresses, THE Module SHALL deny access to clients whose IP matches the specified ranges
5. WHEN a `<RequireAny>` block contains multiple Require directives, THE Module SHALL allow access if any one of the enclosed Require conditions is satisfied
6. WHEN a `<RequireAll>` block contains multiple Require directives, THE Module SHALL allow access only if all enclosed Require conditions are satisfied
7. IF a Require directive is used alongside legacy Order/Allow/Deny directives in the same scope, THEN THE Module SHALL log a warning and apply only the Require directives (Apache 2.4 semantics take precedence)
8. THE Printer SHALL format Require directives and RequireAny/RequireAll blocks back into valid text
9. FOR ALL valid Require directives and container blocks, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 9：`<Limit>` / `<LimitExcept>` HTTP 方法限制块

**用户故事：** 作为 WordPress 管理员，我希望能通过 `<Limit>` 块限制特定 HTTP 方法的访问，以便防护 xmlrpc.php 等端点的滥用。

#### 验收标准

1. WHEN the Parser encounters a `<Limit method1 method2 ...>` opening tag, THE Parser SHALL create a Limit block Directive with the specified HTTP methods list
2. WHEN a request uses an HTTP method listed in the Limit block, THE Module SHALL apply the enclosed directives to that request
3. WHEN a request uses an HTTP method not listed in the Limit block, THE Module SHALL skip the enclosed directives
4. WHEN the Parser encounters a `<LimitExcept method1 method2 ...>` opening tag, THE Parser SHALL create a LimitExcept block Directive with the specified HTTP methods list
5. WHEN a request uses an HTTP method not listed in the LimitExcept block, THE Module SHALL apply the enclosed directives to that request
6. WHEN a request uses an HTTP method listed in the LimitExcept block, THE Module SHALL skip the enclosed directives
7. THE Printer SHALL format Limit and LimitExcept blocks back into valid text
8. FOR ALL valid Limit and LimitExcept blocks, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 10：`AuthType Basic` 认证全家桶

**用户故事：** 作为 cPanel 用户，我希望能通过 .htaccess 配置 HTTP Basic 认证来保护目录，以便使用"密码保护目录"功能。

#### 验收标准

1. WHEN an "AuthType Basic" directive is encountered, THE Parser SHALL parse the directive and set the authentication type to Basic
2. WHEN an "AuthName" directive specifies a realm string, THE Parser SHALL parse the quoted realm name into the Directive object
3. WHEN an "AuthUserFile" directive specifies a file path, THE Parser SHALL parse the file path into the Directive object
4. WHEN a "Require valid-user" directive is encountered in the same scope as AuthType Basic, THE Module SHALL require HTTP Basic authentication for all requests to the protected directory
5. WHEN a client sends a request without valid credentials to a protected directory, THE Module SHALL return a 401 Unauthorized response with a WWW-Authenticate header containing the configured realm
6. WHEN a client sends valid credentials (matching an entry in the Htpasswd_File), THE Module SHALL allow the request to proceed
7. THE Module SHALL support reading Htpasswd_File entries in crypt, MD5 ($apr1$), and bcrypt ($2y$) hash formats
8. IF the Htpasswd_File specified by AuthUserFile does not exist or is not readable, THEN THE Module SHALL log a warning and deny access with a 500 Internal Server Error response
9. THE Printer SHALL format AuthType, AuthName, AuthUserFile, and Require valid-user directives back into valid text
10. FOR ALL valid authentication directive sets, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 11：`AddHandler` / `SetHandler` / `AddType` 指令

**用户故事：** 作为 cPanel 用户，我希望模块能解析 `AddHandler`、`SetHandler` 和 `AddType` 指令，以便多 PHP 版本切换配置能被正确识别。

#### 验收标准

1. WHEN an "AddHandler" directive specifies a handler name and one or more file extensions, THE Parser SHALL parse the handler name and extensions into a Directive object
2. WHEN a "SetHandler" directive specifies a handler name, THE Parser SHALL parse the handler name into a Directive object
3. WHEN an "AddType" directive specifies a MIME type and one or more file extensions, THE Parser SHALL parse the MIME type and extensions into a Directive object
4. WHEN the Executor processes an AddHandler or SetHandler directive, THE Executor SHALL log the handler mapping at DEBUG level (actual handler switching requires LSIAPI External App support beyond this module's scope)
5. WHEN the Executor processes an AddType directive, THE Executor SHALL set the Content-Type response header for matching file extensions
6. THE Printer SHALL format AddHandler, SetHandler, and AddType directives back into valid text
7. FOR ALL valid AddHandler, SetHandler, and AddType directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 12：`DirectoryIndex` 指令

**用户故事：** 作为 CMS 管理员，我希望能通过 `DirectoryIndex index.php index.html` 设置目录默认文件，以便 CMS 入口文件能被正确识别。

#### 验收标准

1. WHEN a DirectoryIndex directive specifies one or more filenames, THE Parser SHALL parse all filenames into a Directive object preserving their order
2. WHEN a directory request is received, THE Module SHALL check for the existence of each DirectoryIndex filename in order and serve the first one found
3. IF none of the DirectoryIndex filenames exist in the requested directory, THEN THE Module SHALL fall back to the OLS default directory index behavior
4. THE Printer SHALL format DirectoryIndex directives back into valid text preserving all filenames
5. FOR ALL valid DirectoryIndex directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 13：`ForceType` 指令

**用户故事：** 作为网站开发者，我希望能通过 `ForceType` 强制设置响应的 MIME 类型，以便覆盖服务器的默认类型判断。

#### 验收标准

1. WHEN a ForceType directive specifies a MIME type, THE Parser SHALL parse the MIME type into a Directive object
2. WHEN the Executor processes a ForceType directive, THE Executor SHALL set the Content-Type response header to the specified MIME type
3. THE Printer SHALL format ForceType directives back into valid text
4. FOR ALL valid ForceType directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 14：`AddEncoding` 指令

**用户故事：** 作为网站开发者，我希望能通过 `AddEncoding` 为特定文件扩展名添加内容编码，以便正确处理压缩文件的传输。

#### 验收标准

1. WHEN an AddEncoding directive specifies an encoding type and one or more file extensions, THE Parser SHALL parse the encoding type and extensions into a Directive object
2. WHEN the Executor processes an AddEncoding directive and the request matches a specified file extension, THE Executor SHALL set the Content-Encoding response header to the specified encoding type
3. THE Printer SHALL format AddEncoding directives back into valid text
4. FOR ALL valid AddEncoding directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 15：`AddCharset` 指令

**用户故事：** 作为网站开发者，我希望能通过 `AddCharset` 为特定文件扩展名添加字符集声明，以便浏览器正确解码文件内容。

#### 验收标准

1. WHEN an AddCharset directive specifies a charset name and one or more file extensions, THE Parser SHALL parse the charset name and extensions into a Directive object
2. WHEN the Executor processes an AddCharset directive and the request matches a specified file extension, THE Executor SHALL append the charset parameter to the Content-Type response header
3. THE Printer SHALL format AddCharset directives back into valid text
4. FOR ALL valid AddCharset directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 16：暴力破解防护增强指令

**用户故事：** 作为使用 Cloudflare/反向代理的 WordPress 管理员，我希望暴力破解防护能识别代理后的真实客户端 IP、支持 IP 白名单和自定义保护路径，以便在 CDN 环境下也能有效防护且不误封可信来源。

#### 验收标准

1. WHEN a "BruteForceXForwardedFor On" directive is encountered, THE Module SHALL use the X-Forwarded-For request header to determine the client IP address instead of the direct connection IP
2. WHEN a "BruteForceXForwardedFor Off" directive is encountered or the directive is absent, THE Module SHALL use the direct connection IP address (existing v1 behavior)
3. WHEN a BruteForceWhitelist directive specifies one or more CIDR ranges (comma-separated), THE Module SHALL exempt all IP addresses within those ranges from brute force tracking and blocking
4. WHEN a client IP matches a BruteForceWhitelist entry, THE Module SHALL allow the request to proceed regardless of failed attempt count
5. WHEN a BruteForceProtectPath directive specifies a URL path, THE Module SHALL apply brute force protection to requests targeting that path in addition to the default wp-login.php and xmlrpc.php paths
6. WHEN multiple BruteForceProtectPath directives are specified, THE Module SHALL protect all specified paths
7. THE Parser SHALL parse BruteForceXForwardedFor, BruteForceWhitelist, and BruteForceProtectPath directives into Directive objects
8. THE Printer SHALL format these directives back into valid text
9. FOR ALL valid brute force enhancement directives, parsing then printing then parsing SHALL produce an equivalent Directive object (round-trip property)

### 需求 17：向后兼容性

**用户故事：** 作为现有 OLS 用户，我希望 v2 模块升级后所有 v1 已支持的指令继续正常工作，以便无缝升级。

#### 验收标准

1. THE Module SHALL continue to support all 28 directive types defined in v1 without behavioral changes
2. THE Module SHALL pass all existing v1 unit tests, property tests, and compatibility tests without modification
3. WHEN new directive types are added to the directive_type_t enumeration, THE Module SHALL append new types after the existing entries to preserve binary compatibility
4. THE Parser SHALL continue to correctly parse all .htaccess files that were valid under v1

## 术语表补充

- **BruteForceXForwardedFor**: 暴力破解防护增强指令，启用后使用 X-Forwarded-For 头获取真实客户端 IP（适用于 CDN/反向代理环境）
- **BruteForceWhitelist**: 暴力破解防护增强指令，指定免于暴力破解追踪的可信 IP/CIDR 范围
- **BruteForceProtectPath**: 暴力破解防护增强指令，指定额外需要保护的 URL 路径（除默认的 wp-login.php 和 xmlrpc.php 外）

### 需求 18：v2 新功能兼容性测试套件

**用户故事：** 作为模块开发者，我希望有一套完整的兼容性测试覆盖所有 v2 新增指令，以便在每次代码变更后自动验证功能正确性。

#### 验收标准

1. THE test suite SHALL include compatibility test cases for each v2 new directive type: IfModule, Options, Files, Header always, ExpiresDefault, Require, Limit/LimitExcept, AuthType Basic, AddHandler/SetHandler/AddType, DirectoryIndex, ForceType, AddEncoding, AddCharset, BruteForceXForwardedFor, BruteForceWhitelist, BruteForceProtectPath
2. EACH compatibility test case SHALL verify three aspects: (a) parse completeness — the directive is parsed without errors, (b) round-trip — parse→print→parse produces equivalent directives, (c) execution correctness — the executor produces the expected side effects
3. THE test suite SHALL include sample .htaccess files representing real-world CMS configurations that exercise v2 directives (e.g., WordPress with IfModule blocks, cPanel AuthType Basic, Apache 2.4 Require syntax)
4. THE test suite SHALL include a combined "CyberPanel feature parity" test that parses and executes the full example .htaccess from the CyberPanel module page, verifying all directives are recognized and executed correctly
5. ALL v2 compatibility tests SHALL be runnable via `ctest --test-dir build -R "compat_tests"` alongside existing v1 compatibility tests

### 需求 19：CI 流水线更新

**用户故事：** 作为模块开发者，我希望 GitHub CI 流水线能自动验证所有 v2 新功能，以便每次提交都能确认功能完整性和向后兼容性。

#### 验收标准

1. THE CI pipeline SHALL run all v1 existing tests (unit, property, compatibility) to verify backward compatibility
2. THE CI pipeline SHALL run all v2 new unit tests, property tests, and compatibility tests
3. THE CI pipeline SHALL include an Apache httpd comparison job that tests v2 new directives (IfModule, Options, Files, Header always, ExpiresDefault, Require, Limit, AuthType Basic, AddHandler/AddType, DirectoryIndex) against real Apache behavior using curl assertions
4. THE CI pipeline SHALL verify that the .so shared library builds cleanly in both Debug and Release modes
5. THE CI pipeline SHALL report test results with clear pass/fail status for each test category (v1 unit, v1 property, v1 compat, v2 unit, v2 property, v2 compat, Apache comparison)
6. THE CI pipeline SHALL fail the build if any test fails, preventing merging of broken code
