/**
 * test_parser.cpp - Unit tests for htaccess_parse()
 *
 * Tests parsing of all directive types, comments, empty lines,
 * syntax errors, FilesMatch blocks, and order preservation.
 *
 * Validates: Requirements 2.1, 2.2, 2.3, 2.4, 9.1
 */
#include <gtest/gtest.h>
#include <cstring>
#include <string>

extern "C" {
#include "htaccess_parser.h"
#include "htaccess_directive.h"
}

#include "mock_lsiapi.h"

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_lsiapi::reset_global_state();
    }

    htaccess_directive_t *parse(const std::string &content) {
        return htaccess_parse(content.c_str(), content.size(), "/test/.htaccess");
    }
};

/* ---- Empty / comment / null input ---- */

TEST_F(ParserTest, NullContentReturnsNull) {
    EXPECT_EQ(htaccess_parse(nullptr, 0, "/test"), nullptr);
}

TEST_F(ParserTest, EmptyContentReturnsNull) {
    EXPECT_EQ(htaccess_parse("", 0, "/test"), nullptr);
}

TEST_F(ParserTest, OnlyCommentsReturnsNull) {
    auto *d = parse("# This is a comment\n# Another comment\n");
    EXPECT_EQ(d, nullptr);
}

TEST_F(ParserTest, OnlyEmptyLinesReturnsNull) {
    auto *d = parse("\n\n\n");
    EXPECT_EQ(d, nullptr);
}

/* ---- Header directives ---- */

TEST_F(ParserTest, HeaderSet) {
    auto *d = parse("Header set X-Frame-Options DENY\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_SET);
    EXPECT_STREQ(d->name, "X-Frame-Options");
    EXPECT_STREQ(d->value, "DENY");
    EXPECT_EQ(d->line_number, 1);
    EXPECT_EQ(d->next, nullptr);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, HeaderUnset) {
    auto *d = parse("Header unset Server\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_UNSET);
    EXPECT_STREQ(d->name, "Server");
    EXPECT_EQ(d->value, nullptr);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, HeaderAppend) {
    auto *d = parse("Header append Cache-Control no-transform\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_APPEND);
    EXPECT_STREQ(d->name, "Cache-Control");
    EXPECT_STREQ(d->value, "no-transform");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, HeaderMerge) {
    auto *d = parse("Header merge Cache-Control public\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_MERGE);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, HeaderAdd) {
    auto *d = parse("Header add Set-Cookie \"session=abc\"\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_ADD);
    EXPECT_STREQ(d->name, "Set-Cookie");
    EXPECT_STREQ(d->value, "session=abc");
    htaccess_directives_free(d);
}

/* ---- RequestHeader directives ---- */

TEST_F(ParserTest, RequestHeaderSet) {
    auto *d = parse("RequestHeader set X-Forwarded-Proto https\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REQUEST_HEADER_SET);
    EXPECT_STREQ(d->name, "X-Forwarded-Proto");
    EXPECT_STREQ(d->value, "https");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, RequestHeaderUnset) {
    auto *d = parse("RequestHeader unset Proxy\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REQUEST_HEADER_UNSET);
    EXPECT_STREQ(d->name, "Proxy");
    htaccess_directives_free(d);
}

/* ---- PHP directives ---- */

TEST_F(ParserTest, PhpValue) {
    auto *d = parse("php_value upload_max_filesize 64M\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_PHP_VALUE);
    EXPECT_STREQ(d->name, "upload_max_filesize");
    EXPECT_STREQ(d->value, "64M");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, PhpFlag) {
    auto *d = parse("php_flag display_errors on\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_PHP_FLAG);
    EXPECT_STREQ(d->name, "display_errors");
    EXPECT_STREQ(d->value, "on");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, PhpFlagOff) {
    auto *d = parse("php_flag display_errors Off\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_PHP_FLAG);
    EXPECT_STREQ(d->value, "Off");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, PhpFlagInvalidValue) {
    auto *d = parse("php_flag display_errors maybe\n");
    EXPECT_EQ(d, nullptr);
    /* Should have logged a warning */
    EXPECT_FALSE(mock_lsiapi::get_log_records().empty());
}

TEST_F(ParserTest, PhpAdminValue) {
    auto *d = parse("php_admin_value open_basedir /var/www\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_PHP_ADMIN_VALUE);
    EXPECT_STREQ(d->name, "open_basedir");
    EXPECT_STREQ(d->value, "/var/www");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, PhpAdminFlag) {
    auto *d = parse("php_admin_flag engine off\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_PHP_ADMIN_FLAG);
    EXPECT_STREQ(d->name, "engine");
    EXPECT_STREQ(d->value, "off");
    htaccess_directives_free(d);
}

/* ---- Access control directives ---- */

TEST_F(ParserTest, OrderAllowDeny) {
    auto *d = parse("Order Allow,Deny\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_ORDER);
    EXPECT_EQ(d->data.acl.order, ORDER_ALLOW_DENY);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, OrderDenyAllow) {
    auto *d = parse("Order Deny,Allow\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_ORDER);
    EXPECT_EQ(d->data.acl.order, ORDER_DENY_ALLOW);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, AllowFromCIDR) {
    auto *d = parse("Allow from 192.168.1.0/24\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_ALLOW_FROM);
    EXPECT_STREQ(d->value, "192.168.1.0/24");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, DenyFromAll) {
    auto *d = parse("Deny from all\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_DENY_FROM);
    EXPECT_STREQ(d->value, "all");
    htaccess_directives_free(d);
}

/* ---- Redirect directives ---- */

TEST_F(ParserTest, RedirectDefault302) {
    auto *d = parse("Redirect /old /new\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REDIRECT);
    EXPECT_STREQ(d->name, "/old");
    EXPECT_STREQ(d->value, "/new");
    EXPECT_EQ(d->data.redirect.status_code, 302);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, RedirectWithStatus) {
    auto *d = parse("Redirect 301 /old-page https://example.com/new-page\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REDIRECT);
    EXPECT_STREQ(d->name, "/old-page");
    EXPECT_STREQ(d->value, "https://example.com/new-page");
    EXPECT_EQ(d->data.redirect.status_code, 301);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, RedirectMatch) {
    auto *d = parse("RedirectMatch 301 ^/blog/(.*)$ https://newblog.com/$1\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REDIRECT_MATCH);
    EXPECT_STREQ(d->data.redirect.pattern, "^/blog/(.*)$");
    EXPECT_STREQ(d->value, "https://newblog.com/$1");
    EXPECT_EQ(d->data.redirect.status_code, 301);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, RedirectMatchDefault302) {
    auto *d = parse("RedirectMatch ^/old/(.*) /new/$1\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_REDIRECT_MATCH);
    EXPECT_EQ(d->data.redirect.status_code, 302);
    htaccess_directives_free(d);
}

/* ---- ErrorDocument ---- */

TEST_F(ParserTest, ErrorDocumentPath) {
    auto *d = parse("ErrorDocument 404 /errors/404.html\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_ERROR_DOCUMENT);
    EXPECT_EQ(d->data.error_doc.error_code, 404);
    EXPECT_STREQ(d->value, "/errors/404.html");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, ErrorDocumentQuotedMessage) {
    auto *d = parse("ErrorDocument 503 \"Service Temporarily Unavailable\"\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_ERROR_DOCUMENT);
    EXPECT_EQ(d->data.error_doc.error_code, 503);
    /* After bug fix (task 1.2): leading quote is preserved so the executor
       can detect text message mode via value[0] == '"' */
    EXPECT_EQ(d->value[0], '"');
    EXPECT_TRUE(strstr(d->value, "Service Temporarily Unavailable") != nullptr);
    htaccess_directives_free(d);
}

/* ---- Expires directives ---- */

TEST_F(ParserTest, ExpiresActiveOn) {
    auto *d = parse("ExpiresActive On\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_EXPIRES_ACTIVE);
    EXPECT_EQ(d->data.expires.active, 1);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, ExpiresActiveOff) {
    auto *d = parse("ExpiresActive Off\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_EXPIRES_ACTIVE);
    EXPECT_EQ(d->data.expires.active, 0);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, ExpiresByType) {
    auto *d = parse("ExpiresByType image/jpeg \"access plus 1 month\"\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_EXPIRES_BY_TYPE);
    EXPECT_STREQ(d->name, "image/jpeg");
    EXPECT_EQ(d->data.expires.duration_sec, 2592000L);
    htaccess_directives_free(d);
}

/* ---- Environment variable directives ---- */

TEST_F(ParserTest, SetEnv) {
    auto *d = parse("SetEnv APP_ENV production\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_SETENV);
    EXPECT_STREQ(d->name, "APP_ENV");
    EXPECT_STREQ(d->value, "production");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, SetEnvIf) {
    auto *d = parse("SetEnvIf Remote_Addr ^192\\.168 local=1\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_SETENVIF);
    EXPECT_STREQ(d->name, "local");
    EXPECT_STREQ(d->value, "1");
    EXPECT_STREQ(d->data.envif.attribute, "Remote_Addr");
    EXPECT_STREQ(d->data.envif.pattern, "^192\\.168");
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BrowserMatch) {
    auto *d = parse("BrowserMatch Googlebot is_bot=1\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BROWSER_MATCH);
    EXPECT_STREQ(d->name, "is_bot");
    EXPECT_STREQ(d->value, "1");
    EXPECT_STREQ(d->data.envif.attribute, "User-Agent");
    EXPECT_STREQ(d->data.envif.pattern, "Googlebot");
    htaccess_directives_free(d);
}

/* ---- Brute force directives ---- */

TEST_F(ParserTest, BruteForceProtectionOn) {
    auto *d = parse("BruteForceProtection On\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_PROTECTION);
    EXPECT_EQ(d->data.brute_force.enabled, 1);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceProtectionOff) {
    auto *d = parse("BruteForceProtection Off\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_PROTECTION);
    EXPECT_EQ(d->data.brute_force.enabled, 0);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceAllowedAttempts) {
    auto *d = parse("BruteForceAllowedAttempts 5\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS);
    EXPECT_EQ(d->data.brute_force.allowed_attempts, 5);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceWindow) {
    auto *d = parse("BruteForceWindow 600\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_WINDOW);
    EXPECT_EQ(d->data.brute_force.window_sec, 600);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceActionBlock) {
    auto *d = parse("BruteForceAction block\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_ACTION);
    EXPECT_EQ(d->data.brute_force.action, BF_ACTION_BLOCK);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceActionThrottle) {
    auto *d = parse("BruteForceAction throttle\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_ACTION);
    EXPECT_EQ(d->data.brute_force.action, BF_ACTION_THROTTLE);
    htaccess_directives_free(d);
}

TEST_F(ParserTest, BruteForceThrottleDuration) {
    auto *d = parse("BruteForceThrottleDuration 5000\n");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_BRUTE_FORCE_THROTTLE_DURATION);
    EXPECT_EQ(d->data.brute_force.throttle_ms, 5000);
    htaccess_directives_free(d);
}

/* ---- FilesMatch block ---- */

TEST_F(ParserTest, FilesMatchBlock) {
    std::string content =
        "<FilesMatch \"\\.php$\">\n"
        "Header set X-Content-Type-Options nosniff\n"
        "Header set X-Frame-Options SAMEORIGIN\n"
        "</FilesMatch>\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_FILES_MATCH);
    EXPECT_STREQ(d->data.files_match.pattern, "\\.php$");
    EXPECT_EQ(d->next, nullptr);

    /* Check children */
    auto *c1 = d->data.files_match.children;
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->type, DIR_HEADER_SET);
    EXPECT_STREQ(c1->name, "X-Content-Type-Options");

    auto *c2 = c1->next;
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(c2->type, DIR_HEADER_SET);
    EXPECT_STREQ(c2->name, "X-Frame-Options");
    EXPECT_EQ(c2->next, nullptr);

    htaccess_directives_free(d);
}

TEST_F(ParserTest, UnclosedFilesMatchDiscarded) {
    std::string content =
        "<FilesMatch \"\\.php$\">\n"
        "Header set X-Test value\n";

    auto *d = parse(content);
    EXPECT_EQ(d, nullptr);
    /* Should have logged a warning about unclosed block */
    auto &logs = mock_lsiapi::get_log_records();
    EXPECT_FALSE(logs.empty());
    bool found_unclosed = false;
    for (auto &log : logs) {
        if (log.message.find("unclosed") != std::string::npos)
            found_unclosed = true;
    }
    EXPECT_TRUE(found_unclosed);
}

/* ---- Order preservation ---- */

TEST_F(ParserTest, PreservesDirectiveOrder) {
    std::string content =
        "Header set X-First one\n"
        "Header set X-Second two\n"
        "Header set X-Third three\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->name, "X-First");
    EXPECT_EQ(d->line_number, 1);

    ASSERT_NE(d->next, nullptr);
    EXPECT_STREQ(d->next->name, "X-Second");
    EXPECT_EQ(d->next->line_number, 2);

    ASSERT_NE(d->next->next, nullptr);
    EXPECT_STREQ(d->next->next->name, "X-Third");
    EXPECT_EQ(d->next->next->line_number, 3);

    EXPECT_EQ(d->next->next->next, nullptr);
    htaccess_directives_free(d);
}

/* ---- Syntax error handling ---- */

TEST_F(ParserTest, SyntaxErrorSkipsLine) {
    std::string content =
        "Header set X-Good value\n"
        "InvalidDirective something\n"
        "Header set X-Also-Good value2\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->name, "X-Good");

    ASSERT_NE(d->next, nullptr);
    EXPECT_STREQ(d->next->name, "X-Also-Good");
    EXPECT_EQ(d->next->next, nullptr);

    /* Should have logged a warning for the invalid line */
    auto &logs = mock_lsiapi::get_log_records();
    EXPECT_FALSE(logs.empty());
    htaccess_directives_free(d);
}

TEST_F(ParserTest, CommentsAndEmptyLinesSkipped) {
    std::string content =
        "# Comment line\n"
        "\n"
        "Header set X-Test value\n"
        "# Another comment\n"
        "\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, DIR_HEADER_SET);
    EXPECT_STREQ(d->name, "X-Test");
    EXPECT_EQ(d->next, nullptr);
    htaccess_directives_free(d);
}

/* ---- Multi-directive file ---- */

TEST_F(ParserTest, MultiDirectiveFile) {
    std::string content =
        "Order Deny,Allow\n"
        "Deny from all\n"
        "Allow from 10.0.0.0/8\n"
        "Header set X-Powered-By OLS\n"
        "php_value memory_limit 256M\n"
        "ExpiresActive On\n"
        "SetEnv APP_ENV staging\n"
        "BruteForceProtection On\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);

    int count = 0;
    for (auto *cur = d; cur; cur = cur->next)
        count++;
    EXPECT_EQ(count, 8);

    /* Verify types in order */
    EXPECT_EQ(d->type, DIR_ORDER);
    EXPECT_EQ(d->next->type, DIR_DENY_FROM);
    EXPECT_EQ(d->next->next->type, DIR_ALLOW_FROM);
    EXPECT_EQ(d->next->next->next->type, DIR_HEADER_SET);
    EXPECT_EQ(d->next->next->next->next->type, DIR_PHP_VALUE);
    EXPECT_EQ(d->next->next->next->next->next->type, DIR_EXPIRES_ACTIVE);
    EXPECT_EQ(d->next->next->next->next->next->next->type, DIR_SETENV);
    EXPECT_EQ(d->next->next->next->next->next->next->next->type, DIR_BRUTE_FORCE_PROTECTION);

    htaccess_directives_free(d);
}

/* ---- Line number tracking ---- */

TEST_F(ParserTest, LineNumbersCorrectWithCommentsAndBlanks) {
    std::string content =
        "# comment\n"
        "\n"
        "Header set X-A val\n"
        "# another comment\n"
        "Header set X-B val\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->line_number, 3);
    ASSERT_NE(d->next, nullptr);
    EXPECT_EQ(d->next->line_number, 5);
    htaccess_directives_free(d);
}

/* ---- FilesMatch with mixed directives ---- */

TEST_F(ParserTest, FilesMatchWithMixedDirectives) {
    std::string content =
        "Header set X-Global global\n"
        "<FilesMatch \"\\.js$\">\n"
        "Header set X-Content-Type application/javascript\n"
        "ExpiresActive On\n"
        "</FilesMatch>\n"
        "Header set X-After after\n";

    auto *d = parse(content);
    ASSERT_NE(d, nullptr);

    /* First: global header */
    EXPECT_EQ(d->type, DIR_HEADER_SET);
    EXPECT_STREQ(d->name, "X-Global");

    /* Second: FilesMatch block */
    auto *fm = d->next;
    ASSERT_NE(fm, nullptr);
    EXPECT_EQ(fm->type, DIR_FILES_MATCH);
    EXPECT_STREQ(fm->data.files_match.pattern, "\\.js$");

    auto *c1 = fm->data.files_match.children;
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->type, DIR_HEADER_SET);
    auto *c2 = c1->next;
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(c2->type, DIR_EXPIRES_ACTIVE);

    /* Third: after header */
    auto *after_d = fm->next;
    ASSERT_NE(after_d, nullptr);
    EXPECT_EQ(after_d->type, DIR_HEADER_SET);
    EXPECT_STREQ(after_d->name, "X-After");

    htaccess_directives_free(d);
}
