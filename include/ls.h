/**
 * ls.h - LSIAPI Stub Header for OLS .htaccess Module
 *
 * This is a stub header that defines the LSIAPI types, constants, and function
 * declarations needed by the module source code in src/. It mirrors the type
 * definitions in tests/mock_lsiapi.h so that the same module code compiles
 * against both the real LSIAPI (in production) and the mock (in tests).
 *
 * This header is a pure declaration header — no implementations are provided.
 * Implementations come from either the real OLS LSIAPI or the mock layer.
 *
 * Validates: Requirements 1.1
 */
#ifndef LS_H
#define LS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Hook-point constants                                               */
/* ------------------------------------------------------------------ */

#ifndef LSI_HKPT_RECV_REQ_HEADER
#define LSI_HKPT_RECV_REQ_HEADER  0
#endif

#ifndef LSI_HKPT_SEND_RESP_HEADER
#define LSI_HKPT_SEND_RESP_HEADER 1
#endif

/* ------------------------------------------------------------------ */
/*  Module signature                                                   */
/* ------------------------------------------------------------------ */

#ifndef LSI_MODULE_SIGNATURE
#define LSI_MODULE_SIGNATURE 0x4C534900
#endif

/* ------------------------------------------------------------------ */
/*  Return codes                                                       */
/* ------------------------------------------------------------------ */

#define LSI_OK    0
#define LSI_ERROR (-1)

/* ------------------------------------------------------------------ */
/*  Log levels                                                         */
/* ------------------------------------------------------------------ */

#define LSI_LOG_DEBUG 0
#define LSI_LOG_INFO  1
#define LSI_LOG_WARN  2
#define LSI_LOG_ERROR 3

/* ------------------------------------------------------------------ */
/*  Opaque types                                                       */
/* ------------------------------------------------------------------ */

/** Opaque session handle — points to internal OLS session data. */
typedef struct lsi_session_s lsi_session_t;

/* ------------------------------------------------------------------ */
/*  Hook callback signature                                            */
/* ------------------------------------------------------------------ */

typedef int (*lsi_hook_cb)(lsi_session_t *session);

/* ------------------------------------------------------------------ */
/*  Module descriptor                                                  */
/* ------------------------------------------------------------------ */

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
/*  LSIAPI function declarations                                       */
/*  These are the functions that module source code calls.              */
/* ------------------------------------------------------------------ */

/* Request headers */
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

/* Logging */
void        lsi_log(lsi_session_t *session, int level, const char *fmt, ...);

/* ------------------------------------------------------------------ */
/*  v2 LSIAPI extensions                                               */
/* ------------------------------------------------------------------ */

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LS_H */
