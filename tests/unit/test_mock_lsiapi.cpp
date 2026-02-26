/**
 * test_mock_lsiapi.cpp - Unit tests for the LSIAPI Mock Layer
 *
 * Verifies that the mock correctly simulates LSIAPI interfaces:
 * request/response headers, env vars, status codes, PHP config,
 * hook registration, and logging.
 */
#include <gtest/gtest.h>
#include "mock_lsiapi.h"

class MockSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_lsiapi::reset_global_state();
        session_.reset();
    }

    MockSession session_;
};

/* ---- Request headers ---- */

TEST_F(MockSessionTest, SetAndGetRequestHeader) {
    session_.add_request_header("Host", "example.com");
    EXPECT_TRUE(session_.has_request_header("Host"));
    EXPECT_EQ(session_.get_request_header("Host"), "example.com");
}

TEST_F(MockSessionTest, RequestHeaderViaCApi) {
    session_.add_request_header("User-Agent", "TestBot/1.0");
    int val_len = 0;
    const char *val = lsi_session_get_req_header_by_name(
        session_.handle(), "User-Agent", 10, &val_len);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::string(val, val_len), "TestBot/1.0");
}

TEST_F(MockSessionTest, SetRequestHeaderViaCApi) {
    lsi_session_set_req_header(session_.handle(), "X-Custom", 8, "value1", 6);
    EXPECT_EQ(session_.get_request_header("X-Custom"), "value1");
}

TEST_F(MockSessionTest, RemoveRequestHeader) {
    session_.add_request_header("X-Remove", "val");
    lsi_session_remove_req_header(session_.handle(), "X-Remove", 8);
    EXPECT_FALSE(session_.has_request_header("X-Remove"));
}

TEST_F(MockSessionTest, MissingRequestHeaderReturnsNull) {
    int val_len = 0;
    const char *val = lsi_session_get_req_header_by_name(
        session_.handle(), "Missing", 7, &val_len);
    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(val_len, 0);
}

/* ---- Response headers ---- */

TEST_F(MockSessionTest, SetResponseHeader) {
    lsi_session_set_resp_header(session_.handle(),
                                "Content-Type", 12, "text/html", 9);
    EXPECT_TRUE(session_.has_response_header("Content-Type"));
    EXPECT_EQ(session_.get_response_header("Content-Type"), "text/html");
    EXPECT_EQ(session_.count_response_headers("Content-Type"), 1);
}

TEST_F(MockSessionTest, SetResponseHeaderReplacesExisting) {
    session_.add_response_header("X-Test", "old");
    lsi_session_set_resp_header(session_.handle(), "X-Test", 6, "new", 3);
    EXPECT_EQ(session_.get_response_header("X-Test"), "new");
    EXPECT_EQ(session_.count_response_headers("X-Test"), 1);
}

TEST_F(MockSessionTest, AddResponseHeader) {
    session_.add_response_header("Set-Cookie", "a=1");
    lsi_session_add_resp_header(session_.handle(),
                                "Set-Cookie", 10, "b=2", 3);
    EXPECT_EQ(session_.count_response_headers("Set-Cookie"), 2);
    auto all = session_.get_all_response_headers("Set-Cookie");
    EXPECT_EQ(all[0], "a=1");
    EXPECT_EQ(all[1], "b=2");
}

TEST_F(MockSessionTest, AppendResponseHeader) {
    session_.add_response_header("X-Append", "val1");
    lsi_session_append_resp_header(session_.handle(),
                                   "X-Append", 8, "val2", 4);
    EXPECT_EQ(session_.get_response_header("X-Append"), "val1, val2");
}

TEST_F(MockSessionTest, AppendToEmptyResponseHeader) {
    lsi_session_append_resp_header(session_.handle(),
                                   "X-New", 5, "first", 5);
    EXPECT_EQ(session_.get_response_header("X-New"), "first");
}

TEST_F(MockSessionTest, RemoveResponseHeader) {
    session_.add_response_header("X-Del", "val");
    lsi_session_remove_resp_header(session_.handle(), "X-Del", 5);
    EXPECT_FALSE(session_.has_response_header("X-Del"));
}

TEST_F(MockSessionTest, GetRespHeaderCountViaCApi) {
    session_.add_response_header("X-Multi", "a");
    session_.add_response_header("X-Multi", "b");
    int count = lsi_session_get_resp_header_count(
        session_.handle(), "X-Multi", 7);
    EXPECT_EQ(count, 2);
}

/* ---- Environment variables ---- */

TEST_F(MockSessionTest, SetAndGetEnvVar) {
    lsi_session_set_env(session_.handle(), "MY_VAR", 6, "hello", 5);
    EXPECT_TRUE(session_.has_env_var("MY_VAR"));
    EXPECT_EQ(session_.get_env_var("MY_VAR"), "hello");
}

TEST_F(MockSessionTest, GetEnvVarViaCApi) {
    session_.add_env_var("TEST_ENV", "world");
    int val_len = 0;
    const char *val = lsi_session_get_env(
        session_.handle(), "TEST_ENV", 8, &val_len);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::string(val, val_len), "world");
}

TEST_F(MockSessionTest, MissingEnvVarReturnsNull) {
    int val_len = 0;
    const char *val = lsi_session_get_env(
        session_.handle(), "NOPE", 4, &val_len);
    EXPECT_EQ(val, nullptr);
    EXPECT_EQ(val_len, 0);
}

/* ---- Status code ---- */

TEST_F(MockSessionTest, DefaultStatusIs200) {
    EXPECT_EQ(lsi_session_get_status(session_.handle()), 200);
}

TEST_F(MockSessionTest, SetAndGetStatus) {
    lsi_session_set_status(session_.handle(), 404);
    EXPECT_EQ(session_.get_status_code(), 404);
    EXPECT_EQ(lsi_session_get_status(session_.handle()), 404);
}

/* ---- Request URI ---- */

TEST_F(MockSessionTest, SetAndGetUri) {
    session_.set_request_uri("/index.html");
    int len = 0;
    const char *uri = lsi_session_get_uri(session_.handle(), &len);
    EXPECT_EQ(std::string(uri, len), "/index.html");
}

/* ---- Document root ---- */

TEST_F(MockSessionTest, SetAndGetDocRoot) {
    session_.set_doc_root("/var/www/html");
    int len = 0;
    const char *root = lsi_session_get_doc_root(session_.handle(), &len);
    EXPECT_EQ(std::string(root, len), "/var/www/html");
}

/* ---- Client IP ---- */

TEST_F(MockSessionTest, SetAndGetClientIp) {
    session_.set_client_ip("192.168.1.100");
    int len = 0;
    const char *ip = lsi_session_get_client_ip(session_.handle(), &len);
    EXPECT_EQ(std::string(ip, len), "192.168.1.100");
}

/* ---- PHP configuration recording ---- */

TEST_F(MockSessionTest, PhpIniRecording) {
    lsi_session_set_php_ini(session_.handle(),
                            "upload_max_filesize", 19,
                            "64M", 3, 0);
    lsi_session_set_php_ini(session_.handle(),
                            "memory_limit", 12,
                            "256M", 4, 1);

    auto &records = session_.get_php_ini_records();
    ASSERT_EQ(records.size(), 2u);

    EXPECT_EQ(records[0].name, "upload_max_filesize");
    EXPECT_EQ(records[0].value, "64M");
    EXPECT_FALSE(records[0].is_admin);

    EXPECT_EQ(records[1].name, "memory_limit");
    EXPECT_EQ(records[1].value, "256M");
    EXPECT_TRUE(records[1].is_admin);
}

/* ---- Response body ---- */

TEST_F(MockSessionTest, SetAndGetRespBody) {
    lsi_session_set_resp_body(session_.handle(),
                              "Not Found", 9);
    EXPECT_EQ(session_.get_resp_body(), "Not Found");
}

/* ---- Hook registration ---- */

TEST_F(MockSessionTest, HookRegistration) {
    auto dummy_cb = [](lsi_session_t *) -> int { return 0; };
    lsi_register_hook(LSI_HKPT_RECV_REQ_HEADER, dummy_cb, 100);
    lsi_register_hook(LSI_HKPT_SEND_RESP_HEADER, dummy_cb, 200);

    auto &hooks = mock_lsiapi::get_hook_records();
    ASSERT_EQ(hooks.size(), 2u);
    EXPECT_EQ(hooks[0].hook_point, LSI_HKPT_RECV_REQ_HEADER);
    EXPECT_EQ(hooks[0].priority, 100);
    EXPECT_EQ(hooks[1].hook_point, LSI_HKPT_SEND_RESP_HEADER);
    EXPECT_EQ(hooks[1].priority, 200);
}

/* ---- Logging ---- */

TEST_F(MockSessionTest, LogRecording) {
    lsi_log(session_.handle(), LSI_LOG_DEBUG, "test %s %d", "msg", 42);
    lsi_log(nullptr, LSI_LOG_WARN, "warning!");

    auto &logs = mock_lsiapi::get_log_records();
    ASSERT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0].level, LSI_LOG_DEBUG);
    EXPECT_EQ(logs[0].message, "test msg 42");
    EXPECT_EQ(logs[1].level, LSI_LOG_WARN);
    EXPECT_EQ(logs[1].message, "warning!");
}

/* ---- Reset ---- */

TEST_F(MockSessionTest, ResetClearsAllState) {
    session_.add_request_header("H", "v");
    session_.add_response_header("R", "v");
    session_.add_env_var("E", "v");
    session_.set_request_uri("/test");
    session_.set_doc_root("/root");
    session_.set_client_ip("1.2.3.4");
    session_.set_status_code(500);
    lsi_session_set_php_ini(session_.handle(), "k", 1, "v", 1, 0);
    lsi_session_set_resp_body(session_.handle(), "body", 4);

    session_.reset();

    EXPECT_FALSE(session_.has_request_header("H"));
    EXPECT_FALSE(session_.has_response_header("R"));
    EXPECT_FALSE(session_.has_env_var("E"));
    EXPECT_EQ(session_.get_request_uri(), "");
    EXPECT_EQ(session_.get_doc_root(), "");
    EXPECT_EQ(session_.get_client_ip(), "");
    EXPECT_EQ(session_.get_status_code(), 200);
    EXPECT_TRUE(session_.get_php_ini_records().empty());
    EXPECT_EQ(session_.get_resp_body(), "");
}
