/**
 * htaccess_exec_redirect.c - Redirect and RedirectMatch directive executors
 *
 * Implements Redirect (prefix match) and RedirectMatch (regex + $N
 * backreference substitution). Both functions return 1 on redirect
 * (short-circuit signal), 0 on no match, -1 on error.
 *
 * Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5
 */
#include "htaccess_exec_redirect.h"

#include <string.h>
#include <regex.h>

/** Maximum number of regex capture groups supported. */
#define MAX_CAPTURES 10

/** Maximum length of the substituted Location URL. */
#define MAX_URL_LEN 4096

/**
 * Substitute $N backreferences in a template string with captured values.
 *
 * @param tmpl      Template string containing $1, $2, etc.
 * @param uri       The original URI that was matched.
 * @param matches   Array of regmatch_t from regexec().
 * @param nmatch    Number of valid entries in matches[].
 * @param out       Output buffer for the substituted string.
 * @param out_size  Size of the output buffer.
 * @return 0 on success, -1 if output would overflow.
 */
static int substitute_backrefs(const char *tmpl, const char *uri,
                               const regmatch_t *matches, size_t nmatch,
                               char *out, size_t out_size)
{
    size_t pos = 0;
    const char *p = tmpl;

    while (*p && pos < out_size - 1) {
        if (*p == '$' && p[1] >= '0' && p[1] <= '9') {
            int idx = p[1] - '0';
            p += 2;

            if ((size_t)idx < nmatch && matches[idx].rm_so >= 0) {
                size_t cap_len = (size_t)(matches[idx].rm_eo - matches[idx].rm_so);
                if (pos + cap_len >= out_size)
                    return -1;
                memcpy(out + pos, uri + matches[idx].rm_so, cap_len);
                pos += cap_len;
            }
            /* If index out of range, $N is simply removed */
        } else {
            out[pos++] = *p++;
        }
    }

    if (pos >= out_size)
        return -1;

    out[pos] = '\0';
    return 0;
}

int exec_redirect(lsi_session_t *session, const htaccess_directive_t *dir)
{
    int uri_len = 0;
    const char *uri;
    int status;
    int name_len;
    int val_len;

    if (!session || !dir)
        return -1;

    if (dir->type != DIR_REDIRECT)
        return -1;

    if (!dir->name || !dir->value)
        return -1;

    /* Get request URI */
    uri = lsi_session_get_uri(session, &uri_len);
    if (!uri || uri_len <= 0)
        return 0;

    name_len = (int)strlen(dir->name);

    /* Prefix match: URI must start with dir->name */
    if (uri_len < name_len)
        return 0;
    if (memcmp(uri, dir->name, (size_t)name_len) != 0)
        return 0;

    /* Match found — set status and Location */
    status = dir->data.redirect.status_code;
    if (status == 0)
        status = 302;

    lsi_session_set_status(session, status);

    val_len = (int)strlen(dir->value);
    lsi_session_set_resp_header(session, "Location", 8, dir->value, val_len);

    return 1; /* Short-circuit */
}

int exec_redirect_match(lsi_session_t *session, const htaccess_directive_t *dir)
{
    int uri_len = 0;
    const char *uri;
    const char *pattern;
    regex_t re;
    regmatch_t matches[MAX_CAPTURES];
    int status;
    char url_buf[MAX_URL_LEN];
    int url_len;
    int rc;

    if (!session || !dir)
        return -1;

    if (dir->type != DIR_REDIRECT_MATCH)
        return -1;

    if (!dir->value)
        return -1;

    pattern = dir->data.redirect.pattern;
    if (!pattern)
        return -1;

    /* Get request URI */
    uri = lsi_session_get_uri(session, &uri_len);
    if (!uri || uri_len <= 0)
        return 0;

    /* Compile regex */
    if (regcomp(&re, pattern, REG_EXTENDED) != 0)
        return -1; /* Invalid regex */

    /* Match against URI */
    rc = regexec(&re, uri, MAX_CAPTURES, matches, 0);
    if (rc != 0) {
        regfree(&re);
        return 0; /* No match */
    }

    /* Substitute $N backreferences in the target URL template */
    if (substitute_backrefs(dir->value, uri, matches, MAX_CAPTURES,
                            url_buf, sizeof(url_buf)) != 0) {
        regfree(&re);
        return -1; /* URL too long */
    }

    regfree(&re);

    /* Set status and Location */
    status = dir->data.redirect.status_code;
    if (status == 0)
        status = 302;

    lsi_session_set_status(session, status);

    url_len = (int)strlen(url_buf);
    lsi_session_set_resp_header(session, "Location", 8, url_buf, url_len);

    return 1; /* Short-circuit */
}
