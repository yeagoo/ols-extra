/**
 * test_exec_require.cpp - Unit tests for Require access control executor
 *
 * Tests Require all granted/denied, Require ip, Require not ip,
 * RequireAny/RequireAll containers, and Require vs Order/Allow/Deny precedence.
 *
 * Validates: Requirements 8.1-8.7
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <string>

#include "mock_lsiapi.h"

extern "C" {
#include "htaccess_exec_require.h"
#include "htaccess_directive.h"
#include "htaccess_parser.h"
}

/* ---- Helpers ---- */

static htaccess_directive_t *parse(const char *input) {
    return htaccess_parse(input, strlen(input), "test");
}

class RequireExecTest : public ::testing::Test {
public:
    void SetUp() override {
        mock_lsiapi::reset_global_state();
        session_.reset();
    }
protected:
    MockSession session_;
};

/* Require all granted — allows any IP */
TEST_F(RequireExecTest, AllGranted)
{
    const char *input = "Require all granted\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    session_.set_client_ip("10.0.0.1");
    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* Require all denied — denies any IP */
TEST_F(RequireExecTest, AllDenied)
{
    const char *input = "Require all denied\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_ERROR);
    EXPECT_EQ(session_.get_status_code(), 403);

    htaccess_directives_free(dirs);
}

/* Require ip — matching CIDR grants access */
TEST_F(RequireExecTest, IpMatchGrants)
{
    const char *input = "Require ip 192.168.1.0/24\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "192.168.1.50");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* Require ip — non-matching CIDR denies */
TEST_F(RequireExecTest, IpNoMatchDenies)
{
    const char *input = "Require ip 192.168.1.0/24\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_ERROR);
    EXPECT_EQ(session_.get_status_code(), 403);

    htaccess_directives_free(dirs);
}

/* Require not ip — matching CIDR denies */
TEST_F(RequireExecTest, NotIpMatchDenies)
{
    const char *input = "Require not ip 10.0.0.0/8\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_ERROR);
    EXPECT_EQ(session_.get_status_code(), 403);

    htaccess_directives_free(dirs);
}

/* Require not ip — non-matching CIDR grants */
TEST_F(RequireExecTest, NotIpNoMatchGrants)
{
    const char *input = "Require not ip 10.0.0.0/8\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "192.168.1.1");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* RequireAny — OR logic: one match grants */
TEST_F(RequireExecTest, RequireAnyOrLogic)
{
    const char *input =
        "<RequireAny>\n"
        "Require ip 192.168.1.0/24\n"
        "Require ip 10.0.0.0/8\n"
        "</RequireAny>\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    /* Matches second rule */
    int rc = exec_require(session_.handle(), dirs, "10.0.0.5");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* RequireAny — no match denies */
TEST_F(RequireExecTest, RequireAnyNoMatchDenies)
{
    const char *input =
        "<RequireAny>\n"
        "Require ip 192.168.1.0/24\n"
        "Require ip 10.0.0.0/8\n"
        "</RequireAny>\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "172.16.0.1");
    EXPECT_EQ(rc, LSI_ERROR);

    htaccess_directives_free(dirs);
}

/* RequireAll — AND logic: all must match */
TEST_F(RequireExecTest, RequireAllAndLogic)
{
    const char *input =
        "<RequireAll>\n"
        "Require all granted\n"
        "Require not ip 10.0.0.0/8\n"
        "</RequireAll>\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    /* 192.168.1.1: all granted=true, not ip 10.0.0.0/8=true → granted */
    int rc = exec_require(session_.handle(), dirs, "192.168.1.1");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* RequireAll — one fails denies */
TEST_F(RequireExecTest, RequireAllOneFailDenies)
{
    const char *input =
        "<RequireAll>\n"
        "Require all granted\n"
        "Require not ip 10.0.0.0/8\n"
        "</RequireAll>\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    /* 10.0.0.1: all granted=true, not ip 10.0.0.0/8=false → denied */
    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_ERROR);

    htaccess_directives_free(dirs);
}

/* Require takes precedence over Order/Allow/Deny */
TEST_F(RequireExecTest, RequirePrecedenceOverOrderAllowDeny)
{
    const char *input =
        "Order Deny,Allow\n"
        "Deny from all\n"
        "Require all granted\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    /* Require all granted should override Deny from all */
    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_OK);

    /* Verify warning was logged */
    const auto &logs = mock_lsiapi::get_log_records();
    bool found_warn = false;
    for (const auto &rec : logs) {
        if (rec.level == LSI_LOG_WARN &&
            rec.message.find("Require takes precedence") != std::string::npos) {
            found_warn = true;
            break;
        }
    }
    EXPECT_TRUE(found_warn);

    htaccess_directives_free(dirs);
}

/* No Require directives — returns OK (no access control) */
TEST_F(RequireExecTest, NoRequireDirectivesAllows)
{
    const char *input = "Header set X-Test value\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "10.0.0.1");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}

/* Multiple Require ip — space-separated CIDRs */
TEST_F(RequireExecTest, RequireIpMultipleCidrs)
{
    const char *input = "Require ip 192.168.1.0/24 10.0.0.0/8\n";
    auto *dirs = parse(input);
    ASSERT_NE(dirs, nullptr);

    int rc = exec_require(session_.handle(), dirs, "10.5.5.5");
    EXPECT_EQ(rc, LSI_OK);

    htaccess_directives_free(dirs);
}
