/**
 * test_exec_header.cpp - Unit tests for Header/RequestHeader executors
 *
 * Tests specific examples for each operation and edge cases.
 *
 * Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

#include "mock_lsiapi.h"

extern "C" {
#include "htaccess_exec_header.h"
#include "htaccess_directive.h"
}

/* ------------------------------------------------------------------ */
/*  Helper                                                             */
/* ------------------------------------------------------------------ */

static htaccess_directive_t *make_dir(directive_type_t type,
                                      const char *name,
                                      const char *value = nullptr)
{
    auto *d = (htaccess_directive_t *)calloc(1, sizeof(htaccess_directive_t));
    d->type = type;
    d->line_number = 1;
    d->name = name ? strdup(name) : nullptr;
    d->value = value ? strdup(value) : nullptr;
    d->next = nullptr;
    return d;
}

static void free_dir(htaccess_directive_t *d)
{
    if (!d) return;
    free(d->name);
    free(d->value);
    free(d);
}

/* ------------------------------------------------------------------ */
/*  Test fixture                                                       */
/* ------------------------------------------------------------------ */

class ExecHeaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mock_lsiapi::reset_global_state();
        session_.reset();
    }

    MockSession session_;
};

/* ------------------------------------------------------------------ */
/*  Header set tests                                                   */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, HeaderSetBasic)
{
    auto *dir = make_dir(DIR_HEADER_SET, "X-Frame-Options", "DENY");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("X-Frame-Options"), "DENY");
    EXPECT_EQ(session_.count_response_headers("X-Frame-Options"), 1);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderSetReplacesExisting)
{
    session_.add_response_header("X-Custom", "old-value");
    auto *dir = make_dir(DIR_HEADER_SET, "X-Custom", "new-value");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("X-Custom"), "new-value");
    EXPECT_EQ(session_.count_response_headers("X-Custom"), 1);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderSetNullValueReturnsError)
{
    auto *dir = make_dir(DIR_HEADER_SET, "X-Test");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_ERROR);
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  Header unset tests                                                 */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, HeaderUnsetRemoves)
{
    session_.add_response_header("X-Powered-By", "PHP/8.0");
    auto *dir = make_dir(DIR_HEADER_UNSET, "X-Powered-By");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_FALSE(session_.has_response_header("X-Powered-By"));
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderUnsetNonExistentIsOk)
{
    auto *dir = make_dir(DIR_HEADER_UNSET, "X-NonExistent");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  Header append tests                                                */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, HeaderAppendToExisting)
{
    session_.add_response_header("Cache-Control", "no-cache");
    auto *dir = make_dir(DIR_HEADER_APPEND, "Cache-Control", "no-store");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("Cache-Control"), "no-cache, no-store");
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderAppendToEmpty)
{
    auto *dir = make_dir(DIR_HEADER_APPEND, "X-New", "value1");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("X-New"), "value1");
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  Header merge tests                                                 */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, HeaderMergeNewValue)
{
    session_.add_response_header("Vary", "Accept");
    auto *dir = make_dir(DIR_HEADER_MERGE, "Vary", "Accept-Encoding");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("Vary"), "Accept, Accept-Encoding");
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderMergeDuplicateSkipped)
{
    session_.add_response_header("Vary", "Accept");
    auto *dir = make_dir(DIR_HEADER_MERGE, "Vary", "Accept");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    /* Value should remain unchanged */
    EXPECT_EQ(session_.get_response_header("Vary"), "Accept");
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderMergeIdempotent)
{
    auto *dir = make_dir(DIR_HEADER_MERGE, "Vary", "Accept-Encoding");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    std::string first = session_.get_response_header("Vary");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    std::string second = session_.get_response_header("Vary");
    EXPECT_EQ(first, second);
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  Header add tests                                                   */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, HeaderAddCreatesNew)
{
    auto *dir = make_dir(DIR_HEADER_ADD, "Set-Cookie", "id=abc");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.count_response_headers("Set-Cookie"), 1);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderAddAccumulates)
{
    session_.add_response_header("Set-Cookie", "id=abc");
    auto *dir = make_dir(DIR_HEADER_ADD, "Set-Cookie", "lang=en");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.count_response_headers("Set-Cookie"), 2);
    auto all = session_.get_all_response_headers("Set-Cookie");
    EXPECT_EQ(all[0], "id=abc");
    EXPECT_EQ(all[1], "lang=en");
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  RequestHeader set tests                                            */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, RequestHeaderSetBasic)
{
    auto *dir = make_dir(DIR_REQUEST_HEADER_SET, "X-Forwarded-For", "10.0.0.1");
    EXPECT_EQ(exec_request_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_request_header("X-Forwarded-For"), "10.0.0.1");
    free_dir(dir);
}

TEST_F(ExecHeaderTest, RequestHeaderSetReplacesExisting)
{
    session_.add_request_header("Authorization", "Bearer old");
    auto *dir = make_dir(DIR_REQUEST_HEADER_SET, "Authorization", "Bearer new");
    EXPECT_EQ(exec_request_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_request_header("Authorization"), "Bearer new");
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  RequestHeader unset tests                                          */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, RequestHeaderUnsetRemoves)
{
    session_.add_request_header("X-Debug", "true");
    auto *dir = make_dir(DIR_REQUEST_HEADER_UNSET, "X-Debug");
    EXPECT_EQ(exec_request_header(session_.handle(), dir), LSI_OK);
    EXPECT_FALSE(session_.has_request_header("X-Debug"));
    free_dir(dir);
}

/* ------------------------------------------------------------------ */
/*  Edge cases                                                         */
/* ------------------------------------------------------------------ */

TEST_F(ExecHeaderTest, NullSessionReturnsError)
{
    auto *dir = make_dir(DIR_HEADER_SET, "X-Test", "val");
    EXPECT_EQ(exec_header(nullptr, dir), LSI_ERROR);
    EXPECT_EQ(exec_request_header(nullptr, dir), LSI_ERROR);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, NullDirectiveReturnsError)
{
    EXPECT_EQ(exec_header(session_.handle(), nullptr), LSI_ERROR);
    EXPECT_EQ(exec_request_header(session_.handle(), nullptr), LSI_ERROR);
}

TEST_F(ExecHeaderTest, NullNameReturnsError)
{
    auto *dir = make_dir(DIR_HEADER_SET, nullptr, "val");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_ERROR);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, InvalidTypeReturnsError)
{
    /* Use a type that doesn't belong to header executor */
    auto *dir = make_dir(DIR_PHP_VALUE, "X-Test", "val");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_ERROR);
    free_dir(dir);
}

TEST_F(ExecHeaderTest, HeaderSetSpecialCharacters)
{
    auto *dir = make_dir(DIR_HEADER_SET, "Content-Security-Policy",
                         "default-src 'self'; script-src 'unsafe-inline'");
    EXPECT_EQ(exec_header(session_.handle(), dir), LSI_OK);
    EXPECT_EQ(session_.get_response_header("Content-Security-Policy"),
              "default-src 'self'; script-src 'unsafe-inline'");
    free_dir(dir);
}
