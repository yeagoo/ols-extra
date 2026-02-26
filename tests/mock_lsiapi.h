/**
 * mock_lsiapi.h - LSIAPI Mock Layer for Unit/Property Testing
 *
 * Simulates the LSIAPI interfaces so tests can run without a real OLS instance.
 * All state is stored in memory for test assertions.
 *
 * Validates: Requirements 1.1, 1.3
 */
#ifndef MOCK_LSIAPI_H
#define MOCK_LSIAPI_H

/* Prevent ls.h from redefining the same types when included transitively */
#ifndef LS_H
#define LS_H
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

/* ------------------------------------------------------------------ */
/*  Forward-compatible LSIAPI type stubs                               */
/*  These mirror the types that will live in include/ls.h (task 1.3).  */
/*  The mock provides concrete backing storage behind these handles.   */
/* ------------------------------------------------------------------ */

#ifdef __cplusplus
extern "C" {
#endif

/* Hook-point constants */
#ifndef LSI_HKPT_RECV_REQ_HEADER
#define LSI_HKPT_RECV_REQ_HEADER  0
#endif
#ifndef LSI_HKPT_SEND_RESP_HEADER
#define LSI_HKPT_SEND_RESP_HEADER 1
#endif

/* Module signature */
#ifndef LSI_MODULE_SIGNATURE
#define LSI_MODULE_SIGNATURE 0x4C534900
#endif

/* Return codes */
#define LSI_OK   0
#define LSI_ERROR (-1)

/* Opaque session handle (points to MockSession in C++ land) */
typedef struct lsi_session_s lsi_session_t;

/* Hook callback signature */
typedef int (*lsi_hook_cb)(lsi_session_t *session);

/* Module descriptor */
typedef struct lsi_module_s {
    int          signature;
    int        (*init_cb)(struct lsi_module_s *module);
    int        (*handler_cb)(lsi_session_t *session);
    void        *config_parser;
    const char  *name;
    void        *server_hooks;
    void        *module_data;
} lsi_module_t;

/* ------------------------------------------------------------------ */
/*  LSIAPI function stubs (C linkage)                                  */
/*  These are the functions that module source code calls.              */
/* ------------------------------------------------------------------ */

/* Request info */
const char *lsi_session_get_req_header_by_name(lsi_session_t *session,
                                                const char *name, int name_len,
                                                int *val_len);
int         lsi_session_set_req_header(lsi_session_t *session,
                                       const char *name, int name_len,
                                       const char *val, int val_len);
int         lsi_session_remove_req_header(lsi_session_t *session,
                                          const char *name, int name_len);

/* Response headers */
const char *lsi_session_get_resp_header_by_name(lsi_session_t *session,
                                                 const char *name, int name_len,
                                                 int *val_len);
int         lsi_session_set_resp_header(lsi_session_t *session,
                                        const char *name, int name_len,
                                        const char *val, int val_len);
int         lsi_session_add_resp_header(lsi_session_t *session,
                                        const char *name, int name_len,
                                        const char *val, int val_len);
int         lsi_session_append_resp_header(lsi_session_t *session,
                                           const char *name, int name_len,
                                           const char *val, int val_len);
int         lsi_session_remove_resp_header(lsi_session_t *session,
                                           const char *name, int name_len);

/* Get count of response headers with a given name */
int         lsi_session_get_resp_header_count(lsi_session_t *session,
                                               const char *name, int name_len);

/* Environment variables */
const char *lsi_session_get_env(lsi_session_t *session,
                                const char *name, int name_len,
                                int *val_len);
int         lsi_session_set_env(lsi_session_t *session,
                                const char *name, int name_len,
                                const char *val, int val_len);

/* Response status */
int         lsi_session_get_status(lsi_session_t *session);
int         lsi_session_set_status(lsi_session_t *session, int code);

/* Request URI */
const char *lsi_session_get_uri(lsi_session_t *session, int *uri_len);

/* Document root */
const char *lsi_session_get_doc_root(lsi_session_t *session, int *len);

/* Client IP */
const char *lsi_session_get_client_ip(lsi_session_t *session, int *len);

/* PHP configuration */
int         lsi_session_set_php_ini(lsi_session_t *session,
                                    const char *name, int name_len,
                                    const char *val, int val_len,
                                    int is_admin);

/* Response body (for ErrorDocument text messages) */
int         lsi_session_set_resp_body(lsi_session_t *session,
                                      const char *buf, int len);

/* Hook registration */
int         lsi_register_hook(int hook_point, lsi_hook_cb cb, int priority);

/* Directory options (v2: Options directive) */
int         lsi_session_set_dir_option(lsi_session_t *session,
                                       const char *option, int enabled);
int         lsi_session_get_dir_option(lsi_session_t *session,
                                       const char *option);

/* Internal URI redirect (v2: DirectoryIndex) */
int         lsi_session_set_uri_internal(lsi_session_t *session,
                                         const char *uri, int uri_len);

/* File existence check (v2: DirectoryIndex) */
int         lsi_session_file_exists(lsi_session_t *session,
                                    const char *path);

/* Request method (v2: Limit/LimitExcept) */
const char *lsi_session_get_method(lsi_session_t *session, int *len);

/* Authorization header (v2: AuthType Basic) */
const char *lsi_session_get_auth_header(lsi_session_t *session, int *len);

/* WWW-Authenticate header (v2: AuthType Basic) */
int         lsi_session_set_www_authenticate(lsi_session_t *session,
                                             const char *realm, int realm_len);

/* Logging */
void        lsi_log(lsi_session_t *session, int level, const char *fmt, ...);

#define LSI_LOG_DEBUG 0
#define LSI_LOG_INFO  1
#define LSI_LOG_WARN  2
#define LSI_LOG_ERROR 3

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ================================================================== */
/*  C++ Mock Implementation Classes                                    */
/*  Used by test code to set up state and inspect results.             */
/* ================================================================== */

#ifdef __cplusplus

/* Record of a single PHP ini call */
struct PhpIniRecord {
    std::string name;
    std::string value;
    bool        is_admin;
};

/* Record of a hook registration */
struct HookRecord {
    int          hook_point;
    lsi_hook_cb  callback;
    int          priority;
};

/* Record of a log message */
struct LogRecord {
    int         level;
    std::string message;
};

/**
 * MockSession — the concrete backing for lsi_session_t.
 *
 * Tests create a MockSession, configure its state (headers, URI, etc.),
 * pass its handle to executor functions, then inspect the resulting state.
 */
class MockSession {
public:
    MockSession();
    ~MockSession();

    /* ---- Setup helpers (called by test code) ---- */

    void set_request_uri(const std::string &uri);
    void set_doc_root(const std::string &root);
    void set_client_ip(const std::string &ip);
    void set_status_code(int code);

    void add_request_header(const std::string &name, const std::string &value);
    void add_response_header(const std::string &name, const std::string &value);
    void add_env_var(const std::string &name, const std::string &value);

    /* v2 setup helpers */
    void set_method(const std::string &method);
    void set_auth_header(const std::string &value);
    void add_existing_file(const std::string &path);

    /* ---- Inspection helpers (called by test assertions) ---- */

    std::string get_request_uri() const;
    std::string get_doc_root() const;
    std::string get_client_ip() const;
    int         get_status_code() const;

    /* Request headers */
    bool        has_request_header(const std::string &name) const;
    std::string get_request_header(const std::string &name) const;

    /* Response headers — supports multiple values per name */
    bool        has_response_header(const std::string &name) const;
    std::string get_response_header(const std::string &name) const;
    std::vector<std::string> get_all_response_headers(const std::string &name) const;
    int         count_response_headers(const std::string &name) const;

    /* Environment variables */
    bool        has_env_var(const std::string &name) const;
    std::string get_env_var(const std::string &name) const;

    /* PHP ini records */
    const std::vector<PhpIniRecord> &get_php_ini_records() const;

    /* Response body */
    std::string get_resp_body() const;

    /* v2 inspection helpers */
    std::string get_method() const;
    std::string get_auth_header_value() const;
    int         get_dir_option(const std::string &option) const;
    std::string get_internal_uri() const;
    std::string get_www_authenticate() const;
    bool        file_exists(const std::string &path) const;

    /* Reset all state */
    void reset();

    /* ---- Opaque handle ---- */
    lsi_session_t *handle();

private:
    friend const char *::lsi_session_get_req_header_by_name(lsi_session_t*, const char*, int, int*);
    friend int         ::lsi_session_set_req_header(lsi_session_t*, const char*, int, const char*, int);
    friend int         ::lsi_session_remove_req_header(lsi_session_t*, const char*, int);
    friend const char *::lsi_session_get_resp_header_by_name(lsi_session_t*, const char*, int, int*);
    friend int         ::lsi_session_set_resp_header(lsi_session_t*, const char*, int, const char*, int);
    friend int         ::lsi_session_add_resp_header(lsi_session_t*, const char*, int, const char*, int);
    friend int         ::lsi_session_append_resp_header(lsi_session_t*, const char*, int, const char*, int);
    friend int         ::lsi_session_remove_resp_header(lsi_session_t*, const char*, int);
    friend int         ::lsi_session_get_resp_header_count(lsi_session_t*, const char*, int);
    friend const char *::lsi_session_get_env(lsi_session_t*, const char*, int, int*);
    friend int         ::lsi_session_set_env(lsi_session_t*, const char*, int, const char*, int);
    friend int         ::lsi_session_get_status(lsi_session_t*);
    friend int         ::lsi_session_set_status(lsi_session_t*, int);
    friend const char *::lsi_session_get_uri(lsi_session_t*, int*);
    friend const char *::lsi_session_get_doc_root(lsi_session_t*, int*);
    friend const char *::lsi_session_get_client_ip(lsi_session_t*, int*);
    friend int         ::lsi_session_set_php_ini(lsi_session_t*, const char*, int, const char*, int, int);
    friend int         ::lsi_session_set_resp_body(lsi_session_t*, const char*, int);
    friend int         ::lsi_session_set_dir_option(lsi_session_t*, const char*, int);
    friend int         ::lsi_session_get_dir_option(lsi_session_t*, const char*);
    friend int         ::lsi_session_set_uri_internal(lsi_session_t*, const char*, int);
    friend int         ::lsi_session_file_exists(lsi_session_t*, const char*);
    friend const char *::lsi_session_get_method(lsi_session_t*, int*);
    friend const char *::lsi_session_get_auth_header(lsi_session_t*, int*);
    friend int         ::lsi_session_set_www_authenticate(lsi_session_t*, const char*, int);

    /* Request headers: name → value */
    std::unordered_map<std::string, std::string> req_headers_;

    /* Response headers: name → list of values (supports add/append) */
    std::unordered_map<std::string, std::vector<std::string>> resp_headers_;

    /* Environment variables */
    std::unordered_map<std::string, std::string> env_vars_;

    /* Request metadata */
    std::string request_uri_;
    std::string doc_root_;
    std::string client_ip_;
    int         status_code_;

    /* PHP ini call log */
    std::vector<PhpIniRecord> php_ini_records_;

    /* Response body */
    std::string resp_body_;

    /* v2: Directory options (option name → enabled flag: 1=on, 0=off) */
    std::unordered_map<std::string, int> dir_options_;

    /* v2: Internal redirect URI */
    std::string internal_uri_;

    /* v2: Request method */
    std::string method_;

    /* v2: Authorization header value */
    std::string auth_header_;

    /* v2: WWW-Authenticate realm */
    std::string www_authenticate_;

    /* v2: Set of files that "exist" for file_exists checks */
    std::unordered_map<std::string, bool> existing_files_;
};

/* ---- Global hook registry (for testing hook registration) ---- */

namespace mock_lsiapi {

/* Get all registered hooks */
const std::vector<HookRecord> &get_hook_records();

/* Get all log records */
const std::vector<LogRecord> &get_log_records();

/* Clear global state (hooks + logs) — call in test SetUp */
void reset_global_state();

} /* namespace mock_lsiapi */

#endif /* __cplusplus */

#endif /* MOCK_LSIAPI_H */
