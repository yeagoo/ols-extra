/**
 * mod_htaccess.c - OLS .htaccess Module Entry Point and Hook Callbacks
 *
 * Implements the LSIAPI module descriptor, initialization/cleanup,
 * request-phase hook (access control, redirect, PHP config, env vars,
 * brute force protection), and response-phase hook (headers, expires,
 * error documents, FilesMatch).
 *
 * Validates: Requirements 1.1, 1.2, 1.3, 1.4, 2.1, 2.4, 4.1-4.7,
 *            6.6, 7.5, 8.1-8.4, 9.1-9.3, 10.1-10.3, 14.1-14.4
 */

#include "ls.h"
#include "htaccess_cache.h"
#include "htaccess_shm.h"
#include "htaccess_dirwalker.h"
#include "htaccess_directive.h"
#include "htaccess_exec_acl.h"
#include "htaccess_exec_redirect.h"
#include "htaccess_exec_php.h"
#include "htaccess_exec_env.h"
#include "htaccess_exec_brute_force.h"
#include "htaccess_exec_header.h"
#include "htaccess_exec_expires.h"
#include "htaccess_exec_error_doc.h"
#include "htaccess_exec_files_match.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define MOD_HTACCESS_CACHE_BUCKETS  64
#define MOD_HTACCESS_SHM_MAX_RECORDS 1024
#define MOD_HTACCESS_HOOK_PRIORITY  100

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */

static int mod_htaccess_init(lsi_module_t *module);
static int on_recv_req_header(lsi_session_t *session);
static int on_send_resp_header(lsi_session_t *session);

/* ------------------------------------------------------------------ */
/*  Module descriptor (19.1)                                           */
/* ------------------------------------------------------------------ */

lsi_module_t MNAME = {
    LSI_MODULE_SIGNATURE,
    mod_htaccess_init,      /* init callback */
    NULL,                   /* handler (not used) */
    NULL,                   /* config_parser */
    "ols-htaccess",         /* module name */
    NULL,                   /* server_hooks (registered via init) */
    NULL                    /* module_data */
};

/* ------------------------------------------------------------------ */
/*  Helper: extract target directory from URI                          */
/*  Strips the filename component, keeping only the directory path.    */
/*  Returns a malloc'd string the caller must free.                    */
/* ------------------------------------------------------------------ */

static char *build_target_dir(const char *doc_root, int doc_root_len,
                              const char *uri, int uri_len)
{
    /* Find last '/' in URI to strip filename */
    int dir_len = uri_len;
    while (dir_len > 0 && uri[dir_len - 1] != '/')
        dir_len--;
    /* dir_len now includes the trailing '/' */
    if (dir_len == 0)
        dir_len = 1; /* at least "/" */

    /* Strip trailing slash from doc_root if present */
    int dr_len = doc_root_len;
    while (dr_len > 0 && doc_root[dr_len - 1] == '/')
        dr_len--;

    /* Strip leading slash from URI dir portion to avoid double slash */
    const char *uri_part = uri;
    int uri_part_len = dir_len;
    if (uri_part_len > 0 && uri_part[0] == '/') {
        uri_part++;
        uri_part_len--;
    }

    /* Allocate: doc_root + "/" + uri_dir + NUL */
    size_t total = (size_t)dr_len + 1 + (size_t)uri_part_len + 1;
    char *result = (char *)malloc(total);
    if (!result)
        return NULL;

    memcpy(result, doc_root, (size_t)dr_len);
    result[dr_len] = '/';
    if (uri_part_len > 0)
        memcpy(result + dr_len + 1, uri_part, (size_t)uri_part_len);
    result[dr_len + 1 + uri_part_len] = '\0';

    return result;
}

/* ------------------------------------------------------------------ */
/*  Helper: extract filename from URI                                  */
/*  Returns pointer into the URI string (no allocation).               */
/* ------------------------------------------------------------------ */

static const char *extract_filename(const char *uri, int uri_len)
{
    int i = uri_len;
    while (i > 0 && uri[i - 1] != '/')
        i--;
    return uri + i;
}

/* ------------------------------------------------------------------ */
/*  Logging helpers (19.4)                                             */
/* ------------------------------------------------------------------ */

/**
 * Log a successful directive application at DEBUG level.
 * Format: "Applying directive <type> at <filepath>:<line>"
 */
static void log_directive_ok(lsi_session_t *session,
                             const htaccess_directive_t *dir,
                             const char *type_str)
{
    const char *file = dir->name ? dir->name : "(unknown)";
    lsi_log(session, LSI_LOG_DEBUG,
            "Applying directive %s at %s:%d",
            type_str, file, dir->line_number);
}

/**
 * Log a directive failure at WARN level.
 * Format: "Directive <type> failed at <filepath>:<line>: <reason>"
 */
static void log_directive_fail(lsi_session_t *session,
                               const htaccess_directive_t *dir,
                               const char *type_str,
                               const char *reason)
{
    const char *file = dir->name ? dir->name : "(unknown)";
    lsi_log(session, LSI_LOG_WARN,
            "Directive %s failed at %s:%d: %s",
            type_str, file, dir->line_number, reason);
}

/**
 * Return a human-readable string for a directive type.
 */
static const char *directive_type_str(directive_type_t type)
{
    switch (type) {
    case DIR_HEADER_SET:                    return "Header set";
    case DIR_HEADER_UNSET:                  return "Header unset";
    case DIR_HEADER_APPEND:                 return "Header append";
    case DIR_HEADER_MERGE:                  return "Header merge";
    case DIR_HEADER_ADD:                    return "Header add";
    case DIR_REQUEST_HEADER_SET:            return "RequestHeader set";
    case DIR_REQUEST_HEADER_UNSET:          return "RequestHeader unset";
    case DIR_PHP_VALUE:                     return "php_value";
    case DIR_PHP_FLAG:                      return "php_flag";
    case DIR_PHP_ADMIN_VALUE:               return "php_admin_value";
    case DIR_PHP_ADMIN_FLAG:                return "php_admin_flag";
    case DIR_ORDER:                         return "Order";
    case DIR_ALLOW_FROM:                    return "Allow from";
    case DIR_DENY_FROM:                     return "Deny from";
    case DIR_REDIRECT:                      return "Redirect";
    case DIR_REDIRECT_MATCH:                return "RedirectMatch";
    case DIR_ERROR_DOCUMENT:                return "ErrorDocument";
    case DIR_FILES_MATCH:                   return "FilesMatch";
    case DIR_EXPIRES_ACTIVE:                return "ExpiresActive";
    case DIR_EXPIRES_BY_TYPE:               return "ExpiresByType";
    case DIR_SETENV:                        return "SetEnv";
    case DIR_SETENVIF:                      return "SetEnvIf";
    case DIR_BROWSER_MATCH:                 return "BrowserMatch";
    case DIR_BRUTE_FORCE_PROTECTION:        return "BruteForceProtection";
    case DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS:  return "BruteForceAllowedAttempts";
    case DIR_BRUTE_FORCE_WINDOW:            return "BruteForceWindow";
    case DIR_BRUTE_FORCE_ACTION:            return "BruteForceAction";
    case DIR_BRUTE_FORCE_THROTTLE_DURATION: return "BruteForceThrottleDuration";
    default:                                return "Unknown";
    }
}

/* ------------------------------------------------------------------ */
/*  Module init / cleanup (19.1)                                       */
/* ------------------------------------------------------------------ */

/**
 * Module initialization — called by LSIAPI when the module is loaded.
 * Initializes cache and shared memory, registers hook callbacks.
 */
static int mod_htaccess_init(lsi_module_t *module)
{
    (void)module;

    /* Initialize cache */
    if (htaccess_cache_init(MOD_HTACCESS_CACHE_BUCKETS) != 0) {
        lsi_log(NULL, LSI_LOG_ERROR,
                "mod_htaccess: failed to initialize cache");
        return LSI_ERROR;
    }

    /* Initialize shared memory for brute force protection */
    if (shm_init("/dev/shm/ols/", MOD_HTACCESS_SHM_MAX_RECORDS) != 0) {
        lsi_log(NULL, LSI_LOG_WARN,
                "mod_htaccess: failed to initialize shared memory, "
                "brute force protection will be disabled");
        /* Non-fatal: continue without brute force protection */
    }

    /* Register hook callbacks */
    if (lsi_register_hook(LSI_HKPT_RECV_REQ_HEADER,
                          on_recv_req_header,
                          MOD_HTACCESS_HOOK_PRIORITY) != 0) {
        lsi_log(NULL, LSI_LOG_ERROR,
                "mod_htaccess: failed to register recv_req_header hook");
        return LSI_ERROR;
    }

    if (lsi_register_hook(LSI_HKPT_SEND_RESP_HEADER,
                          on_send_resp_header,
                          MOD_HTACCESS_HOOK_PRIORITY) != 0) {
        lsi_log(NULL, LSI_LOG_ERROR,
                "mod_htaccess: failed to register send_resp_header hook");
        return LSI_ERROR;
    }

    lsi_log(NULL, LSI_LOG_INFO,
            "mod_htaccess: module initialized successfully");
    return LSI_OK;
}

/**
 * Module cleanup — releases cache and shared memory.
 */
int mod_htaccess_cleanup(lsi_module_t *module)
{
    (void)module;
    htaccess_cache_destroy();
    shm_destroy();
    lsi_log(NULL, LSI_LOG_INFO, "mod_htaccess: module cleaned up");
    return LSI_OK;
}

/* ------------------------------------------------------------------ */
/*  Request-phase hook callback (19.2)                                 */
/* ------------------------------------------------------------------ */

/**
 * on_recv_req_header — called at LSI_HKPT_RECV_REQ_HEADER.
 *
 * Flow:
 * 1. Get doc_root and request URI
 * 2. Build target directory (strip filename from URI)
 * 3. Call DirWalker to collect merged directives
 * 4. Execute request-phase directives in order:
 *    a. Access control — return immediately on deny
 *    b. Redirects — return immediately on match
 *    c. PHP configuration
 *    d. Environment variables
 *    e. Brute force protection
 * 5. Free directives and return
 */
static int on_recv_req_header(lsi_session_t *session)
{
    int doc_root_len = 0;
    const char *doc_root = lsi_session_get_doc_root(session, &doc_root_len);
    if (!doc_root || doc_root_len <= 0) {
        lsi_log(session, LSI_LOG_DEBUG,
                "mod_htaccess: no document root, skipping");
        return LSI_OK;
    }

    int uri_len = 0;
    const char *uri = lsi_session_get_uri(session, &uri_len);
    if (!uri || uri_len <= 0) {
        lsi_log(session, LSI_LOG_DEBUG,
                "mod_htaccess: no request URI, skipping");
        return LSI_OK;
    }

    /* Build target directory path */
    char *target_dir = build_target_dir(doc_root, doc_root_len, uri, uri_len);
    if (!target_dir) {
        lsi_log(session, LSI_LOG_WARN,
                "mod_htaccess: failed to allocate target directory");
        return LSI_OK;
    }

    /* Collect merged directives via DirWalker */
    htaccess_directive_t *directives = htaccess_dirwalk(session, doc_root,
                                                         target_dir);
    free(target_dir);

    if (!directives) {
        lsi_log(session, LSI_LOG_DEBUG,
                "mod_htaccess: no directives found for request");
        return LSI_OK;
    }

    /* (a) Access control */
    int rc = exec_access_control(session, directives);
    if (rc == LSI_ERROR) {
        lsi_log(session, LSI_LOG_DEBUG,
                "mod_htaccess: access denied by ACL");
        htaccess_directives_free(directives);
        return LSI_OK;
    }

    /* (b) Redirects */
    const htaccess_directive_t *dir;
    for (dir = directives; dir != NULL; dir = dir->next) {
        int redir_rc = 0;
        if (dir->type == DIR_REDIRECT) {
            redir_rc = exec_redirect(session, dir);
            if (redir_rc > 0) {
                log_directive_ok(session, dir, "Redirect");
                htaccess_directives_free(directives);
                return LSI_OK;
            } else if (redir_rc < 0) {
                log_directive_fail(session, dir, "Redirect",
                                   "execution error");
            }
        } else if (dir->type == DIR_REDIRECT_MATCH) {
            redir_rc = exec_redirect_match(session, dir);
            if (redir_rc > 0) {
                log_directive_ok(session, dir, "RedirectMatch");
                htaccess_directives_free(directives);
                return LSI_OK;
            } else if (redir_rc < 0) {
                log_directive_fail(session, dir, "RedirectMatch",
                                   "execution error");
            }
        }
    }

    /* (c) PHP configuration */
    for (dir = directives; dir != NULL; dir = dir->next) {
        int php_rc = LSI_OK;
        const char *type_name = directive_type_str(dir->type);
        switch (dir->type) {
        case DIR_PHP_VALUE:
            php_rc = exec_php_value(session, dir);
            break;
        case DIR_PHP_FLAG:
            php_rc = exec_php_flag(session, dir);
            break;
        case DIR_PHP_ADMIN_VALUE:
            php_rc = exec_php_admin_value(session, dir);
            break;
        case DIR_PHP_ADMIN_FLAG:
            php_rc = exec_php_admin_flag(session, dir);
            break;
        default:
            continue;
        }
        if (php_rc == LSI_OK)
            log_directive_ok(session, dir, type_name);
        else
            log_directive_fail(session, dir, type_name, "PHP config error");
    }

    /* (d) Environment variables */
    for (dir = directives; dir != NULL; dir = dir->next) {
        int env_rc = LSI_OK;
        const char *type_name = directive_type_str(dir->type);
        switch (dir->type) {
        case DIR_SETENV:
            env_rc = exec_setenv(session, dir);
            break;
        case DIR_SETENVIF:
            env_rc = exec_setenvif(session, dir);
            break;
        case DIR_BROWSER_MATCH:
            env_rc = exec_browser_match(session, dir);
            break;
        default:
            continue;
        }
        if (env_rc == LSI_OK)
            log_directive_ok(session, dir, type_name);
        else
            log_directive_fail(session, dir, type_name, "env var error");
    }

    /* (e) Brute force protection */
    int ip_len = 0;
    const char *client_ip = lsi_session_get_client_ip(session, &ip_len);
    if (client_ip && ip_len > 0) {
        rc = exec_brute_force(session, directives, client_ip);
        if (rc == LSI_ERROR) {
            lsi_log(session, LSI_LOG_DEBUG,
                    "mod_htaccess: request blocked by brute force protection");
        }
    }

    htaccess_directives_free(directives);
    return LSI_OK;
}

/* ------------------------------------------------------------------ */
/*  Response-phase hook callback (19.3)                                */
/* ------------------------------------------------------------------ */

/**
 * on_send_resp_header — called at LSI_HKPT_SEND_RESP_HEADER.
 *
 * Flow:
 * 1. Get doc_root and URI, build target directory
 * 2. Call DirWalker to get merged directives
 * 3. Extract filename from URI for FilesMatch
 * 4. Execute response-phase directives:
 *    a. Header / RequestHeader directives
 *    b. FilesMatch conditional blocks
 *    c. Expires directives
 *    d. ErrorDocument directives
 * 5. Free directives and return
 */
static int on_send_resp_header(lsi_session_t *session)
{
    int doc_root_len = 0;
    const char *doc_root = lsi_session_get_doc_root(session, &doc_root_len);
    if (!doc_root || doc_root_len <= 0)
        return LSI_OK;

    int uri_len = 0;
    const char *uri = lsi_session_get_uri(session, &uri_len);
    if (!uri || uri_len <= 0)
        return LSI_OK;

    /* Build target directory */
    char *target_dir = build_target_dir(doc_root, doc_root_len, uri, uri_len);
    if (!target_dir)
        return LSI_OK;

    /* Collect merged directives */
    htaccess_directive_t *directives = htaccess_dirwalk(session, doc_root,
                                                         target_dir);
    free(target_dir);

    if (!directives)
        return LSI_OK;

    /* Extract filename for FilesMatch */
    const char *filename = extract_filename(uri, uri_len);

    /* (a) Header / RequestHeader directives */
    const htaccess_directive_t *dir;
    for (dir = directives; dir != NULL; dir = dir->next) {
        int hdr_rc = LSI_OK;
        const char *type_name = directive_type_str(dir->type);
        switch (dir->type) {
        case DIR_HEADER_SET:
        case DIR_HEADER_UNSET:
        case DIR_HEADER_APPEND:
        case DIR_HEADER_MERGE:
        case DIR_HEADER_ADD:
            hdr_rc = exec_header(session, dir);
            break;
        case DIR_REQUEST_HEADER_SET:
        case DIR_REQUEST_HEADER_UNSET:
            hdr_rc = exec_request_header(session, dir);
            break;
        default:
            continue;
        }
        if (hdr_rc == LSI_OK)
            log_directive_ok(session, dir, type_name);
        else
            log_directive_fail(session, dir, type_name, "header error");
    }

    /* (b) FilesMatch conditional blocks */
    for (dir = directives; dir != NULL; dir = dir->next) {
        if (dir->type != DIR_FILES_MATCH)
            continue;
        int fm_rc = exec_files_match(session, dir, filename);
        if (fm_rc == LSI_OK)
            log_directive_ok(session, dir, "FilesMatch");
        else
            log_directive_fail(session, dir, "FilesMatch",
                               "pattern match error");
    }

    /* (c) Expires directives — pass Content-Type from response headers */
    const char *content_type = lsi_session_get_resp_header_by_name(
        session, "Content-Type", 12, NULL);
    if (!content_type)
        content_type = "application/octet-stream";
    exec_expires(session, directives, content_type);

    /* (d) ErrorDocument directives */
    for (dir = directives; dir != NULL; dir = dir->next) {
        if (dir->type != DIR_ERROR_DOCUMENT)
            continue;
        int ed_rc = exec_error_document(session, dir);
        if (ed_rc == 0)
            log_directive_ok(session, dir, "ErrorDocument");
        else if (ed_rc < 0)
            log_directive_fail(session, dir, "ErrorDocument",
                               "error document processing failed");
    }

    htaccess_directives_free(directives);
    return LSI_OK;
}
