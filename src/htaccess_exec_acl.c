/**
 * htaccess_exec_acl.c - Access control directive executor implementation
 *
 * Implements Apache-compatible Order/Allow/Deny access control evaluation
 * with CIDR matching and "all" keyword support.
 *
 * Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5, 6.6
 */
#include "htaccess_exec_acl.h"
#include "htaccess_cidr.h"

#include <string.h>
#include <strings.h>

/**
 * Check if a client IP matches a single Allow/Deny rule value.
 *
 * The rule value can be:
 *   - "all"           → matches any IP
 *   - "A.B.C.D"       → matches a single IP (/32)
 *   - "A.B.C.D/N"     → matches a CIDR range
 *
 * @param rule_value  The Allow/Deny rule string (e.g. "192.168.1.0/24").
 * @param client_ip   Client IP as uint32_t in host byte order.
 * @return 1 if the IP matches the rule, 0 otherwise.
 */
static int ip_matches_rule(const char *rule_value, uint32_t client_ip)
{
    cidr_v4_t cidr;

    if (!rule_value)
        return 0;

    /* Handle "all" keyword explicitly */
    if (strcasecmp(rule_value, "all") == 0)
        return 1;

    /* Parse CIDR/IP and match */
    if (cidr_parse(rule_value, &cidr) != 0)
        return 0; /* Invalid rule — skip */

    return cidr_match(&cidr, client_ip);
}

int exec_access_control(lsi_session_t *session,
                        const htaccess_directive_t *directives)
{
    const htaccess_directive_t *dir;
    acl_order_t order = ORDER_ALLOW_DENY; /* default */
    int have_order = 0;
    int allow_matched = 0;
    int deny_matched = 0;
    uint32_t client_ip;
    int ip_len = 0;
    const char *ip_str;

    if (!session || !directives)
        return LSI_OK;

    /* Find the Order directive (use the last one found) */
    for (dir = directives; dir; dir = dir->next) {
        if (dir->type == DIR_ORDER) {
            order = dir->data.acl.order;
            have_order = 1;
        }
    }

    /* If no Order directive and no Allow/Deny rules, nothing to do */
    if (!have_order) {
        int has_acl_rules = 0;
        for (dir = directives; dir; dir = dir->next) {
            if (dir->type == DIR_ALLOW_FROM || dir->type == DIR_DENY_FROM) {
                has_acl_rules = 1;
                break;
            }
        }
        if (!has_acl_rules)
            return LSI_OK;
    }

    /* Get client IP */
    ip_str = lsi_session_get_client_ip(session, &ip_len);
    if (!ip_str || ip_len <= 0)
        return LSI_OK; /* No client IP available, allow by default */

    if (ip_parse(ip_str, &client_ip) != 0)
        return LSI_OK; /* Cannot parse client IP, allow by default */

    /* Check all Allow and Deny rules */
    for (dir = directives; dir; dir = dir->next) {
        if (dir->type == DIR_ALLOW_FROM && dir->value) {
            if (ip_matches_rule(dir->value, client_ip))
                allow_matched = 1;
        } else if (dir->type == DIR_DENY_FROM && dir->value) {
            if (ip_matches_rule(dir->value, client_ip))
                deny_matched = 1;
        }
    }

    /* Evaluate according to Apache ACL semantics */
    int denied = 0;

    if (order == ORDER_ALLOW_DENY) {
        /*
         * Order Allow,Deny: Default is DENY.
         * Allow rules evaluated first, then Deny rules override.
         * Access is allowed only if Allow matches AND Deny does not match.
         */
        if (allow_matched && !deny_matched)
            denied = 0;
        else
            denied = 1;
    } else {
        /*
         * Order Deny,Allow: Default is ALLOW.
         * Deny rules evaluated first, then Allow rules override.
         * Access is denied only if Deny matches AND Allow does not match.
         */
        if (deny_matched && !allow_matched)
            denied = 1;
        else
            denied = 0;
    }

    if (denied) {
        lsi_session_set_status(session, 403);
        return LSI_ERROR;
    }

    return LSI_OK;
}
