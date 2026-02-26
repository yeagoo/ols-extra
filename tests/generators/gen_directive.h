/**
 * gen_directive.h - Directive generator for RapidCheck
 *
 * Generates random htaccess_directive_t* covering all 28 directive types.
 * Each generated directive is heap-allocated and must be freed by the caller
 * via htaccess_directives_free().
 *
 * Validates: Requirements 2.6, 13.1
 */
#ifndef GEN_DIRECTIVE_H
#define GEN_DIRECTIVE_H

#include <rapidcheck.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "htaccess_directive.h"
}

#include "gen_header.h"
#include "gen_cidr.h"
#include "gen_regex.h"
#include "gen_expires.h"

namespace gen {

/**
 * Helper: allocate a zeroed directive with given type and line number.
 */
inline htaccess_directive_t *allocDir(directive_type_t type, int line = 1)
{
    auto *d = (htaccess_directive_t *)calloc(1, sizeof(htaccess_directive_t));
    if (d) {
        d->type = type;
        d->line_number = line;
    }
    return d;
}

/**
 * Generate a random directive_type_t covering all 28 enum values.
 */
inline rc::Gen<directive_type_t> directiveType()
{
    return rc::gen::element(
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
        DIR_EXPIRES_BY_TYPE
    );
}

/**
 * Generate remaining directive types (split due to rc::gen::element arity).
 */
inline rc::Gen<directive_type_t> directiveTypeExtra()
{
    return rc::gen::element(
        DIR_SETENV,
        DIR_SETENVIF,
        DIR_BROWSER_MATCH,
        DIR_BRUTE_FORCE_PROTECTION,
        DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS,
        DIR_BRUTE_FORCE_WINDOW,
        DIR_BRUTE_FORCE_ACTION,
        DIR_BRUTE_FORCE_THROTTLE_DURATION
    );
}

/**
 * Generate any of the 28 directive types.
 */
inline rc::Gen<directive_type_t> anyDirectiveType()
{
    return rc::gen::oneOf(directiveType(), directiveTypeExtra());
}

/**
 * Generate a random htaccess_directive_t* for a specific type.
 * The returned pointer is heap-allocated; caller must free.
 * Does NOT generate FilesMatch children (use directiveWithChildren for that).
 */
inline rc::Gen<htaccess_directive_t *> directiveOfType(directive_type_t type)
{
    switch (type) {

    case DIR_HEADER_SET:
    case DIR_HEADER_APPEND:
    case DIR_HEADER_MERGE:
    case DIR_HEADER_ADD:
        return rc::gen::map(
            rc::gen::pair(headerName(), headerValue()),
            [type](const std::pair<std::string, std::string> &p) {
                auto *d = allocDir(type);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.c_str());
                return d;
            });

    case DIR_HEADER_UNSET:
        return rc::gen::map(headerName(),
            [](const std::string &n) {
                auto *d = allocDir(DIR_HEADER_UNSET);
                d->name = strdup(n.c_str());
                return d;
            });

    case DIR_REQUEST_HEADER_SET:
        return rc::gen::map(
            rc::gen::pair(headerName(), headerValue()),
            [](const std::pair<std::string, std::string> &p) {
                auto *d = allocDir(DIR_REQUEST_HEADER_SET);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.c_str());
                return d;
            });

    case DIR_REQUEST_HEADER_UNSET:
        return rc::gen::map(headerName(),
            [](const std::string &n) {
                auto *d = allocDir(DIR_REQUEST_HEADER_UNSET);
                d->name = strdup(n.c_str());
                return d;
            });

    case DIR_PHP_VALUE:
    case DIR_PHP_ADMIN_VALUE:
        return rc::gen::map(
            rc::gen::pair(alphaIdent(), simpleValue()),
            [type](const std::pair<std::string, std::string> &p) {
                auto *d = allocDir(type);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.c_str());
                return d;
            });

    case DIR_PHP_FLAG:
    case DIR_PHP_ADMIN_FLAG:
        return rc::gen::map(
            rc::gen::pair(alphaIdent(), rc::gen::arbitrary<bool>()),
            [type](const std::pair<std::string, bool> &p) {
                auto *d = allocDir(type);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second ? "on" : "off");
                return d;
            });

    case DIR_ORDER:
        return rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool allowDeny) {
                auto *d = allocDir(DIR_ORDER);
                d->data.acl.order = allowDeny ? ORDER_ALLOW_DENY
                                              : ORDER_DENY_ALLOW;
                return d;
            });

    case DIR_ALLOW_FROM:
    case DIR_DENY_FROM:
        return rc::gen::map(cidrOrAll(),
            [type](const std::string &v) {
                auto *d = allocDir(type);
                d->value = strdup(v.c_str());
                return d;
            });

    case DIR_REDIRECT:
        return rc::gen::map(
            rc::gen::tuple(
                rc::gen::element(301, 302, 303, 307, 308),
                simpleValue(),
                simpleValue()),
            [](const std::tuple<int, std::string, std::string> &t) {
                auto *d = allocDir(DIR_REDIRECT);
                d->data.redirect.status_code = std::get<0>(t);
                d->data.redirect.pattern = nullptr;
                d->name = strdup(("/" + std::get<1>(t)).c_str());
                d->value = strdup(("https://example.com/" + std::get<2>(t)).c_str());
                return d;
            });

    case DIR_REDIRECT_MATCH:
        return rc::gen::map(
            rc::gen::tuple(
                rc::gen::element(301, 302, 303, 307, 308),
                simpleRegex(),
                simpleValue()),
            [](const std::tuple<int, std::string, std::string> &t) {
                auto *d = allocDir(DIR_REDIRECT_MATCH);
                d->data.redirect.status_code = std::get<0>(t);
                d->data.redirect.pattern = strdup(std::get<1>(t).c_str());
                d->value = strdup(("https://example.com/" + std::get<2>(t)).c_str());
                return d;
            });

    case DIR_ERROR_DOCUMENT:
        return rc::gen::map(
            rc::gen::pair(
                rc::gen::element(400, 401, 403, 404, 500, 502, 503),
                simpleValue()),
            [](const std::pair<int, std::string> &p) {
                auto *d = allocDir(DIR_ERROR_DOCUMENT);
                d->data.error_doc.error_code = p.first;
                d->value = strdup(("/errors/" + p.second + ".html").c_str());
                return d;
            });

    case DIR_FILES_MATCH:
        return rc::gen::map(fileMatchRegex(),
            [](const std::string &pattern) {
                auto *d = allocDir(DIR_FILES_MATCH);
                d->data.files_match.pattern = strdup(pattern.c_str());
                d->data.files_match.children = nullptr;
                return d;
            });

    case DIR_EXPIRES_ACTIVE:
        return rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool on) {
                auto *d = allocDir(DIR_EXPIRES_ACTIVE);
                d->data.expires.active = on ? 1 : 0;
                return d;
            });

    case DIR_EXPIRES_BY_TYPE: {
        auto mimeTypes = std::vector<std::string>{
            "text/html", "text/css", "image/png", "image/jpeg",
            "application/javascript", "application/json"};
        return rc::gen::map(
            rc::gen::pair(
                rc::gen::elementOf(mimeTypes),
                expiresDuration()),
            [](const std::pair<std::string, ExpiresResult> &p) {
                auto *d = allocDir(DIR_EXPIRES_BY_TYPE);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.first.c_str());
                d->data.expires.duration_sec = p.second.second;
                return d;
            });
    }

    case DIR_SETENV:
        return rc::gen::map(
            rc::gen::pair(alphaIdent(), simpleValue()),
            [](const std::pair<std::string, std::string> &p) {
                auto *d = allocDir(DIR_SETENV);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.c_str());
                return d;
            });

    case DIR_SETENVIF: {
        auto attrs = std::vector<std::string>{
            "Remote_Addr", "Request_URI", "User-Agent"};
        return rc::gen::map(
            rc::gen::tuple(
                rc::gen::elementOf(attrs),
                simpleRegex(),
                alphaIdent(),
                simpleValue()),
            [](const std::tuple<std::string, std::string,
                                std::string, std::string> &t) {
                auto *d = allocDir(DIR_SETENVIF);
                d->data.envif.attribute = strdup(std::get<0>(t).c_str());
                d->data.envif.pattern = strdup(std::get<1>(t).c_str());
                d->name = strdup(std::get<2>(t).c_str());
                d->value = strdup(std::get<3>(t).c_str());
                return d;
            });
    }

    case DIR_BROWSER_MATCH:
        return rc::gen::map(
            rc::gen::tuple(simpleRegex(), alphaIdent(), simpleValue()),
            [](const std::tuple<std::string, std::string, std::string> &t) {
                auto *d = allocDir(DIR_BROWSER_MATCH);
                d->data.envif.attribute = strdup("User-Agent");
                d->data.envif.pattern = strdup(std::get<0>(t).c_str());
                d->name = strdup(std::get<1>(t).c_str());
                d->value = strdup(std::get<2>(t).c_str());
                return d;
            });

    case DIR_BRUTE_FORCE_PROTECTION:
        return rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool on) {
                auto *d = allocDir(DIR_BRUTE_FORCE_PROTECTION);
                d->data.brute_force.enabled = on ? 1 : 0;
                return d;
            });

    case DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS:
        return rc::gen::map(
            rc::gen::inRange(1, 100),
            [](int n) {
                auto *d = allocDir(DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS);
                d->data.brute_force.allowed_attempts = n;
                return d;
            });

    case DIR_BRUTE_FORCE_WINDOW:
        return rc::gen::map(
            rc::gen::inRange(1, 3600),
            [](int n) {
                auto *d = allocDir(DIR_BRUTE_FORCE_WINDOW);
                d->data.brute_force.window_sec = n;
                return d;
            });

    case DIR_BRUTE_FORCE_ACTION:
        return rc::gen::map(
            rc::gen::arbitrary<bool>(),
            [](bool block) {
                auto *d = allocDir(DIR_BRUTE_FORCE_ACTION);
                d->data.brute_force.action = block ? BF_ACTION_BLOCK
                                                   : BF_ACTION_THROTTLE;
                return d;
            });

    case DIR_BRUTE_FORCE_THROTTLE_DURATION:
        return rc::gen::map(
            rc::gen::inRange(100, 10000),
            [](int ms) {
                auto *d = allocDir(DIR_BRUTE_FORCE_THROTTLE_DURATION);
                d->data.brute_force.throttle_ms = ms;
                return d;
            });

    default:
        /* Fallback: generate a simple Header set directive */
        return rc::gen::map(
            rc::gen::pair(headerName(), headerValue()),
            [](const std::pair<std::string, std::string> &p) {
                auto *d = allocDir(DIR_HEADER_SET);
                d->name = strdup(p.first.c_str());
                d->value = strdup(p.second.c_str());
                return d;
            });
    }
}

/**
 * Generate a random directive of any non-FilesMatch type.
 */
inline rc::Gen<htaccess_directive_t *> simpleDirective()
{
    return rc::gen::mapcat(
        anyDirectiveType(),
        [](directive_type_t type) {
            /* Skip FilesMatch to avoid recursion in simple contexts */
            if (type == DIR_FILES_MATCH)
                return directiveOfType(DIR_HEADER_SET);
            return directiveOfType(type);
        });
}

/**
 * Generate a linked list of 1-N random directives.
 * Caller must free with htaccess_directives_free().
 */
inline rc::Gen<htaccess_directive_t *> directiveList(int maxCount = 8)
{
    return rc::gen::mapcat(
        rc::gen::inRange(1, maxCount + 1),
        [](int count) {
            return rc::gen::map(
                rc::gen::container<std::vector<htaccess_directive_t *>>(
                    (std::size_t)count,
                    simpleDirective()),
                [](const std::vector<htaccess_directive_t *> &dirs) {
                    /* Link into a list */
                    for (std::size_t i = 0; i + 1 < dirs.size(); i++)
                        dirs[i]->next = dirs[i + 1];
                    if (!dirs.empty())
                        dirs.back()->next = nullptr;
                    return dirs.empty() ? nullptr : dirs.front();
                });
        });
}

} /* namespace gen */

#endif /* GEN_DIRECTIVE_H */
