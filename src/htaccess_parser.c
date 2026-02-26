/**
 * htaccess_parser.c - .htaccess file parser implementation
 *
 * Parses .htaccess content line-by-line into a linked list of
 * htaccess_directive_t nodes. Supports all 28 directive types and
 * nested FilesMatch blocks.
 *
 * Validates: Requirements 2.1, 2.2, 2.3, 9.1
 */
#include "htaccess_parser.h"
#include "htaccess_expires.h"
#include "ls.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/** Skip leading whitespace. */
static const char *skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return p;
}

/** Skip trailing whitespace in-place and return trimmed length. */
static size_t trimmed_len(const char *s, size_t len)
{
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        len--;
    return len;
}

/**
 * Case-insensitive match of a keyword at position p.
 * Returns pointer past keyword if matched (followed by space or NUL),
 * or NULL on mismatch.
 */
static const char *match_kw(const char *p, const char *kw)
{
    size_t n = strlen(kw);
    if (strncasecmp(p, kw, n) != 0)
        return NULL;
    if (p[n] != '\0' && !isspace((unsigned char)p[n]))
        return NULL;
    return p + n;
}

/**
 * Extract the next whitespace-delimited token from *pp.
 * Returns a strdup'd copy, advances *pp past the token.
 * Returns NULL if no token available.
 */
static char *next_token(const char **pp)
{
    const char *p = skip_ws(*pp);
    if (!*p)
        return NULL;

    const char *start = p;
    /* Handle quoted token */
    if (*p == '"') {
        start = ++p;
        while (*p && *p != '"')
            p++;
        char *tok = strndup(start, (size_t)(p - start));
        if (*p == '"')
            p++;
        *pp = p;
        return tok;
    }

    while (*p && !isspace((unsigned char)*p))
        p++;
    char *tok = strndup(start, (size_t)(p - start));
    *pp = p;
    return tok;
}

/**
 * Get the rest of the line (trimmed) as a strdup'd string.
 * Returns NULL if nothing remains.
 */
static char *rest_of_line(const char **pp)
{
    const char *p = skip_ws(*pp);
    if (!*p)
        return NULL;

    size_t len = strlen(p);
    len = trimmed_len(p, len);
    if (len == 0)
        return NULL;

    /* Handle quoted rest */
    if (*p == '"' && len >= 2 && p[len - 1] == '"') {
        char *s = strndup(p + 1, len - 2);
        *pp = p + len;
        return s;
    }

    char *s = strndup(p, len);
    *pp = p + len;
    return s;
}

/** Allocate a zeroed directive node. */
static htaccess_directive_t *alloc_directive(directive_type_t type, int line)
{
    htaccess_directive_t *d = (htaccess_directive_t *)calloc(1, sizeof(*d));
    if (d) {
        d->type = type;
        d->line_number = line;
    }
    return d;
}

/** Append directive to tail of list. */
static void append_directive(htaccess_directive_t **head,
                             htaccess_directive_t **tail,
                             htaccess_directive_t *node)
{
    if (!*head) {
        *head = node;
        *tail = node;
    } else {
        (*tail)->next = node;
        *tail = node;
    }
}

/* ------------------------------------------------------------------ */
/*  Individual directive parsers                                        */
/*  Each returns a directive node or NULL on parse failure.             */
/* ------------------------------------------------------------------ */

/**
 * Parse: Header set|unset|append|merge|add <name> [<value>]
 */
static htaccess_directive_t *parse_header(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *action = next_token(&p);
    if (!action)
        return NULL;

    directive_type_t type;
    int needs_value = 1;

    if (strcasecmp(action, "set") == 0)
        type = DIR_HEADER_SET;
    else if (strcasecmp(action, "unset") == 0) {
        type = DIR_HEADER_UNSET;
        needs_value = 0;
    } else if (strcasecmp(action, "append") == 0)
        type = DIR_HEADER_APPEND;
    else if (strcasecmp(action, "merge") == 0)
        type = DIR_HEADER_MERGE;
    else if (strcasecmp(action, "add") == 0)
        type = DIR_HEADER_ADD;
    else {
        free(action);
        return NULL;
    }
    free(action);

    char *name = next_token(&p);
    if (!name)
        return NULL;

    char *value = NULL;
    if (needs_value) {
        value = rest_of_line(&p);
        if (!value) {
            free(name);
            return NULL;
        }
    }

    htaccess_directive_t *d = alloc_directive(type, line);
    if (!d) {
        free(name);
        free(value);
        return NULL;
    }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: RequestHeader set|unset <name> [<value>]
 */
static htaccess_directive_t *parse_request_header(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *action = next_token(&p);
    if (!action)
        return NULL;

    directive_type_t type;
    int needs_value = 1;

    if (strcasecmp(action, "set") == 0)
        type = DIR_REQUEST_HEADER_SET;
    else if (strcasecmp(action, "unset") == 0) {
        type = DIR_REQUEST_HEADER_UNSET;
        needs_value = 0;
    } else {
        free(action);
        return NULL;
    }
    free(action);

    char *name = next_token(&p);
    if (!name)
        return NULL;

    char *value = NULL;
    if (needs_value) {
        value = rest_of_line(&p);
        if (!value) {
            free(name);
            return NULL;
        }
    }

    htaccess_directive_t *d = alloc_directive(type, line);
    if (!d) {
        free(name);
        free(value);
        return NULL;
    }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: php_value <name> <value>
 */
static htaccess_directive_t *parse_php_value(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *name = next_token(&p);
    if (!name)
        return NULL;
    char *value = rest_of_line(&p);
    if (!value) {
        free(name);
        return NULL;
    }
    htaccess_directive_t *d = alloc_directive(DIR_PHP_VALUE, line);
    if (!d) { free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: php_flag <name> <on|off>
 */
static htaccess_directive_t *parse_php_flag(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *name = next_token(&p);
    if (!name)
        return NULL;
    char *value = next_token(&p);
    if (!value) {
        free(name);
        return NULL;
    }
    if (strcasecmp(value, "on") != 0 && strcasecmp(value, "off") != 0) {
        free(name);
        free(value);
        return NULL;
    }
    htaccess_directive_t *d = alloc_directive(DIR_PHP_FLAG, line);
    if (!d) { free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: php_admin_value <name> <value>
 */
static htaccess_directive_t *parse_php_admin_value(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *name = next_token(&p);
    if (!name)
        return NULL;
    char *value = rest_of_line(&p);
    if (!value) {
        free(name);
        return NULL;
    }
    htaccess_directive_t *d = alloc_directive(DIR_PHP_ADMIN_VALUE, line);
    if (!d) { free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: php_admin_flag <name> <on|off>
 */
static htaccess_directive_t *parse_php_admin_flag(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *name = next_token(&p);
    if (!name)
        return NULL;
    char *value = next_token(&p);
    if (!value) {
        free(name);
        return NULL;
    }
    if (strcasecmp(value, "on") != 0 && strcasecmp(value, "off") != 0) {
        free(name);
        free(value);
        return NULL;
    }
    htaccess_directive_t *d = alloc_directive(DIR_PHP_ADMIN_FLAG, line);
    if (!d) { free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: Order Allow,Deny | Order Deny,Allow
 */
static htaccess_directive_t *parse_order(const char *args, int line)
{
    const char *p = skip_ws(args);
    htaccess_directive_t *d = alloc_directive(DIR_ORDER, line);
    if (!d)
        return NULL;

    if (strncasecmp(p, "Allow,Deny", 10) == 0 ||
        strncasecmp(p, "allow,deny", 10) == 0) {
        d->data.acl.order = ORDER_ALLOW_DENY;
    } else if (strncasecmp(p, "Deny,Allow", 10) == 0 ||
               strncasecmp(p, "deny,allow", 10) == 0) {
        d->data.acl.order = ORDER_DENY_ALLOW;
    } else {
        free(d);
        return NULL;
    }
    return d;
}

/**
 * Parse: Allow from <cidr|all>
 */
static htaccess_directive_t *parse_allow_from(const char *args, int line)
{
    const char *p = skip_ws(args);
    const char *after = match_kw(p, "from");
    if (!after)
        return NULL;
    p = skip_ws(after);
    char *value = next_token(&p);
    if (!value)
        return NULL;

    htaccess_directive_t *d = alloc_directive(DIR_ALLOW_FROM, line);
    if (!d) { free(value); return NULL; }
    d->value = value;
    return d;
}

/**
 * Parse: Deny from <cidr|all>
 */
static htaccess_directive_t *parse_deny_from(const char *args, int line)
{
    const char *p = skip_ws(args);
    const char *after = match_kw(p, "from");
    if (!after)
        return NULL;
    p = skip_ws(after);
    char *value = next_token(&p);
    if (!value)
        return NULL;

    htaccess_directive_t *d = alloc_directive(DIR_DENY_FROM, line);
    if (!d) { free(value); return NULL; }
    d->value = value;
    return d;
}

/**
 * Parse: Redirect [status] <path> <url>
 */
static htaccess_directive_t *parse_redirect(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *tok1 = next_token(&p);
    if (!tok1)
        return NULL;

    int status_code = 302; /* default */
    char *path = NULL;
    char *url = NULL;

    /* Check if first token is a status code */
    char *endp;
    long code = strtol(tok1, &endp, 10);
    if (*endp == '\0' && code >= 100 && code <= 599) {
        status_code = (int)code;
        path = next_token(&p);
        if (!path) {
            free(tok1);
            return NULL;
        }
        url = rest_of_line(&p);
        if (!url) {
            free(tok1);
            free(path);
            return NULL;
        }
        free(tok1);
    } else {
        /* First token is the path */
        path = tok1;
        url = rest_of_line(&p);
        if (!url) {
            free(path);
            return NULL;
        }
    }

    htaccess_directive_t *d = alloc_directive(DIR_REDIRECT, line);
    if (!d) { free(path); free(url); return NULL; }
    d->name = path;
    d->value = url;
    d->data.redirect.status_code = status_code;
    d->data.redirect.pattern = NULL;
    return d;
}

/**
 * Parse: RedirectMatch [status] <pattern> <url>
 */
static htaccess_directive_t *parse_redirect_match(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *tok1 = next_token(&p);
    if (!tok1)
        return NULL;

    int status_code = 302;
    char *pattern = NULL;
    char *url = NULL;

    char *endp;
    long code = strtol(tok1, &endp, 10);
    if (*endp == '\0' && code >= 100 && code <= 599) {
        status_code = (int)code;
        pattern = next_token(&p);
        if (!pattern) {
            free(tok1);
            return NULL;
        }
        url = rest_of_line(&p);
        if (!url) {
            free(tok1);
            free(pattern);
            return NULL;
        }
        free(tok1);
    } else {
        pattern = tok1;
        url = rest_of_line(&p);
        if (!url) {
            free(pattern);
            return NULL;
        }
    }

    htaccess_directive_t *d = alloc_directive(DIR_REDIRECT_MATCH, line);
    if (!d) { free(pattern); free(url); return NULL; }
    d->value = url;
    d->data.redirect.status_code = status_code;
    d->data.redirect.pattern = pattern;
    return d;
}

/**
 * Parse: ErrorDocument <code> <path|url|"message">
 */
static htaccess_directive_t *parse_error_document(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *code_str = next_token(&p);
    if (!code_str)
        return NULL;

    char *endp;
    long code = strtol(code_str, &endp, 10);
    if (*endp != '\0' || code < 100 || code > 599) {
        free(code_str);
        return NULL;
    }
    free(code_str);

    char *value = rest_of_line(&p);
    if (!value)
        return NULL;

    htaccess_directive_t *d = alloc_directive(DIR_ERROR_DOCUMENT, line);
    if (!d) { free(value); return NULL; }
    d->value = value;
    d->data.error_doc.error_code = (int)code;
    return d;
}

/**
 * Parse: ExpiresActive On|Off
 */
static htaccess_directive_t *parse_expires_active(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    int active;
    if (strcasecmp(val, "on") == 0)
        active = 1;
    else if (strcasecmp(val, "off") == 0)
        active = 0;
    else {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_EXPIRES_ACTIVE, line);
    if (!d)
        return NULL;
    d->data.expires.active = active;
    return d;
}

/**
 * Parse: ExpiresByType <mime-type> "access plus N unit"
 */
static htaccess_directive_t *parse_expires_by_type(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *mime = next_token(&p);
    if (!mime)
        return NULL;

    char *duration_str = rest_of_line(&p);
    if (!duration_str) {
        free(mime);
        return NULL;
    }

    long secs = parse_expires_duration(duration_str);
    if (secs < 0) {
        free(mime);
        free(duration_str);
        return NULL;
    }

    htaccess_directive_t *d = alloc_directive(DIR_EXPIRES_BY_TYPE, line);
    if (!d) { free(mime); free(duration_str); return NULL; }
    d->name = mime;
    d->value = duration_str;
    d->data.expires.duration_sec = secs;
    return d;
}

/**
 * Parse: SetEnv <name> <value>
 */
static htaccess_directive_t *parse_setenv(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *name = next_token(&p);
    if (!name)
        return NULL;
    char *value = rest_of_line(&p);
    if (!value) {
        free(name);
        return NULL;
    }
    htaccess_directive_t *d = alloc_directive(DIR_SETENV, line);
    if (!d) { free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    return d;
}

/**
 * Parse: SetEnvIf <attribute> <pattern> <name>=<value>
 */
static htaccess_directive_t *parse_setenvif(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *attribute = next_token(&p);
    if (!attribute)
        return NULL;
    char *pattern = next_token(&p);
    if (!pattern) {
        free(attribute);
        return NULL;
    }
    char *assignment = rest_of_line(&p);
    if (!assignment) {
        free(attribute);
        free(pattern);
        return NULL;
    }

    /* Split assignment on '=' */
    char *eq = strchr(assignment, '=');
    char *name = NULL;
    char *value = NULL;
    if (eq) {
        name = strndup(assignment, (size_t)(eq - assignment));
        value = strdup(eq + 1);
    } else {
        /* No '=', treat entire assignment as name with empty value */
        name = strdup(assignment);
        value = strdup("");
    }
    free(assignment);

    htaccess_directive_t *d = alloc_directive(DIR_SETENVIF, line);
    if (!d) { free(attribute); free(pattern); free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    d->data.envif.attribute = attribute;
    d->data.envif.pattern = pattern;
    return d;
}

/**
 * Parse: BrowserMatch <pattern> <name>=<value>
 */
static htaccess_directive_t *parse_browser_match(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *pattern = next_token(&p);
    if (!pattern)
        return NULL;
    char *assignment = rest_of_line(&p);
    if (!assignment) {
        free(pattern);
        return NULL;
    }

    char *eq = strchr(assignment, '=');
    char *name = NULL;
    char *value = NULL;
    if (eq) {
        name = strndup(assignment, (size_t)(eq - assignment));
        value = strdup(eq + 1);
    } else {
        name = strdup(assignment);
        value = strdup("");
    }
    free(assignment);

    htaccess_directive_t *d = alloc_directive(DIR_BROWSER_MATCH, line);
    if (!d) { free(pattern); free(name); free(value); return NULL; }
    d->name = name;
    d->value = value;
    d->data.envif.attribute = strdup("User-Agent");
    d->data.envif.pattern = pattern;
    return d;
}

/**
 * Parse: BruteForceProtection On|Off
 */
static htaccess_directive_t *parse_brute_force_protection(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    int enabled;
    if (strcasecmp(val, "on") == 0)
        enabled = 1;
    else if (strcasecmp(val, "off") == 0)
        enabled = 0;
    else {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_BRUTE_FORCE_PROTECTION, line);
    if (!d)
        return NULL;
    d->data.brute_force.enabled = enabled;
    return d;
}

/**
 * Parse: BruteForceAllowedAttempts <N>
 */
static htaccess_directive_t *parse_brute_force_attempts(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    char *endp;
    long n = strtol(val, &endp, 10);
    if (*endp != '\0' || n <= 0) {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS, line);
    if (!d)
        return NULL;
    d->data.brute_force.allowed_attempts = (int)n;
    return d;
}

/**
 * Parse: BruteForceWindow <N>
 */
static htaccess_directive_t *parse_brute_force_window(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    char *endp;
    long n = strtol(val, &endp, 10);
    if (*endp != '\0' || n <= 0) {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_BRUTE_FORCE_WINDOW, line);
    if (!d)
        return NULL;
    d->data.brute_force.window_sec = (int)n;
    return d;
}

/**
 * Parse: BruteForceAction block|throttle
 */
static htaccess_directive_t *parse_brute_force_action(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    bf_action_t action;
    if (strcasecmp(val, "block") == 0)
        action = BF_ACTION_BLOCK;
    else if (strcasecmp(val, "throttle") == 0)
        action = BF_ACTION_THROTTLE;
    else {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_BRUTE_FORCE_ACTION, line);
    if (!d)
        return NULL;
    d->data.brute_force.action = action;
    return d;
}

/**
 * Parse: BruteForceThrottleDuration <N>
 */
static htaccess_directive_t *parse_brute_force_throttle(const char *args, int line)
{
    const char *p = skip_ws(args);
    char *val = next_token(&p);
    if (!val)
        return NULL;

    char *endp;
    long n = strtol(val, &endp, 10);
    if (*endp != '\0' || n <= 0) {
        free(val);
        return NULL;
    }
    free(val);

    htaccess_directive_t *d = alloc_directive(DIR_BRUTE_FORCE_THROTTLE_DURATION, line);
    if (!d)
        return NULL;
    d->data.brute_force.throttle_ms = (int)n;
    return d;
}

/* ------------------------------------------------------------------ */
/*  Line dispatcher                                                    */
/* ------------------------------------------------------------------ */

/**
 * Try to parse a single non-empty, non-comment line into a directive.
 * Returns a directive node, or NULL if the line is not recognised.
 */
static htaccess_directive_t *parse_line(const char *line, int line_num)
{
    const char *p = skip_ws(line);
    const char *after;

    /* Header */
    after = match_kw(p, "Header");
    if (after)
        return parse_header(after, line_num);

    /* RequestHeader */
    after = match_kw(p, "RequestHeader");
    if (after)
        return parse_request_header(after, line_num);

    /* php_value */
    after = match_kw(p, "php_value");
    if (after)
        return parse_php_value(after, line_num);

    /* php_flag */
    after = match_kw(p, "php_flag");
    if (after)
        return parse_php_flag(after, line_num);

    /* php_admin_value */
    after = match_kw(p, "php_admin_value");
    if (after)
        return parse_php_admin_value(after, line_num);

    /* php_admin_flag */
    after = match_kw(p, "php_admin_flag");
    if (after)
        return parse_php_admin_flag(after, line_num);

    /* Order */
    after = match_kw(p, "Order");
    if (after)
        return parse_order(after, line_num);

    /* Allow */
    after = match_kw(p, "Allow");
    if (after)
        return parse_allow_from(after, line_num);

    /* Deny */
    after = match_kw(p, "Deny");
    if (after)
        return parse_deny_from(after, line_num);

    /* RedirectMatch (must check before Redirect) */
    after = match_kw(p, "RedirectMatch");
    if (after)
        return parse_redirect_match(after, line_num);

    /* Redirect */
    after = match_kw(p, "Redirect");
    if (after)
        return parse_redirect(after, line_num);

    /* ErrorDocument */
    after = match_kw(p, "ErrorDocument");
    if (after)
        return parse_error_document(after, line_num);

    /* ExpiresActive */
    after = match_kw(p, "ExpiresActive");
    if (after)
        return parse_expires_active(after, line_num);

    /* ExpiresByType */
    after = match_kw(p, "ExpiresByType");
    if (after)
        return parse_expires_by_type(after, line_num);

    /* SetEnvIf (must check before SetEnv) */
    after = match_kw(p, "SetEnvIf");
    if (after)
        return parse_setenvif(after, line_num);

    /* SetEnv */
    after = match_kw(p, "SetEnv");
    if (after)
        return parse_setenv(after, line_num);

    /* BrowserMatch */
    after = match_kw(p, "BrowserMatch");
    if (after)
        return parse_browser_match(after, line_num);

    /* BruteForceProtection */
    after = match_kw(p, "BruteForceProtection");
    if (after)
        return parse_brute_force_protection(after, line_num);

    /* BruteForceAllowedAttempts */
    after = match_kw(p, "BruteForceAllowedAttempts");
    if (after)
        return parse_brute_force_attempts(after, line_num);

    /* BruteForceWindow */
    after = match_kw(p, "BruteForceWindow");
    if (after)
        return parse_brute_force_window(after, line_num);

    /* BruteForceAction */
    after = match_kw(p, "BruteForceAction");
    if (after)
        return parse_brute_force_action(after, line_num);

    /* BruteForceThrottleDuration */
    after = match_kw(p, "BruteForceThrottleDuration");
    if (after)
        return parse_brute_force_throttle(after, line_num);

    return NULL; /* Unrecognised directive */
}

/* ------------------------------------------------------------------ */
/*  FilesMatch block detection helpers                                 */
/* ------------------------------------------------------------------ */

/**
 * Check if a line is a <FilesMatch "pattern"> opening tag.
 * If so, extracts the pattern into *out_pattern (caller must free).
 * Returns 1 if matched, 0 otherwise.
 */
static int is_files_match_open(const char *line, char **out_pattern)
{
    const char *p = skip_ws(line);
    if (*p != '<')
        return 0;
    p++;
    const char *after = match_kw(p, "FilesMatch");
    if (!after)
        return 0;
    p = skip_ws(after);

    /* Extract pattern (may be quoted) */
    if (*p == '"') {
        p++;
        const char *end = strchr(p, '"');
        if (!end)
            return 0;
        *out_pattern = strndup(p, (size_t)(end - p));
        /* Verify closing '>' follows */
        p = skip_ws(end + 1);
        if (*p != '>')
        {
            free(*out_pattern);
            *out_pattern = NULL;
            return 0;
        }
        return 1;
    }

    /* Unquoted pattern */
    const char *start = p;
    while (*p && *p != '>' && !isspace((unsigned char)*p))
        p++;
    if (p == start)
        return 0;
    *out_pattern = strndup(start, (size_t)(p - start));
    p = skip_ws(p);
    if (*p != '>') {
        free(*out_pattern);
        *out_pattern = NULL;
        return 0;
    }
    return 1;
}

/**
 * Check if a line is a </FilesMatch> closing tag.
 * Accepts both </FilesMatch> and </FilesMatch > (with optional space).
 */
static int is_files_match_close(const char *line)
{
    const char *p = skip_ws(line);
    if (*p != '<')
        return 0;
    p++;
    if (*p != '/')
        return 0;
    p++;

    /* Match "FilesMatch" keyword — accept '>' immediately after */
    size_t kw_len = strlen("FilesMatch");
    if (strncasecmp(p, "FilesMatch", kw_len) != 0)
        return 0;
    p += kw_len;
    p = skip_ws(p);
    return (*p == '>');
}

/* ------------------------------------------------------------------ */
/*  Main parser entry point                                            */
/* ------------------------------------------------------------------ */

htaccess_directive_t *htaccess_parse(const char *content, size_t len,
                                     const char *filepath)
{
    if (!content || len == 0)
        return NULL;

    /* Make a NUL-terminated copy for safe string operations */
    char *buf = (char *)malloc(len + 1);
    if (!buf)
        return NULL;
    memcpy(buf, content, len);
    buf[len] = '\0';

    htaccess_directive_t *head = NULL;
    htaccess_directive_t *tail = NULL;

    const char *fp = filepath ? filepath : "<unknown>";
    int line_num = 0;
    int in_files_match = 0;
    char *fm_pattern = NULL;
    int fm_start_line = 0;
    htaccess_directive_t *fm_children_head = NULL;
    htaccess_directive_t *fm_children_tail = NULL;

    /* Manual line splitting to correctly count empty lines */
    char *cur = buf;
    while (cur && *cur) {
        line_num++;

        /* Find end of current line */
        char *eol = strchr(cur, '\n');
        if (eol)
            *eol = '\0';

        /* Trim trailing \r (Windows line endings) and whitespace */
        {
            size_t llen = strlen(cur);
            while (llen > 0 && (cur[llen - 1] == '\r' ||
                                isspace((unsigned char)cur[llen - 1])))
                cur[--llen] = '\0';
        }

        /* Trim leading whitespace */
        const char *p = skip_ws(cur);

        /* Advance to next line */
        cur = eol ? eol + 1 : NULL;

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == '#')
            continue;

        /* Check for </FilesMatch> closing tag */
        if (in_files_match && is_files_match_close(p)) {
            htaccess_directive_t *fm = alloc_directive(DIR_FILES_MATCH, fm_start_line);
            if (fm) {
                fm->data.files_match.pattern = fm_pattern;
                fm->data.files_match.children = fm_children_head;
                append_directive(&head, &tail, fm);
            } else {
                free(fm_pattern);
                htaccess_directives_free(fm_children_head);
            }
            fm_pattern = NULL;
            fm_children_head = NULL;
            fm_children_tail = NULL;
            in_files_match = 0;
            continue;
        }

        /* Check for <FilesMatch "pattern"> opening tag */
        if (!in_files_match) {
            char *pattern = NULL;
            if (is_files_match_open(p, &pattern)) {
                in_files_match = 1;
                fm_pattern = pattern;
                fm_start_line = line_num;
                fm_children_head = NULL;
                fm_children_tail = NULL;
                continue;
            }
        }

        /* Parse the directive line */
        htaccess_directive_t *dir = parse_line(p, line_num);
        if (dir) {
            if (in_files_match) {
                append_directive(&fm_children_head, &fm_children_tail, dir);
            } else {
                append_directive(&head, &tail, dir);
            }
        } else {
            lsi_log(NULL, LSI_LOG_WARN,
                    "[htaccess] %s:%d: syntax error, skipping line: %s",
                    fp, line_num, p);
        }
    }

    /* Handle unclosed FilesMatch block */
    if (in_files_match) {
        lsi_log(NULL, LSI_LOG_WARN,
                "[htaccess] %s:%d: unclosed <FilesMatch> block, discarding",
                fp, fm_start_line);
        free(fm_pattern);
        htaccess_directives_free(fm_children_head);
    }

    free(buf);
    return head;
}
