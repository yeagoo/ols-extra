/**
 * htaccess_exec_auth.c - AuthType Basic executor
 *
 * Collects auth config from directive list, validates Authorization header
 * against htpasswd file entries. Supports crypt hash format.
 *
 * Validates: Requirements 10.1-10.9
 */
#define _GNU_SOURCE
#include "htaccess_exec_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* Base64 decode table */
static const unsigned char b64_table[256] = {
    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
    ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
    ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
    ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
    ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
    ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
};

/**
 * Simple base64 decode. Returns decoded length, or -1 on error.
 */
static int base64_decode(const char *in, size_t in_len,
                         unsigned char *out, size_t out_cap)
{
    size_t i, j = 0;
    unsigned char buf[4];
    int pad = 0;

    for (i = 0; i < in_len; i += 4) {
        int k;
        for (k = 0; k < 4 && (i + k) < in_len; k++) {
            char c = in[i + k];
            if (c == '=') { pad++; buf[k] = 0; }
            else buf[k] = b64_table[(unsigned char)c];
        }
        if (j + 3 > out_cap) return -1;
        out[j++] = (buf[0] << 2) | (buf[1] >> 4);
        if (pad < 2) out[j++] = (buf[1] << 4) | (buf[2] >> 2);
        if (pad < 1) out[j++] = (buf[2] << 6) | buf[3];
    }
    return (int)j;
}

int htpasswd_check(const char *hash, const char *password)
{
    if (!hash || !password)
        return -1;

    /* crypt() format: 13-char DES or $id$salt$hash */
    char *result = crypt(password, hash);
    if (!result)
        return -1;

    return (strcmp(result, hash) == 0) ? 1 : 0;
}

/**
 * Parse "Basic <base64>" from Authorization header.
 * Returns 1 on success, fills user/pass (caller frees).
 */
static int parse_basic_auth(const char *auth_header, int auth_len,
                            char **out_user, char **out_pass)
{
    if (!auth_header || auth_len < 7)
        return 0;

    /* Must start with "Basic " */
    if (strncasecmp(auth_header, "Basic ", 6) != 0)
        return 0;

    const char *b64 = auth_header + 6;
    size_t b64_len = (size_t)(auth_len - 6);

    unsigned char decoded[256];
    int dec_len = base64_decode(b64, b64_len, decoded, sizeof(decoded) - 1);
    if (dec_len <= 0)
        return 0;
    decoded[dec_len] = '\0';

    /* Split on ':' */
    char *colon = strchr((char *)decoded, ':');
    if (!colon)
        return 0;

    *colon = '\0';
    *out_user = strdup((char *)decoded);
    *out_pass = strdup(colon + 1);
    return 1;
}

int exec_auth_basic(lsi_session_t *session,
                    const htaccess_directive_t *directives)
{
    if (!session || !directives)
        return LSI_OK;

    /* Collect auth config from directive list */
    const char *auth_type = NULL;
    const char *auth_name = NULL;
    const char *auth_user_file = NULL;
    int require_valid_user = 0;

    const htaccess_directive_t *dir;
    for (dir = directives; dir; dir = dir->next) {
        switch (dir->type) {
        case DIR_AUTH_TYPE:
            auth_type = dir->value;
            break;
        case DIR_AUTH_NAME:
            auth_name = dir->value;
            break;
        case DIR_AUTH_USER_FILE:
            auth_user_file = dir->value;
            break;
        case DIR_REQUIRE_VALID_USER:
            require_valid_user = 1;
            break;
        default:
            break;
        }
    }

    /* If no AuthType Basic + Require valid-user, nothing to do */
    if (!auth_type || strcasecmp(auth_type, "Basic") != 0)
        return LSI_OK;
    if (!require_valid_user)
        return LSI_OK;

    /* AuthUserFile is required */
    if (!auth_user_file) {
        lsi_log(session, LSI_LOG_ERROR,
                "[htaccess] AuthUserFile not specified");
        lsi_session_set_status(session, 500);
        return LSI_ERROR;
    }

    /* Get Authorization header */
    int auth_len = 0;
    const char *auth_header = lsi_session_get_auth_header(session, &auth_len);

    char *user = NULL;
    char *pass = NULL;
    if (!auth_header || auth_len <= 0 ||
        !parse_basic_auth(auth_header, auth_len, &user, &pass)) {
        /* No credentials — send 401 */
        if (auth_name)
            lsi_session_set_www_authenticate(session, auth_name,
                                             (int)strlen(auth_name));
        lsi_session_set_status(session, 401);
        return LSI_ERROR;
    }

    /* Open htpasswd file */
    FILE *fp = fopen(auth_user_file, "r");
    if (!fp) {
        lsi_log(session, LSI_LOG_ERROR,
                "[htaccess] Cannot open AuthUserFile: %s", auth_user_file);
        free(user);
        free(pass);
        lsi_session_set_status(session, 500);
        return LSI_ERROR;
    }

    /* Search for matching user */
    char line[512];
    int authenticated = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';

        if (strcmp(line, user) != 0) continue;

        /* Found user — check password */
        const char *hash = colon + 1;
        if (htpasswd_check(hash, pass) == 1) {
            authenticated = 1;
            break;
        }
    }
    fclose(fp);

    free(user);
    free(pass);

    if (!authenticated) {
        if (auth_name)
            lsi_session_set_www_authenticate(session, auth_name,
                                             (int)strlen(auth_name));
        lsi_session_set_status(session, 401);
        return LSI_ERROR;
    }

    return LSI_OK;
}
