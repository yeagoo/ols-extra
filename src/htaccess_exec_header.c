/**
 * htaccess_exec_header.c - Header and RequestHeader directive executors
 *
 * Implements execution of Header (set/unset/append/merge/add) and
 * RequestHeader (set/unset) directives via LSIAPI session calls.
 *
 * Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7
 */
#include "htaccess_exec_header.h"
#include <string.h>

/**
 * Check if a substring exists within a comma-separated header value.
 * Used by merge to ensure idempotency.
 *
 * @param haystack  The current header value (comma-separated).
 * @param hay_len   Length of haystack.
 * @param needle    The value to search for.
 * @param needle_len Length of needle.
 * @return 1 if needle is found as a token, 0 otherwise.
 */
static int value_exists_in_header(const char *haystack, int hay_len,
                                  const char *needle, int needle_len)
{
    if (!haystack || hay_len <= 0 || !needle || needle_len <= 0)
        return 0;

    const char *p = haystack;
    const char *end = haystack + hay_len;

    while (p < end) {
        /* Skip leading whitespace */
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;

        /* Find end of current token (next comma or end) */
        const char *tok_start = p;
        while (p < end && *p != ',')
            p++;

        /* Trim trailing whitespace from token */
        const char *tok_end = p;
        while (tok_end > tok_start && (*(tok_end - 1) == ' ' || *(tok_end - 1) == '\t'))
            tok_end--;

        int tok_len = (int)(tok_end - tok_start);
        if (tok_len == needle_len && memcmp(tok_start, needle, (size_t)needle_len) == 0)
            return 1;

        /* Skip comma */
        if (p < end)
            p++;
    }
    return 0;
}

int exec_header(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name)
        return LSI_ERROR;

    int name_len = (int)strlen(dir->name);
    int val_len = dir->value ? (int)strlen(dir->value) : 0;

    switch (dir->type) {
    case DIR_HEADER_SET:
    case DIR_HEADER_ALWAYS_SET:
        if (!dir->value)
            return LSI_ERROR;
        return lsi_session_set_resp_header(session, dir->name, name_len,
                                           dir->value, val_len);

    case DIR_HEADER_UNSET:
    case DIR_HEADER_ALWAYS_UNSET:
        return lsi_session_remove_resp_header(session, dir->name, name_len);

    case DIR_HEADER_APPEND:
    case DIR_HEADER_ALWAYS_APPEND:
        if (!dir->value)
            return LSI_ERROR;
        return lsi_session_append_resp_header(session, dir->name, name_len,
                                              dir->value, val_len);

    case DIR_HEADER_MERGE:
    case DIR_HEADER_ALWAYS_MERGE: {
        if (!dir->value)
            return LSI_ERROR;

        /* Check if value already exists in current header */
        int cur_len = 0;
        const char *cur_val = lsi_session_get_resp_header_by_name(
            session, dir->name, name_len, &cur_len);

        if (cur_val && cur_len > 0) {
            if (value_exists_in_header(cur_val, cur_len, dir->value, val_len))
                return LSI_OK; /* Already present, skip (idempotency) */
        }

        /* Value not present, append it */
        return lsi_session_append_resp_header(session, dir->name, name_len,
                                              dir->value, val_len);
    }

    case DIR_HEADER_ADD:
    case DIR_HEADER_ALWAYS_ADD:
        if (!dir->value)
            return LSI_ERROR;
        return lsi_session_add_resp_header(session, dir->name, name_len,
                                           dir->value, val_len);

    default:
        return LSI_ERROR;
    }
}

int exec_request_header(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name)
        return LSI_ERROR;

    int name_len = (int)strlen(dir->name);

    switch (dir->type) {
    case DIR_REQUEST_HEADER_SET: {
        if (!dir->value)
            return LSI_ERROR;
        int val_len = (int)strlen(dir->value);
        return lsi_session_set_req_header(session, dir->name, name_len,
                                          dir->value, val_len);
    }

    case DIR_REQUEST_HEADER_UNSET:
        return lsi_session_remove_req_header(session, dir->name, name_len);

    default:
        return LSI_ERROR;
    }
}
