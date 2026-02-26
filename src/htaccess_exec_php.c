/**
 * htaccess_exec_php.c - PHP configuration directive executors
 *
 * Implements execution of php_value, php_flag, php_admin_value, and
 * php_admin_flag directives via LSIAPI session calls.
 *
 * Validates: Requirements 5.1, 5.2, 5.3, 5.4, 5.5
 */
#include "htaccess_exec_php.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  PHP_INI_SYSTEM settings list                                       */
/*                                                                     */
/*  These settings can only be set in php.ini or httpd.conf, NOT via   */
/*  php_value/php_flag in .htaccess. If referenced by php_value or     */
/*  php_flag, we log a warning and ignore the directive.               */
/*  php_admin_value/php_admin_flag CAN set these.                      */
/* ------------------------------------------------------------------ */

static const char *php_ini_system_settings[] = {
    "allow_url_fopen",
    "allow_url_include",
    "disable_classes",
    "disable_functions",
    "engine",
    "expose_php",
    "max_input_time",
    "memory_limit",
    "open_basedir",
    "post_max_size",
    "realpath_cache_size",
    "realpath_cache_ttl",
    "safe_mode",
    "upload_max_filesize",
    "upload_tmp_dir",
    "max_file_uploads",
    "sys_temp_dir",
    NULL
};

/**
 * Check if a PHP ini setting name is a PHP_INI_SYSTEM level setting.
 *
 * @param name  The PHP ini setting name.
 * @return 1 if it is a system-level setting, 0 otherwise.
 */
static int is_php_ini_system(const char *name)
{
    if (!name)
        return 0;

    for (int i = 0; php_ini_system_settings[i] != NULL; i++) {
        if (strcmp(name, php_ini_system_settings[i]) == 0)
            return 1;
    }
    return 0;
}

int exec_php_value(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name || !dir->value)
        return LSI_ERROR;

    /* PHP_INI_SYSTEM settings cannot be set via php_value */
    if (is_php_ini_system(dir->name)) {
        lsi_log(session, LSI_LOG_WARN,
                "php_value: setting '%s' is PHP_INI_SYSTEM level, ignored "
                "(line %d)",
                dir->name, dir->line_number);
        return LSI_OK;
    }

    int name_len = (int)strlen(dir->name);
    int val_len  = (int)strlen(dir->value);

    return lsi_session_set_php_ini(session, dir->name, name_len,
                                   dir->value, val_len, 0);
}

int exec_php_flag(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name || !dir->value)
        return LSI_ERROR;

    /* PHP_INI_SYSTEM settings cannot be set via php_flag */
    if (is_php_ini_system(dir->name)) {
        lsi_log(session, LSI_LOG_WARN,
                "php_flag: setting '%s' is PHP_INI_SYSTEM level, ignored "
                "(line %d)",
                dir->name, dir->line_number);
        return LSI_OK;
    }

    int name_len = (int)strlen(dir->name);
    int val_len  = (int)strlen(dir->value);

    return lsi_session_set_php_ini(session, dir->name, name_len,
                                   dir->value, val_len, 0);
}

int exec_php_admin_value(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name || !dir->value)
        return LSI_ERROR;

    int name_len = (int)strlen(dir->name);
    int val_len  = (int)strlen(dir->value);

    return lsi_session_set_php_ini(session, dir->name, name_len,
                                   dir->value, val_len, 1);
}

int exec_php_admin_flag(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name || !dir->value)
        return LSI_ERROR;

    int name_len = (int)strlen(dir->name);
    int val_len  = (int)strlen(dir->value);

    return lsi_session_set_php_ini(session, dir->name, name_len,
                                   dir->value, val_len, 1);
}
