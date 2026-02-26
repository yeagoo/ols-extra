/**
 * htaccess_exec_options.c - Options directive executor implementation
 *
 * Parses +/-Indexes, +/-FollowSymLinks, +/-MultiViews, +/-ExecCGI flags
 * from the parsed directive and calls lsi_session_set_dir_option() via LSIAPI.
 *
 * Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5
 */
#include "htaccess_exec_options.h"
#include <string.h>

/**
 * Apply a single option flag to the session.
 * enabled: 1 = enable, 0 = disable.
 */
static void apply_option(lsi_session_t *session,
                         const char *name, int tri_state)
{
    if (tri_state == 0)
        return; /* unchanged — skip */
    lsi_session_set_dir_option(session, name, tri_state > 0 ? 1 : 0);
}

int exec_options(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir)
        return LSI_ERROR;
    if (dir->type != DIR_OPTIONS)
        return LSI_ERROR;

    apply_option(session, "Indexes",        dir->data.options.indexes);
    apply_option(session, "FollowSymLinks", dir->data.options.follow_symlinks);
    apply_option(session, "MultiViews",     dir->data.options.multiviews);
    apply_option(session, "ExecCGI",        dir->data.options.exec_cgi);

    return LSI_OK;
}
