/**
 * htaccess_directive.h - Directive data model for OLS .htaccess module
 *
 * Defines the directive_type_t enum (28 directive types), supporting enums
 * (acl_order_t, bf_action_t), and the htaccess_directive_t linked-list node
 * structure with a union for type-specific fields.
 *
 * Validates: Requirements 2.2, 4.1-4.7, 5.1-5.4, 6.1-6.4, 7.1-7.4,
 *            8.1-8.3, 9.1, 10.1-10.4, 11.1-11.6, 12.1-12.8
 */
#ifndef HTACCESS_DIRECTIVE_H
#define HTACCESS_DIRECTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Directive type enumeration — covers all 28 supported .htaccess directives.
 */
typedef enum {
    DIR_HEADER_SET,
    DIR_HEADER_UNSET,
    DIR_HEADER_APPEND,
    DIR_HEADER_MERGE,
    DIR_HEADER_ADD,
    DIR_REQUEST_HEADER_SET,
    DIR_REQUEST_HEADER_UNSET,
    DIR_PHP_VALUE,
    DIR_PHP_FLAG,
    DIR_PHP_ADMIN_VALUE,
    DIR_PHP_ADMIN_FLAG,
    DIR_ORDER,
    DIR_ALLOW_FROM,
    DIR_DENY_FROM,
    DIR_REDIRECT,
    DIR_REDIRECT_MATCH,
    DIR_ERROR_DOCUMENT,
    DIR_FILES_MATCH,
    DIR_EXPIRES_ACTIVE,
    DIR_EXPIRES_BY_TYPE,
    DIR_SETENV,
    DIR_SETENVIF,
    DIR_BROWSER_MATCH,
    DIR_BRUTE_FORCE_PROTECTION,
    DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS,
    DIR_BRUTE_FORCE_WINDOW,
    DIR_BRUTE_FORCE_ACTION,
    DIR_BRUTE_FORCE_THROTTLE_DURATION,
} directive_type_t;

/**
 * Access control order — determines default policy and evaluation order.
 */
typedef enum {
    ORDER_ALLOW_DENY,   /* Default deny, evaluate Allow then Deny */
    ORDER_DENY_ALLOW,   /* Default allow, evaluate Deny then Allow */
} acl_order_t;

/**
 * Brute force protection action type.
 */
typedef enum {
    BF_ACTION_BLOCK,    /* Return 403 Forbidden */
    BF_ACTION_THROTTLE, /* Delay response */
} bf_action_t;

/**
 * Directive structure — linked-list node.
 *
 * Each node represents a single parsed .htaccess directive. The `name` and
 * `value` fields carry the generic key/value pair used by most directives.
 * The `data` union holds type-specific fields that vary by directive type.
 */
typedef struct htaccess_directive {
    directive_type_t type;
    int line_number;              /* Source file line number (for logging) */

    /* Generic key-value pair (used by most directives) */
    char *name;                   /* Directive/header/variable/MIME type name */
    char *value;                  /* Directive/header/variable value */

    /* Type-specific fields */
    union {
        struct {
            acl_order_t order;
        } acl;

        struct {
            int   status_code;    /* HTTP status code (301, 302, etc.) */
            char *pattern;        /* RedirectMatch regex pattern */
        } redirect;

        struct {
            int error_code;       /* HTTP error code (403, 404, 500, etc.) */
        } error_doc;

        struct {
            char *pattern;        /* FilesMatch regex pattern */
            struct htaccess_directive *children; /* Nested directive list */
        } files_match;

        struct {
            int  active;          /* 1=On, 0=Off */
            long duration_sec;    /* Expiration duration in seconds */
        } expires;

        struct {
            char *attribute;      /* SetEnvIf attribute name */
            char *pattern;        /* SetEnvIf regex pattern */
        } envif;

        struct {
            int        enabled;          /* BruteForceProtection On/Off */
            int        allowed_attempts; /* Max allowed attempts */
            int        window_sec;       /* Time window in seconds */
            bf_action_t action;          /* block or throttle */
            int        throttle_ms;      /* Throttle delay in milliseconds */
        } brute_force;
    } data;

    struct htaccess_directive *next; /* Next node in linked list */
} htaccess_directive_t;

/**
 * Free an entire directive linked list.
 *
 * Walks the list and frees all dynamically allocated memory including
 * name, value, type-specific strings, and nested children (for FilesMatch).
 *
 * @param head  Head of the directive linked list (may be NULL).
 */
void htaccess_directives_free(htaccess_directive_t *head);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HTACCESS_DIRECTIVE_H */
