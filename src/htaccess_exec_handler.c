#include "htaccess_exec_handler.h"
#include <string.h>
#include <stdlib.h>

/* Check if filename ends with the given extension (case-insensitive) */
static int has_extension(const char *filename, const char *ext)
{
    if (!filename || !ext)
        return 0;
    size_t flen = strlen(filename);
    size_t elen = strlen(ext);
    /* ext may or may not have leading dot */
    const char *e = ext;
    if (*e == '.')
        e++;
    elen = strlen(e);
    if (elen == 0 || flen <= elen)
        return 0;
    /* filename must have a dot before the extension */
    const char *dot = filename + flen - elen - 1;
    if (*dot != '.')
        return 0;
    return (strncasecmp(dot + 1, e, elen) == 0);
}

int exec_add_handler(lsi_session_t *session, const htaccess_directive_t *dir)
{
    (void)session;
    (void)dir;
    /* AddHandler is logged at DEBUG level; OLS handles handler mapping natively */
    return LSI_OK;
}

int exec_set_handler(lsi_session_t *session, const htaccess_directive_t *dir)
{
    (void)session;
    (void)dir;
    /* SetHandler is logged at DEBUG level; OLS handles handler mapping natively */
    return LSI_OK;
}

int exec_add_type(lsi_session_t *session, const htaccess_directive_t *dir,
                  const char *filename)
{
    if (!dir || !dir->name || !filename)
        return LSI_OK;

    /* dir->name = MIME type, dir->value = space-separated extension list */
    if (!dir->value)
        return LSI_OK;

    /* Check each extension in the list */
    char *exts = strdup(dir->value);
    if (!exts)
        return LSI_ERROR;

    char *saveptr = NULL;
    char *tok = strtok_r(exts, " \t", &saveptr);
    while (tok) {
        if (has_extension(filename, tok)) {
            /* Match found — set Content-Type */
            lsi_session_set_resp_header(session,
                                        "Content-Type", 12,
                                        dir->name, (int)strlen(dir->name));
            free(exts);
            return LSI_OK;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    free(exts);
    return LSI_OK;
}
