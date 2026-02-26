/**
 * htaccess_exec_files_match.c - FilesMatch directive executor
 *
 * Compiles the FilesMatch regex pattern and matches it against the filename.
 * If matched, executes nested directives in original order by dispatching
 * to the appropriate executor (e.g., exec_header for Header directives).
 *
 * Validates: Requirements 9.1, 9.2, 9.3
 */
#include "htaccess_exec_files_match.h"
#include "htaccess_exec_header.h"

#include <regex.h>
#include <string.h>

/**
 * Dispatch and execute a single nested directive by type.
 *
 * @param session  LSIAPI session handle.
 * @param child    The nested directive to execute.
 * @return LSI_OK on success, LSI_ERROR on failure.
 */
static int dispatch_child(lsi_session_t *session,
                          const htaccess_directive_t *child)
{
    switch (child->type) {
    case DIR_HEADER_SET:
    case DIR_HEADER_UNSET:
    case DIR_HEADER_APPEND:
    case DIR_HEADER_MERGE:
    case DIR_HEADER_ADD:
        return exec_header(session, child);

    case DIR_REQUEST_HEADER_SET:
    case DIR_REQUEST_HEADER_UNSET:
        return exec_request_header(session, child);

    default:
        /* Unsupported nested directive type — skip with warning */
        lsi_log(session, LSI_LOG_WARN,
                "FilesMatch: unsupported nested directive type %d at line %d",
                (int)child->type, child->line_number);
        return LSI_OK;
    }
}

int exec_files_match(lsi_session_t *session, const htaccess_directive_t *dir,
                     const char *filename)
{
    regex_t re;
    int rc;
    const htaccess_directive_t *child;
    const char *pattern;

    if (!session || !dir || !filename)
        return LSI_ERROR;

    if (dir->type != DIR_FILES_MATCH)
        return LSI_ERROR;

    pattern = dir->data.files_match.pattern;
    if (!pattern)
        return LSI_ERROR;

    /* Compile the regex pattern (POSIX extended) */
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
        lsi_log(session, LSI_LOG_WARN,
                "FilesMatch: invalid regex pattern '%s' at line %d",
                pattern, dir->line_number);
        return LSI_ERROR;
    }

    /* Match filename against the pattern */
    rc = regexec(&re, filename, 0, NULL, 0);
    regfree(&re);

    if (rc != 0) {
        /* No match — skip all children */
        return LSI_OK;
    }

    /* Match found — execute nested directives in order */
    child = dir->data.files_match.children;
    while (child) {
        int ret = dispatch_child(session, child);
        if (ret != LSI_OK) {
            lsi_log(session, LSI_LOG_WARN,
                    "FilesMatch: nested directive at line %d failed",
                    child->line_number);
            /* Continue with remaining children (graceful degradation) */
        }
        child = child->next;
    }

    return LSI_OK;
}
