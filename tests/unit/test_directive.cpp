/**
 * test_directive.cpp - Unit tests for htaccess_directive_t and related types
 *
 * Tests enum values, struct layout, and htaccess_directives_free().
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "htaccess_directive.h"
}

/* ---- Helper: allocate a directive with strdup'd name/value ---- */

static htaccess_directive_t *make_directive(directive_type_t type,
                                            const char *name,
                                            const char *value,
                                            int line) {
    auto *d = static_cast<htaccess_directive_t *>(
        calloc(1, sizeof(htaccess_directive_t)));
    d->type = type;
    d->line_number = line;
    if (name)  d->name  = strdup(name);
    if (value) d->value = strdup(value);
    d->next = nullptr;
    return d;
}

/* ---- Enum value tests ---- */

TEST(DirectiveEnums, DirectiveTypeHas28Values) {
    /* The last enum value should be 27 (0-indexed) */
    EXPECT_EQ(static_cast<int>(DIR_HEADER_SET), 0);
    EXPECT_EQ(static_cast<int>(DIR_BRUTE_FORCE_THROTTLE_DURATION), 27);
}

TEST(DirectiveEnums, AclOrderValues) {
    EXPECT_EQ(static_cast<int>(ORDER_ALLOW_DENY), 0);
    EXPECT_EQ(static_cast<int>(ORDER_DENY_ALLOW), 1);
}

TEST(DirectiveEnums, BfActionValues) {
    EXPECT_EQ(static_cast<int>(BF_ACTION_BLOCK), 0);
    EXPECT_EQ(static_cast<int>(BF_ACTION_THROTTLE), 1);
}

/* ---- Struct field access tests ---- */

TEST(DirectiveStruct, BasicFieldAccess) {
    htaccess_directive_t d = {};
    d.type = DIR_HEADER_SET;
    d.line_number = 42;
    d.name = nullptr;
    d.value = nullptr;
    d.next = nullptr;

    EXPECT_EQ(d.type, DIR_HEADER_SET);
    EXPECT_EQ(d.line_number, 42);
    EXPECT_EQ(d.next, nullptr);
}

TEST(DirectiveStruct, UnionAclField) {
    htaccess_directive_t d = {};
    d.type = DIR_ORDER;
    d.data.acl.order = ORDER_DENY_ALLOW;
    EXPECT_EQ(d.data.acl.order, ORDER_DENY_ALLOW);
}

TEST(DirectiveStruct, UnionRedirectField) {
    htaccess_directive_t d = {};
    d.type = DIR_REDIRECT;
    d.data.redirect.status_code = 301;
    d.data.redirect.pattern = nullptr;
    EXPECT_EQ(d.data.redirect.status_code, 301);
}

TEST(DirectiveStruct, UnionErrorDocField) {
    htaccess_directive_t d = {};
    d.type = DIR_ERROR_DOCUMENT;
    d.data.error_doc.error_code = 404;
    EXPECT_EQ(d.data.error_doc.error_code, 404);
}

TEST(DirectiveStruct, UnionExpiresField) {
    htaccess_directive_t d = {};
    d.type = DIR_EXPIRES_BY_TYPE;
    d.data.expires.active = 1;
    d.data.expires.duration_sec = 3600;
    EXPECT_EQ(d.data.expires.active, 1);
    EXPECT_EQ(d.data.expires.duration_sec, 3600);
}

TEST(DirectiveStruct, UnionBruteForceField) {
    htaccess_directive_t d = {};
    d.type = DIR_BRUTE_FORCE_PROTECTION;
    d.data.brute_force.enabled = 1;
    d.data.brute_force.allowed_attempts = 10;
    d.data.brute_force.window_sec = 300;
    d.data.brute_force.action = BF_ACTION_THROTTLE;
    d.data.brute_force.throttle_ms = 5000;

    EXPECT_EQ(d.data.brute_force.enabled, 1);
    EXPECT_EQ(d.data.brute_force.allowed_attempts, 10);
    EXPECT_EQ(d.data.brute_force.window_sec, 300);
    EXPECT_EQ(d.data.brute_force.action, BF_ACTION_THROTTLE);
    EXPECT_EQ(d.data.brute_force.throttle_ms, 5000);
}

/* ---- Free function tests ---- */

TEST(DirectiveFree, FreeNullIsNoOp) {
    /* Should not crash */
    htaccess_directives_free(nullptr);
}

TEST(DirectiveFree, FreeSingleNode) {
    auto *d = make_directive(DIR_HEADER_SET, "X-Frame-Options", "DENY", 1);
    htaccess_directives_free(d);
    /* No crash = pass (memory checked by sanitizers) */
}

TEST(DirectiveFree, FreeLinkedList) {
    auto *d1 = make_directive(DIR_HEADER_SET, "H1", "V1", 1);
    auto *d2 = make_directive(DIR_HEADER_UNSET, "H2", nullptr, 2);
    auto *d3 = make_directive(DIR_PHP_VALUE, "upload_max", "64M", 3);
    d1->next = d2;
    d2->next = d3;
    htaccess_directives_free(d1);
}

TEST(DirectiveFree, FreeRedirectWithPattern) {
    auto *d = make_directive(DIR_REDIRECT_MATCH, nullptr, "/new-url", 5);
    d->data.redirect.status_code = 301;
    d->data.redirect.pattern = strdup("^/old/(.*)$");
    htaccess_directives_free(d);
}

TEST(DirectiveFree, FreeFilesMatchWithChildren) {
    /* Create a FilesMatch directive with nested children */
    auto *fm = make_directive(DIR_FILES_MATCH, nullptr, nullptr, 10);
    fm->data.files_match.pattern = strdup("\\.php$");

    auto *child1 = make_directive(DIR_HEADER_SET, "X-Content-Type-Options",
                                  "nosniff", 11);
    auto *child2 = make_directive(DIR_HEADER_SET, "X-Frame-Options",
                                  "SAMEORIGIN", 12);
    child1->next = child2;
    fm->data.files_match.children = child1;

    htaccess_directives_free(fm);
}

TEST(DirectiveFree, FreeSetEnvIfWithFields) {
    auto *d = make_directive(DIR_SETENVIF, "no_gzip", "1", 7);
    d->data.envif.attribute = strdup("User-Agent");
    d->data.envif.pattern = strdup("MSIE [1-6]");
    htaccess_directives_free(d);
}

TEST(DirectiveFree, FreeBrowserMatchWithFields) {
    auto *d = make_directive(DIR_BROWSER_MATCH, "no_gzip", "1", 8);
    d->data.envif.attribute = strdup("User-Agent");
    d->data.envif.pattern = strdup("Googlebot");
    htaccess_directives_free(d);
}
