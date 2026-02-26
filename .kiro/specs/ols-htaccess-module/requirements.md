# 需求文档

## 简介

本项目旨在开发一个 OpenLiteSpeed LSIAPI 原生模块（.so），用于在 OpenLiteSpeed 服务器上解析和执行 Apache .htaccess 文件中的指令。该模块受 CyberPanel 的 cyberpanel_ols.so 模块启发，目标是实现对常用 Apache .htaccess 指令的广泛兼容，使从 Apache 迁移到 OpenLiteSpeed 的用户无需大幅修改现有配置。模块将以 C/C++ 编写，编译为 .so 文件，加载到 OpenLiteSpeed 的模块目录（/usr/local/lsws/modules/）中运行。

## 术语表

- **OLS**: OpenLiteSpeed，一个开源的高性能 Web 服务器
- **LSIAPI**: LiteSpeed API，OpenLiteSpeed 提供的原生模块开发接口
- **Module**: 本项目开发的 .htaccess 兼容模块（编译后的 .so 文件）
- **Parser**: 模块中负责解析 .htaccess 文件内容的组件
- **Printer**: 模块中负责将解析后的指令结构体格式化输出为 .htaccess 文本的组件
- **Cache**: 模块中负责缓存已解析 .htaccess 文件的哈希表组件
- **Directive**: .htaccess 文件中的单条配置指令
- **CIDR**: 无类别域间路由，用于表示 IP 地址范围的记法（如 192.168.1.0/24）
- **Hook**: LSIAPI 提供的请求处理管线中的挂载点，模块通过注册 Hook 回调来介入请求处理流程

## 需求

### 需求 1：LSIAPI 模块生命周期管理

**用户故事：** 作为服务器管理员，我希望该模块能作为标准 LSIAPI 模块加载和卸载，以便与 OpenLiteSpeed 无缝集成。

#### 验收标准

1. WHEN OLS starts and the Module is configured in the server configuration, THE Module SHALL register itself with LSIAPI and initialize all internal data structures
2. WHEN OLS shuts down or the Module is unloaded, THE Module SHALL release all allocated memory and cached data
3. THE Module SHALL register Hook callbacks for the receive-request-header and send-response-header processing phases
4. IF the Module fails to initialize, THEN THE Module SHALL log a descriptive error message to the OLS error log and return a non-zero error code to LSIAPI

### 需求 2：.htaccess 文件解析

**用户故事：** 作为服务器管理员，我希望模块能正确解析 .htaccess 文件中的指令，以便在 OLS 上执行 Apache 风格的配置。

#### 验收标准

1. WHEN a request is received, THE Parser SHALL locate and parse the .htaccess file in the requested document root directory
2. WHEN a .htaccess file contains multiple directives, THE Parser SHALL parse each directive into a structured Directive object preserving the original order
3. IF a .htaccess file contains syntax errors, THEN THE Parser SHALL log a warning with the file path and line number and skip the malformed directive
4. IF a .htaccess file does not exist in the document root, THEN THE Module SHALL proceed with request processing without applying any .htaccess directives
5. THE Printer SHALL format Directive objects back into valid .htaccess file text
6. FOR ALL valid .htaccess files, parsing then printing then parsing SHALL produce an equivalent set of Directive objects (round-trip property)

### 需求 3：.htaccess 文件缓存

**用户故事：** 作为服务器管理员，我希望模块能缓存已解析的 .htaccess 文件，以便减少重复解析带来的性能开销。

#### 验收标准

1. WHEN a .htaccess file is successfully parsed, THE Cache SHALL store the parsed Directive objects in a hash table keyed by the file's absolute path
2. WHEN a cached .htaccess file is requested, THE Cache SHALL compare the file's modification time (mtime) with the cached entry's mtime
3. WHEN the file's mtime matches the cached entry's mtime, THE Cache SHALL return the cached Directive objects without re-parsing
4. WHEN the file's mtime differs from the cached entry's mtime, THE Cache SHALL re-parse the file and update the cached entry
5. THE Module SHALL add less than 0.5 milliseconds of processing overhead per request when serving from cache
6. THE Cache SHALL consume no more than 2 kilobytes of memory per cached .htaccess file entry


### 需求 4：Header 指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 设置 HTTP 响应头和请求头，以便控制缓存策略、安全头等。

#### 验收标准

1. WHEN a Header directive with action "set" is encountered, THE Module SHALL set the specified response header to the given value, replacing any existing value
2. WHEN a Header directive with action "unset" is encountered, THE Module SHALL remove the specified response header
3. WHEN a Header directive with action "append" is encountered, THE Module SHALL append the given value to the existing response header, separated by a comma
4. WHEN a Header directive with action "merge" is encountered, THE Module SHALL append the given value to the existing response header only if the value is not already present
5. WHEN a Header directive with action "add" is encountered, THE Module SHALL add a new response header with the specified name and value, even if a header with the same name already exists
6. WHEN a RequestHeader directive with action "set" is encountered, THE Module SHALL set the specified request header to the given value
7. WHEN a RequestHeader directive with action "unset" is encountered, THE Module SHALL remove the specified request header

### 需求 5：PHP 配置指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 覆盖 PHP 配置值，以便在不修改全局 php.ini 的情况下为不同目录设置不同的 PHP 参数。

#### 验收标准

1. WHEN a php_value directive is encountered, THE Module SHALL pass the specified PHP ini setting name and value to the PHP handler for the current request
2. WHEN a php_flag directive is encountered, THE Module SHALL pass the specified PHP ini boolean setting (on/off) to the PHP handler for the current request
3. WHEN a php_admin_value directive is encountered, THE Module SHALL pass the specified PHP admin-level ini setting to the PHP handler, and the setting SHALL NOT be overridable by php_value in subdirectories
4. WHEN a php_admin_flag directive is encountered, THE Module SHALL pass the specified PHP admin-level boolean ini setting to the PHP handler, and the setting SHALL NOT be overridable by php_flag in subdirectories
5. IF a php_value or php_flag directive references a PHP_INI_SYSTEM level setting, THEN THE Module SHALL log a warning and ignore the directive

### 需求 6：访问控制指令支持

**用户故事：** 作为服务器管理员，我希望能通过 .htaccess 控制 IP 访问权限，以便保护敏感目录。

#### 验收标准

1. WHEN an "Order Allow,Deny" directive is encountered, THE Module SHALL default to denying access and then evaluate Allow rules followed by Deny rules
2. WHEN an "Order Deny,Allow" directive is encountered, THE Module SHALL default to allowing access and then evaluate Deny rules followed by Allow rules
3. WHEN an "Allow from" directive specifies a CIDR range, THE Module SHALL grant access to all IP addresses within that CIDR range
4. WHEN a "Deny from" directive specifies a CIDR range, THE Module SHALL deny access to all IP addresses within that CIDR range
5. WHEN an "Allow from" or "Deny from" directive specifies "all", THE Module SHALL apply the rule to all client IP addresses
6. IF a client IP address is denied access after evaluation, THEN THE Module SHALL return a 403 Forbidden response

### 需求 7：重定向指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 配置 URL 重定向，以便管理 URL 变更和 SEO 需求。

#### 验收标准

1. WHEN a Redirect directive is encountered with a status code and target URL, THE Module SHALL return the specified HTTP status code with the Location header set to the target URL
2. WHEN a Redirect directive omits the status code, THE Module SHALL default to a 302 temporary redirect
3. WHEN a RedirectMatch directive is encountered with a regex pattern, THE Module SHALL match the request URI against the regex pattern
4. WHEN a RedirectMatch regex contains capture groups, THE Module SHALL substitute captured values into the target URL using $1, $2, etc. backreferences
5. IF a Redirect or RedirectMatch directive matches the request, THEN THE Module SHALL stop processing subsequent directives and send the redirect response immediately


### 需求 8：自定义错误页面指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 配置自定义错误页面，以便为用户提供友好的错误提示。

#### 验收标准

1. WHEN an ErrorDocument directive specifies an HTTP error code and a local file path, THE Module SHALL serve the specified file as the response body for that error code
2. WHEN an ErrorDocument directive specifies an HTTP error code and an external URL, THE Module SHALL return a 302 redirect to the specified URL for that error code
3. WHEN an ErrorDocument directive specifies an HTTP error code and a quoted text message, THE Module SHALL return the text message as the response body for that error code
4. IF the specified local error document file does not exist, THEN THE Module SHALL fall back to the default OLS error page and log a warning

### 需求 9：FilesMatch 指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 对匹配特定正则表达式的文件名应用指令，以便精细控制文件级别的配置。

#### 验收标准

1. WHEN a FilesMatch directive with a regex pattern is encountered, THE Module SHALL apply the enclosed directives only to files whose names match the regex pattern
2. WHEN a request URI does not match the FilesMatch regex pattern, THE Module SHALL skip all directives enclosed within that FilesMatch block
3. IF a FilesMatch block contains nested directives (such as Header or Deny), THEN THE Module SHALL evaluate those nested directives in their original order

### 需求 10：缓存过期指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 控制浏览器缓存过期时间，以便优化网站加载性能。

#### 验收标准

1. WHEN an "ExpiresActive On" directive is encountered, THE Module SHALL enable expires header processing for the current directory scope
2. WHEN an "ExpiresActive Off" directive is encountered, THE Module SHALL disable expires header processing for the current directory scope
3. WHILE ExpiresActive is On, WHEN an ExpiresByType directive specifies a MIME type and expiration duration, THE Module SHALL set the Expires and Cache-Control max-age headers for responses matching that MIME type
4. THE Module SHALL support expiration duration formats including "access plus N seconds", "access plus N minutes", "access plus N hours", "access plus N days", "access plus N months", and "access plus N years"

### 需求 11：环境变量指令支持

**用户故事：** 作为网站开发者，我希望能通过 .htaccess 设置和条件设置环境变量，以便在应用程序中使用这些变量进行逻辑判断。

#### 验收标准

1. WHEN a SetEnv directive is encountered, THE Module SHALL set the specified environment variable to the given value for the current request
2. WHEN a SetEnvIf directive is encountered with an attribute name, regex pattern, and variable assignment, THE Module SHALL set the environment variable only when the specified request attribute matches the regex pattern
3. WHEN a SetEnvIf directive references the attribute "Remote_Addr", THE Module SHALL match against the client IP address
4. WHEN a SetEnvIf directive references the attribute "Request_URI", THE Module SHALL match against the request URI
5. WHEN a SetEnvIf directive references the attribute "User-Agent", THE Module SHALL match against the User-Agent request header
6. WHEN a BrowserMatch directive is encountered with a regex pattern and variable assignment, THE Module SHALL set the environment variable when the User-Agent header matches the regex pattern

### 需求 12：WordPress 暴力破解防护指令支持

**用户故事：** 作为 WordPress 站点管理员，我希望能通过 .htaccess 启用登录暴力破解防护，以便保护网站免受自动化攻击。

#### 验收标准

1. WHEN a "BruteForceProtection On" directive is encountered, THE Module SHALL enable brute force protection for the current directory scope
2. WHEN a BruteForceAllowedAttempts directive specifies a number N, THE Module SHALL track failed login attempts per client IP and trigger protection after N failed attempts
3. WHEN a BruteForceWindow directive specifies a duration in seconds, THE Module SHALL count failed attempts only within the specified time window
4. WHEN a client IP exceeds the allowed attempts within the configured window, THE Module SHALL apply the action specified by the BruteForceAction directive
5. WHEN a BruteForceAction directive specifies "block", THE Module SHALL return a 403 Forbidden response for subsequent requests from the blocked client IP
6. WHEN a BruteForceAction directive specifies "throttle", THE Module SHALL delay responses to the throttled client IP by the duration specified in the BruteForceThrottleDuration directive
7. IF no BruteForceAllowedAttempts directive is specified, THEN THE Module SHALL default to 10 allowed attempts
8. IF no BruteForceWindow directive is specified, THEN THE Module SHALL default to a 300-second (5-minute) time window

### 需求 13：目录层级继承

**用户故事：** 作为网站开发者，我希望子目录的 .htaccess 文件能继承并覆盖父目录的配置，以便实现灵活的分层配置管理。

#### 验收标准

1. WHEN a request targets a file in a subdirectory, THE Module SHALL process .htaccess files from the document root down to the target directory in order
2. WHEN a subdirectory .htaccess file contains a directive that conflicts with a parent directory directive, THE Module SHALL use the subdirectory directive (child overrides parent)
3. WHEN a subdirectory does not contain a .htaccess file, THE Module SHALL apply only the parent directory's .htaccess directives

### 需求 14：日志与调试

**用户故事：** 作为服务器管理员，我希望模块提供详细的日志输出，以便排查 .htaccess 配置问题。

#### 验收标准

1. THE Module SHALL log all directive processing actions at DEBUG log level through the OLS logging interface
2. WHEN a directive is successfully applied, THE Module SHALL log the directive type, file path, and line number at DEBUG level
3. WHEN a directive fails to apply, THE Module SHALL log the directive type, file path, line number, and failure reason at WARN level
4. THE Module SHALL support a configurable log level parameter that controls the verbosity of module-specific log output
