/**
 * gen_htaccess.h - .htaccess content string generator for RapidCheck
 *
 * Generates random valid .htaccess file content strings that can be
 * parsed by htaccess_parse(). Each generated string contains 1-N
 * directive lines covering all supported directive types.
 *
 * Validates: Requirements 2.6
 */
#ifndef GEN_HTACCESS_H
#define GEN_HTACCESS_H

#include <rapidcheck.h>
#include <string>
#include <vector>
#include <utility>

#include "gen_header.h"
#include "gen_cidr.h"
#include "gen_regex.h"
#include "gen_expires.h"

extern "C" {
#include "htaccess_directive.h"
}

namespace gen {

/**
 * Tagged directive line: the text line and its expected directive_type_t.
 */
using TaggedLine = std::pair<std::string, directive_type_t>;

/**
 * Generate a single random .htaccess directive line paired with its
 * expected directive type. Covers all parseable directive types.
 */
inline rc::Gen<TaggedLine> taggedDirectiveLine()
{
    return rc::gen::oneOf(
        /* Header set <name> <value> */
        rc::gen::map(
            rc::gen::pair(headerName(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"Header set " + p.first + " " + p.second,
                        DIR_HEADER_SET};
            }),
        /* Header unset <name> */
        rc::gen::map(headerName(),
            [](const std::string &n) -> TaggedLine {
                return {"Header unset " + n, DIR_HEADER_UNSET};
            }),
        /* Header append <name> <value> */
        rc::gen::map(
            rc::gen::pair(headerName(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"Header append " + p.first + " " + p.second,
                        DIR_HEADER_APPEND};
            }),
        /* Header merge <name> <value> */
        rc::gen::map(
            rc::gen::pair(headerName(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"Header merge " + p.first + " " + p.second,
                        DIR_HEADER_MERGE};
            }),
        /* Header add <name> <value> */
        rc::gen::map(
            rc::gen::pair(headerName(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"Header add " + p.first + " " + p.second,
                        DIR_HEADER_ADD};
            }),
        /* RequestHeader set <name> <value> */
        rc::gen::map(
            rc::gen::pair(headerName(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"RequestHeader set " + p.first + " " + p.second,
                        DIR_REQUEST_HEADER_SET};
            }),
        /* RequestHeader unset <name> */
        rc::gen::map(headerName(),
            [](const std::string &n) -> TaggedLine {
                return {"RequestHeader unset " + n,
                        DIR_REQUEST_HEADER_UNSET};
            }),
        /* php_value <name> <value> */
        rc::gen::map(
            rc::gen::pair(alphaIdent(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"php_value " + p.first + " " + p.second,
                        DIR_PHP_VALUE};
            }),
        /* php_flag <name> on|off */
        rc::gen::map(
            rc::gen::pair(alphaIdent(), rc::gen::arbitrary<bool>()),
            [](const std::pair<std::string, bool> &p) -> TaggedLine {
                return {"php_flag " + p.first + " " +
                        (p.second ? "on" : "off"),
                        DIR_PHP_FLAG};
            }),
        /* php_admin_value <name> <value> */
        rc::gen::map(
            rc::gen::pair(alphaIdent(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"php_admin_value " + p.first + " " + p.second,
                        DIR_PHP_ADMIN_VALUE};
            }),
        /* php_admin_flag <name> on|off */
        rc::gen::map(
            rc::gen::pair(alphaIdent(), rc::gen::arbitrary<bool>()),
            [](const std::pair<std::string, bool> &p) -> TaggedLine {
                return {"php_admin_flag " + p.first + " " +
                        (p.second ? "on" : "off"),
                        DIR_PHP_ADMIN_FLAG};
            }),
        /* Order Allow,Deny / Deny,Allow */
        rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool ad) -> TaggedLine {
                return {ad ? "Order Allow,Deny" : "Order Deny,Allow",
                        DIR_ORDER};
            }),
        /* Allow from <cidr|all> */
        rc::gen::map(cidrOrAll(),
            [](const std::string &v) -> TaggedLine {
                return {"Allow from " + v, DIR_ALLOW_FROM};
            }),
        /* Deny from <cidr|all> */
        rc::gen::map(cidrOrAll(),
            [](const std::string &v) -> TaggedLine {
                return {"Deny from " + v, DIR_DENY_FROM};
            }),
        /* SetEnv <name> <value> */
        rc::gen::map(
            rc::gen::pair(alphaIdent(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) -> TaggedLine {
                return {"SetEnv " + p.first + " " + p.second, DIR_SETENV};
            }),
        /* ExpiresActive On/Off */
        rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool on) -> TaggedLine {
                return {on ? "ExpiresActive On" : "ExpiresActive Off",
                        DIR_EXPIRES_ACTIVE};
            }),
        /* BruteForceProtection On/Off */
        rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool on) -> TaggedLine {
                return {on ? "BruteForceProtection On"
                           : "BruteForceProtection Off",
                        DIR_BRUTE_FORCE_PROTECTION};
            }),
        /* BruteForceAllowedAttempts <N> */
        rc::gen::map(
            rc::gen::inRange(1, 100),
            [](int n) -> TaggedLine {
                return {"BruteForceAllowedAttempts " + std::to_string(n),
                        DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS};
            }),
        /* BruteForceWindow <N> */
        rc::gen::map(
            rc::gen::inRange(1, 3600),
            [](int n) -> TaggedLine {
                return {"BruteForceWindow " + std::to_string(n),
                        DIR_BRUTE_FORCE_WINDOW};
            }),
        /* BruteForceAction block|throttle */
        rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool block) -> TaggedLine {
                return {block ? "BruteForceAction block"
                              : "BruteForceAction throttle",
                        DIR_BRUTE_FORCE_ACTION};
            })
    );
}

/**
 * Generate a random .htaccess file content string with 1-maxLines
 * directive lines. Returns the content string.
 */
inline rc::Gen<std::string> htaccessContent(int maxLines = 10)
{
    return rc::gen::mapcat(
        rc::gen::inRange(1, maxLines + 1),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<TaggedLine>>(
                    (std::size_t)count,
                    taggedDirectiveLine()),
                [](const std::vector<TaggedLine> &lines) {
                    std::string content;
                    for (const auto &tl : lines)
                        content += tl.first + "\n";
                    return content;
                });
        });
}

/**
 * Generate tagged .htaccess content: the content string plus a vector
 * of expected directive types in order.
 */
using TaggedContent = std::pair<std::string, std::vector<directive_type_t>>;

inline rc::Gen<TaggedContent> taggedHtaccessContent(int maxLines = 10)
{
    return rc::gen::mapcat(
        rc::gen::inRange(1, maxLines + 1),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<TaggedLine>>(
                    (std::size_t)count,
                    taggedDirectiveLine()),
                [](const std::vector<TaggedLine> &lines) -> TaggedContent {
                    std::string content;
                    std::vector<directive_type_t> types;
                    for (const auto &tl : lines) {
                        content += tl.first + "\n";
                        types.push_back(tl.second);
                    }
                    return {content, types};
                });
        });
}

} /* namespace gen */

#endif /* GEN_HTACCESS_H */
