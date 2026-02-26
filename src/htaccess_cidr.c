/**
 * htaccess_cidr.c - CIDR parsing and matching implementation
 *
 * Validates: Requirements 6.3, 6.4, 6.5
 */
#include "htaccess_cidr.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * Internal: parse four dotted-decimal octets from `str` into a uint32_t
 * in host byte order.  On success sets *out and returns 0; on any format
 * error returns -1.
 *
 * `end` (if not NULL) is set to point to the first character after the
 * last octet that was consumed.
 */
static int parse_ipv4(const char *str, uint32_t *out, const char **end)
{
    uint32_t addr = 0;
    int i;

    for (i = 0; i < 4; i++) {
        const char *p = str;
        unsigned long octet;
        char *ep;

        if (i > 0) {
            if (*p != '.')
                return -1;
            p++;
        }

        /* Reject leading zeros (e.g. "01") to avoid ambiguity */
        if (*p == '0' && isdigit((unsigned char)p[1]))
            return -1;

        octet = strtoul(p, &ep, 10);
        if (ep == p || octet > 255)
            return -1;

        addr = (addr << 8) | (uint32_t)octet;
        str = ep;
    }

    *out = addr;
    if (end)
        *end = str;
    return 0;
}

/**
 * Build a subnet mask for a given prefix length (0-32).
 * E.g. prefix=24 → 0xFFFFFF00
 */
static uint32_t prefix_to_mask(int prefix)
{
    if (prefix == 0)
        return 0;
    return ~((uint32_t)0) << (32 - prefix);
}

/* ------------------------------------------------------------------ */

int cidr_parse(const char *cidr_str, cidr_v4_t *out)
{
    const char *p;
    uint32_t ip;
    int prefix;

    if (!cidr_str || !out)
        return -1;

    /* Skip leading whitespace */
    while (*cidr_str && isspace((unsigned char)*cidr_str))
        cidr_str++;

    /* Handle "all" keyword (case-insensitive) */
    if (strncasecmp(cidr_str, "all", 3) == 0) {
        p = cidr_str + 3;
        /* Only trailing whitespace allowed */
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p != '\0')
            return -1;
        out->network = 0;
        out->mask    = 0;
        return 0;
    }

    /* Parse the IP portion */
    if (parse_ipv4(cidr_str, &ip, &p) != 0)
        return -1;

    if (*p == '/') {
        /* CIDR notation: parse prefix length */
        char *ep;
        unsigned long val;

        p++;  /* skip '/' */
        val = strtoul(p, &ep, 10);
        if (ep == p || val > 32)
            return -1;
        prefix = (int)val;
        p = ep;
    } else {
        /* Plain IP — treat as /32 */
        prefix = 32;
    }

    /* Only trailing whitespace allowed */
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        return -1;

    out->mask    = prefix_to_mask(prefix);
    out->network = ip & out->mask;
    return 0;
}

int cidr_match(const cidr_v4_t *cidr, uint32_t ip)
{
    if (!cidr)
        return 0;
    /* mask==0 (from "all") matches everything */
    return (ip & cidr->mask) == (cidr->network & cidr->mask) ? 1 : 0;
}

int ip_parse(const char *ip_str, uint32_t *out_ip)
{
    const char *end;

    if (!ip_str || !out_ip)
        return -1;

    /* Skip leading whitespace */
    while (*ip_str && isspace((unsigned char)*ip_str))
        ip_str++;

    if (parse_ipv4(ip_str, out_ip, &end) != 0)
        return -1;

    /* Only trailing whitespace allowed */
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return -1;

    return 0;
}
