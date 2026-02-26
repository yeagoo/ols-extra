/**
 * htaccess_exec_error_doc.c - ErrorDocument directive executor
 *
 * Supports three ErrorDocument modes:
 * 1. External URL (http:// or https://) → 302 redirect
 * 2. Quoted text message → response body
 * 3. Local file path → stub (log intent; fall back to OLS default if missing)
 *
 * Validates: Requirements 8.1, 8.2, 8.3, 8.4
 */
#include "htaccess_exec_error_doc.h"

#include <string.h>

/**
 * Check if a string starts with a given prefix.
 */
static int starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int exec_error_document(lsi_session_t *session, const htaccess_directive_t *dir)
{
    int current_status;
    const char *value;
    int value_len;

    if (!session || !dir)
        return -1;

    if (dir->type != DIR_ERROR_DOCUMENT)
        return -1;

    if (!dir->value)
        return -1;

    /* Check if current response status matches the error code */
    current_status = lsi_session_get_status(session);
    if (current_status != dir->data.error_doc.error_code)
        return 0; /* No match — not our error code */

    value = dir->value;
    value_len = (int)strlen(value);

    /* Mode 1: External URL → 302 redirect */
    if (starts_with(value, "http://") || starts_with(value, "https://")) {
        lsi_session_set_status(session, 302);
        lsi_session_set_resp_header(session, "Location", 8,
                                    value, value_len);
        return 0;
    }

    /* Mode 2: Quoted text message → response body */
    if (value[0] == '"') {
        const char *text_start = value + 1;
        int text_len = value_len - 1;

        /* Strip trailing quote if present */
        if (text_len > 0 && text_start[text_len - 1] == '"')
            text_len--;

        lsi_session_set_resp_body(session, text_start, text_len);
        return 0;
    }

    /* Mode 3: Local file path (starts with /) */
    if (value[0] == '/') {
        /*
         * In a real module, we would check if the file exists and serve it.
         * For now, log that we would serve it. Since we can't access real
         * files in tests, this is a stub that logs a warning about the
         * local file path and falls back to OLS default error page.
         */
        lsi_log(session, LSI_LOG_WARN,
                "ErrorDocument: local file '%s' handling is a stub; "
                "falling back to OLS default error page", value);
        return 0;
    }

    /* Unknown format — log warning */
    lsi_log(session, LSI_LOG_WARN,
            "ErrorDocument: unrecognized value format '%s'", value);
    return 0;
}
