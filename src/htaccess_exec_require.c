/**
 * htaccess_exec_require.c - Apache 2.4 Require access control executor
 *
 * Evaluates Require directives: all granted/denied, ip, not ip.
 * Supports RequireAny (OR) and RequireAll (AND) container blocks.
 * When Require coexists with Order/Allow/Deny, Require takes precedence.
 *
 * Validates: Requirements 8.1-8.8
 */
#include "htaccess_exec_require.h"
#include "htaccess_cidr.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>

/**
 * Check if client IP matches a space-separated list of CIDR ranges.
 * Returns 1 if any CIDR matches, 0 otherwise.
 */
static int ip_in_cidr_list(uint32_t client_ip, const char *cidr_list)
{
    if (!cidr_list)
        return 0;

    /* Work on a copy since we tokenize */
    char *buf = strdup(cidr_list);
    if (!buf)
        return 0;

    int matched = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(buf, " \t", &saveptr);
    while (tok) {
        cidr_v4_t cidr;
        if (cidr_parse(tok, &cidr) == 0) {
            if (cidr_match(&cidr, client_ip)) {
                matched = 1;
                break;
            }
        }
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    free(buf);
    return matched;
}

/**
 * Evaluate a single Require directive against a client IP.
 * Returns: 1 = grant, 0 = deny, -1 = not applicable (skip)
 */
static int eval_single_require(const htaccess_directive_t *dir, uint32_t client_ip)
{
    switch (dir->type) {
    case DIR_REQUIRE_ALL_GRANTED:
        return 1;
    case DIR_REQUIRE_ALL_DENIED:
        return 0;
    case DIR_REQUIRE_IP:
        return ip_in_cidr_list(client_ip, dir->value) ? 1 : 0;
    case DIR_REQUIRE_NOT_IP:
        return ip_in_cidr_list(client_ip, dir->value) ? 0 : 1;
    default:
        return -1; /* Not a Require directive */
    }
}

/* Forward declarations for mutual recursion */
static int eval_require_any(const htaccess_directive_t *container, uint32_t client_ip);
static int eval_require_all(const htaccess_directive_t *container, uint32_t client_ip);

/**
 * Evaluate a RequireAny container (OR logic).
 * Access granted if at least one child grants.
 * Returns: 1 = grant, 0 = deny
 */
static int eval_require_any(const htaccess_directive_t *container, uint32_t client_ip)
{
    const htaccess_directive_t *child;
    for (child = container->data.require_container.children; child; child = child->next) {
        /* Handle nested containers */
        if (child->type == DIR_REQUIRE_ANY_OPEN) {
            if (eval_require_any(child, client_ip))
                return 1;
            continue;
        }
        if (child->type == DIR_REQUIRE_ALL_OPEN) {
            if (eval_require_all(child, client_ip))
                return 1;
            continue;
        }
        int r = eval_single_require(child, client_ip);
        if (r == 1)
            return 1;
    }
    return 0;
}

/**
 * Evaluate a RequireAll container (AND logic).
 * Access granted only if all children grant.
 * Returns: 1 = grant, 0 = deny
 */
static int eval_require_all(const htaccess_directive_t *container, uint32_t client_ip)
{
    const htaccess_directive_t *child;
    for (child = container->data.require_container.children; child; child = child->next) {
        if (child->type == DIR_REQUIRE_ANY_OPEN) {
            if (!eval_require_any(child, client_ip))
                return 0;
            continue;
        }
        if (child->type == DIR_REQUIRE_ALL_OPEN) {
            if (!eval_require_all(child, client_ip))
                return 0;
            continue;
        }
        int r = eval_single_require(child, client_ip);
        if (r == 0)
            return 0;
    }
    return 1;
}

int exec_require(lsi_session_t *session,
                 const htaccess_directive_t *directives,
                 const char *client_ip)
{
    if (!session || !directives)
        return LSI_OK;

    /* Check if any Require directives exist */
    int has_require = 0;
    const htaccess_directive_t *dir;
    for (dir = directives; dir; dir = dir->next) {
        switch (dir->type) {
        case DIR_REQUIRE_ALL_GRANTED:
        case DIR_REQUIRE_ALL_DENIED:
        case DIR_REQUIRE_IP:
        case DIR_REQUIRE_NOT_IP:
        case DIR_REQUIRE_ANY_OPEN:
        case DIR_REQUIRE_ALL_OPEN:
            has_require = 1;
            break;
        default:
            break;
        }
        if (has_require)
            break;
    }

    if (!has_require)
        return LSI_OK;

    /* If Require coexists with Order/Allow/Deny, log warning */
    for (dir = directives; dir; dir = dir->next) {
        if (dir->type == DIR_ORDER || dir->type == DIR_ALLOW_FROM ||
            dir->type == DIR_DENY_FROM) {
            lsi_log(session, LSI_LOG_WARN,
                    "[htaccess] Require and Order/Allow/Deny coexist; "
                    "Require takes precedence");
            break;
        }
    }

    /* Parse client IP */
    uint32_t ip_val = 0;
    if (!client_ip || ip_parse(client_ip, &ip_val) != 0) {
        /* Cannot parse IP — deny for safety */
        lsi_session_set_status(session, 403);
        return LSI_ERROR;
    }

    /* Evaluate Require directives — implicit RequireAny (OR) at top level */
    int granted = 0;
    for (dir = directives; dir; dir = dir->next) {
        if (dir->type == DIR_REQUIRE_ANY_OPEN) {
            if (eval_require_any(dir, ip_val)) {
                granted = 1;
                break;
            }
            continue;
        }
        if (dir->type == DIR_REQUIRE_ALL_OPEN) {
            if (eval_require_all(dir, ip_val)) {
                granted = 1;
                break;
            }
            continue;
        }
        int r = eval_single_require(dir, ip_val);
        if (r == 1) {
            granted = 1;
            break;
        }
    }

    if (!granted) {
        lsi_session_set_status(session, 403);
        return LSI_ERROR;
    }

    return LSI_OK;
}
