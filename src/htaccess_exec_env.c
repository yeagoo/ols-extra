/**
 * htaccess_exec_env.c - Environment variable directive executor
 *
 * Validates: Requirements 11.1, 11.2, 11.3, 11.4, 11.5, 11.6
 */
#include "htaccess_exec_env.h"
#include <string.h>
#include <regex.h>

static const char *get_attribute_value(lsi_session_t *session,
                                       const char *attribute,
                                       int *out_len)
{
    if (!attribute || !out_len)
        return NULL;
    if (strcmp(attribute, "Remote_Addr") == 0)
        return lsi_session_get_client_ip(session, out_len);
    if (strcmp(attribute, "Request_URI") == 0)
        return lsi_session_get_uri(session, out_len);
    int attr_len = (int)strlen(attribute);
    return lsi_session_get_req_header_by_name(
        session, attribute, attr_len, out_len);
}

static int match_and_set(lsi_session_t *session,
                         const htaccess_directive_t *dir,
                         const char *attr_value)
{
    regex_t re;
    int rc;
    if (!dir->data.envif.pattern)
        return LSI_ERROR;
    if (regcomp(&re, dir->data.envif.pattern,
                REG_EXTENDED | REG_NOSUB) != 0)
        return LSI_ERROR;
    rc = regexec(&re, attr_value, 0, NULL, 0);
    regfree(&re);
    if (rc != 0)
        return LSI_OK;
    if (!dir->name)
        return LSI_ERROR;
    int name_len = (int)strlen(dir->name);
    int val_len = dir->value ? (int)strlen(dir->value) : 0;
    const char *val = dir->value ? dir->value : "";
    return lsi_session_set_env(session, dir->name,
                               name_len, val, val_len);
}

int exec_setenv(lsi_session_t *session, const htaccess_directive_t *dir)
{
    if (!session || !dir || !dir->name)
        return LSI_ERROR;
    if (dir->type != DIR_SETENV)
        return LSI_ERROR;
    int name_len = (int)strlen(dir->name);
    int val_len = dir->value ? (int)strlen(dir->value) : 0;
    const char *val = dir->value ? dir->value : "";
    return lsi_session_set_env(session, dir->name,
                               name_len, val, val_len);
}

int exec_setenvif(lsi_session_t *session, const htaccess_directive_t *dir)
{
    int attr_len = 0;
    const char *attr_value;
    if (!session || !dir)
        return LSI_ERROR;
    if (dir->type != DIR_SETENVIF)
        return LSI_ERROR;
    if (!dir->data.envif.attribute)
        return LSI_ERROR;
    attr_value = get_attribute_value(session, dir->data.envif.attribute,
                                     &attr_len);
    if (!attr_value || attr_len <= 0)
        return LSI_OK;
    return match_and_set(session, dir, attr_value);
}

int exec_browser_match(lsi_session_t *session, const htaccess_directive_t *dir)
{
    int ua_len = 0;
    const char *ua;
    if (!session || !dir)
        return LSI_ERROR;
    if (dir->type != DIR_BROWSER_MATCH)
        return LSI_ERROR;
    ua = lsi_session_get_req_header_by_name(session,
        "User-Agent", 10, &ua_len);
    if (!ua || ua_len <= 0)
        return LSI_OK;
    return match_and_set(session, dir, ua);
}