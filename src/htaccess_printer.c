/**
 * htaccess_printer.c - .htaccess directive printer implementation
 *
 * Formats a linked list of htaccess_directive_t nodes into canonical
 * .htaccess text. Output is designed to round-trip through htaccess_parse().
 *
 * Validates: Requirements 2.5
 */
#include "htaccess_printer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Dynamic string buffer                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} strbuf_t;

static int strbuf_init(strbuf_t *sb, size_t initial_cap)
{
    sb->buf = (char *)malloc(initial_cap);
    if (!sb->buf)
        return -1;
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return 0;
}

static int strbuf_ensure(strbuf_t *sb, size_t extra)
{
    size_t needed = sb->len + extra + 1;
    if (needed <= sb->cap)
        return 0;
    size_t new_cap = sb->cap * 2;
    if (new_cap < needed)
        new_cap = needed;
    char *tmp = (char *)realloc(sb->buf, new_cap);
    if (!tmp)
        return -1;
    sb->buf = tmp;
    sb->cap = new_cap;
    return 0;
}

static int strbuf_append(strbuf_t *sb, const char *s)
{
    size_t slen = strlen(s);
    if (strbuf_ensure(sb, slen) != 0)
        return -1;
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Per-directive printers                                             */
/* ------------------------------------------------------------------ */

/**
 * Print a single directive into the string buffer.
 * Returns 0 on success, -1 on allocation failure.
 */
static int print_directive(strbuf_t *sb, const htaccess_directive_t *d)
{
    char tmp[64];

    switch (d->type) {

    /* --- Header directives --- */
    case DIR_HEADER_SET:
        if (strbuf_append(sb, "Header set ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_UNSET:
        if (strbuf_append(sb, "Header unset ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        break;

    case DIR_HEADER_APPEND:
        if (strbuf_append(sb, "Header append ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_MERGE:
        if (strbuf_append(sb, "Header merge ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_ADD:
        if (strbuf_append(sb, "Header add ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- RequestHeader directives --- */
    case DIR_REQUEST_HEADER_SET:
        if (strbuf_append(sb, "RequestHeader set ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_REQUEST_HEADER_UNSET:
        if (strbuf_append(sb, "RequestHeader unset ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        break;

    /* --- PHP directives --- */
    case DIR_PHP_VALUE:
        if (strbuf_append(sb, "php_value ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_PHP_FLAG:
        if (strbuf_append(sb, "php_flag ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_PHP_ADMIN_VALUE:
        if (strbuf_append(sb, "php_admin_value ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_PHP_ADMIN_FLAG:
        if (strbuf_append(sb, "php_admin_flag ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- Access control directives --- */
    case DIR_ORDER:
        if (d->data.acl.order == ORDER_ALLOW_DENY) {
            if (strbuf_append(sb, "Order Allow,Deny") != 0) return -1;
        } else {
            if (strbuf_append(sb, "Order Deny,Allow") != 0) return -1;
        }
        break;

    case DIR_ALLOW_FROM:
        if (strbuf_append(sb, "Allow from ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_DENY_FROM:
        if (strbuf_append(sb, "Deny from ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- Redirect directives --- */
    case DIR_REDIRECT:
        if (strbuf_append(sb, "Redirect") != 0) return -1;
        if (d->data.redirect.status_code != 302) {
            snprintf(tmp, sizeof(tmp), " %d", d->data.redirect.status_code);
            if (strbuf_append(sb, tmp) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_REDIRECT_MATCH:
        if (strbuf_append(sb, "RedirectMatch") != 0) return -1;
        if (d->data.redirect.status_code != 302) {
            snprintf(tmp, sizeof(tmp), " %d", d->data.redirect.status_code);
            if (strbuf_append(sb, tmp) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (d->data.redirect.pattern) {
            if (strbuf_append(sb, d->data.redirect.pattern) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- ErrorDocument --- */
    case DIR_ERROR_DOCUMENT:
        snprintf(tmp, sizeof(tmp), "ErrorDocument %d ", d->data.error_doc.error_code);
        if (strbuf_append(sb, tmp) != 0) return -1;
        if (d->value && d->value[0] == '"') {
            /* Value already contains quotes (text message mode) — output directly */
            if (strbuf_append(sb, d->value) != 0) return -1;
        } else {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    /* --- FilesMatch block --- */
    case DIR_FILES_MATCH:
        if (strbuf_append(sb, "<FilesMatch \"") != 0) return -1;
        if (d->data.files_match.pattern) {
            if (strbuf_append(sb, d->data.files_match.pattern) != 0) return -1;
        }
        if (strbuf_append(sb, "\">\n") != 0) return -1;
        /* Print nested children */
        for (const htaccess_directive_t *child = d->data.files_match.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</FilesMatch>") != 0) return -1;
        break;

    /* --- IfModule block --- */
    case DIR_IFMODULE:
        if (strbuf_append(sb, "<IfModule ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (strbuf_append(sb, ">\n") != 0) return -1;
        /* Print nested children */
        for (const htaccess_directive_t *child = d->data.ifmodule.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</IfModule>") != 0) return -1;
        break;

    /* --- Header always directives --- */
    case DIR_HEADER_ALWAYS_SET:
        if (strbuf_append(sb, "Header always set ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_ALWAYS_UNSET:
        if (strbuf_append(sb, "Header always unset ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        break;

    case DIR_HEADER_ALWAYS_APPEND:
        if (strbuf_append(sb, "Header always append ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_ALWAYS_MERGE:
        if (strbuf_append(sb, "Header always merge ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_HEADER_ALWAYS_ADD:
        if (strbuf_append(sb, "Header always add ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- Options directive --- */
    case DIR_OPTIONS:
        if (strbuf_append(sb, "Options ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    /* --- Files block --- */
    case DIR_FILES:
        if (strbuf_append(sb, "<Files ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (strbuf_append(sb, ">\n") != 0) return -1;
        /* Print nested children */
        for (const htaccess_directive_t *child = d->data.files.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</Files>") != 0) return -1;
        break;

    /* --- Expires directives --- */
    case DIR_EXPIRES_ACTIVE:
        if (strbuf_append(sb, "ExpiresActive ") != 0) return -1;
        if (strbuf_append(sb, d->data.expires.active ? "On" : "Off") != 0) return -1;
        break;

    case DIR_EXPIRES_BY_TYPE:
        if (strbuf_append(sb, "ExpiresByType ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " \"") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        if (strbuf_append(sb, "\"") != 0) return -1;
        break;

    case DIR_EXPIRES_DEFAULT:
        if (strbuf_append(sb, "ExpiresDefault \"") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        if (strbuf_append(sb, "\"") != 0) return -1;
        break;

    /* --- Require directives --- */
    case DIR_REQUIRE_ALL_GRANTED:
        if (strbuf_append(sb, "Require all granted") != 0) return -1;
        break;

    case DIR_REQUIRE_ALL_DENIED:
        if (strbuf_append(sb, "Require all denied") != 0) return -1;
        break;

    case DIR_REQUIRE_IP:
        if (strbuf_append(sb, "Require ip ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_REQUIRE_NOT_IP:
        if (strbuf_append(sb, "Require not ip ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_REQUIRE_VALID_USER:
        if (strbuf_append(sb, "Require valid-user") != 0) return -1;
        break;

    /* --- Auth directives --- */
    case DIR_AUTH_TYPE:
        if (strbuf_append(sb, "AuthType ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_AUTH_NAME:
        if (strbuf_append(sb, "AuthName \"") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        if (strbuf_append(sb, "\"") != 0) return -1;
        break;

    case DIR_AUTH_USER_FILE:
        if (strbuf_append(sb, "AuthUserFile ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_REQUIRE_ANY_OPEN:
        if (strbuf_append(sb, "<RequireAny>\n") != 0) return -1;
        for (const htaccess_directive_t *child = d->data.require_container.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</RequireAny>") != 0) return -1;
        break;

    case DIR_REQUIRE_ALL_OPEN:
        if (strbuf_append(sb, "<RequireAll>\n") != 0) return -1;
        for (const htaccess_directive_t *child = d->data.require_container.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</RequireAll>") != 0) return -1;
        break;

    /* --- Limit / LimitExcept blocks --- */
    case DIR_LIMIT:
        if (strbuf_append(sb, "<Limit ") != 0) return -1;
        if (d->data.limit.methods) {
            if (strbuf_append(sb, d->data.limit.methods) != 0) return -1;
        }
        if (strbuf_append(sb, ">\n") != 0) return -1;
        for (const htaccess_directive_t *child = d->data.limit.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</Limit>") != 0) return -1;
        break;

    case DIR_LIMIT_EXCEPT:
        if (strbuf_append(sb, "<LimitExcept ") != 0) return -1;
        if (d->data.limit.methods) {
            if (strbuf_append(sb, d->data.limit.methods) != 0) return -1;
        }
        if (strbuf_append(sb, ">\n") != 0) return -1;
        for (const htaccess_directive_t *child = d->data.limit.children;
             child; child = child->next) {
            if (print_directive(sb, child) != 0) return -1;
            if (strbuf_append(sb, "\n") != 0) return -1;
        }
        if (strbuf_append(sb, "</LimitExcept>") != 0) return -1;
        break;

    /* --- Environment variable directives --- */
    case DIR_SETENV:
        if (strbuf_append(sb, "SetEnv ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_SETENVIF:
        if (strbuf_append(sb, "SetEnvIf ") != 0) return -1;
        if (d->data.envif.attribute) {
            if (strbuf_append(sb, d->data.envif.attribute) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (d->data.envif.pattern) {
            if (strbuf_append(sb, d->data.envif.pattern) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, "=") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    case DIR_BROWSER_MATCH:
        if (strbuf_append(sb, "BrowserMatch ") != 0) return -1;
        if (d->data.envif.pattern) {
            if (strbuf_append(sb, d->data.envif.pattern) != 0) return -1;
        }
        if (strbuf_append(sb, " ") != 0) return -1;
        if (strbuf_append(sb, d->name) != 0) return -1;
        if (strbuf_append(sb, "=") != 0) return -1;
        if (strbuf_append(sb, d->value) != 0) return -1;
        break;

    /* --- Brute force protection directives --- */
    case DIR_BRUTE_FORCE_PROTECTION:
        if (strbuf_append(sb, "BruteForceProtection ") != 0) return -1;
        if (strbuf_append(sb, d->data.brute_force.enabled ? "On" : "Off") != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS:
        snprintf(tmp, sizeof(tmp), "BruteForceAllowedAttempts %d",
                 d->data.brute_force.allowed_attempts);
        if (strbuf_append(sb, tmp) != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_WINDOW:
        snprintf(tmp, sizeof(tmp), "BruteForceWindow %d",
                 d->data.brute_force.window_sec);
        if (strbuf_append(sb, tmp) != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_ACTION:
        if (strbuf_append(sb, "BruteForceAction ") != 0) return -1;
        if (strbuf_append(sb, d->data.brute_force.action == BF_ACTION_BLOCK
                                  ? "block" : "throttle") != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_THROTTLE_DURATION:
        snprintf(tmp, sizeof(tmp), "BruteForceThrottleDuration %d",
                 d->data.brute_force.throttle_ms);
        if (strbuf_append(sb, tmp) != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_X_FORWARDED_FOR:
        if (strbuf_append(sb, "BruteForceXForwardedFor ") != 0) return -1;
        if (strbuf_append(sb, d->data.brute_force.enabled ? "On" : "Off") != 0) return -1;
        break;

    case DIR_BRUTE_FORCE_WHITELIST:
        if (strbuf_append(sb, "BruteForceWhitelist ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_BRUTE_FORCE_PROTECT_PATH:
        if (strbuf_append(sb, "BruteForceProtectPath ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    /* --- Handler/Type directives --- */
    case DIR_ADD_HANDLER:
        if (strbuf_append(sb, "AddHandler ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (d->value) {
            if (strbuf_append(sb, " ") != 0) return -1;
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_SET_HANDLER:
        if (strbuf_append(sb, "SetHandler ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_ADD_TYPE:
        if (strbuf_append(sb, "AddType ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (d->value) {
            if (strbuf_append(sb, " ") != 0) return -1;
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_DIRECTORY_INDEX:
        if (strbuf_append(sb, "DirectoryIndex ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_FORCE_TYPE:
        if (strbuf_append(sb, "ForceType ") != 0) return -1;
        if (d->value) {
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_ADD_ENCODING:
        if (strbuf_append(sb, "AddEncoding ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (d->value) {
            if (strbuf_append(sb, " ") != 0) return -1;
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    case DIR_ADD_CHARSET:
        if (strbuf_append(sb, "AddCharset ") != 0) return -1;
        if (d->name) {
            if (strbuf_append(sb, d->name) != 0) return -1;
        }
        if (d->value) {
            if (strbuf_append(sb, " ") != 0) return -1;
            if (strbuf_append(sb, d->value) != 0) return -1;
        }
        break;

    default:
        /* Unknown directive type — skip silently */
        break;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

char *htaccess_print(const htaccess_directive_t *head)
{
    if (!head)
        return NULL;

    strbuf_t sb;
    if (strbuf_init(&sb, 256) != 0)
        return NULL;

    int first = 1;
    for (const htaccess_directive_t *d = head; d; d = d->next) {
        if (!first) {
            if (strbuf_append(&sb, "\n") != 0) {
                free(sb.buf);
                return NULL;
            }
        }
        first = 0;

        if (print_directive(&sb, d) != 0) {
            free(sb.buf);
            return NULL;
        }
    }

    /* Append trailing newline */
    if (strbuf_append(&sb, "\n") != 0) {
        free(sb.buf);
        return NULL;
    }

    return sb.buf;
}
