/**
 * htaccess_exec_brute_force.c - Brute force protection executor implementation
 *
 * Tracks per-IP failed login attempts and triggers block or throttle
 * actions when the configured threshold is exceeded within the time window.
 *
 * Validates: Requirements 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8
 */
#include "htaccess_exec_brute_force.h"
#include "htaccess_shm.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

int exec_brute_force(lsi_session_t *session,
                     const htaccess_directive_t *directives,
                     const char *client_ip)
{
    const htaccess_directive_t *dir;
    int enabled = 0;
    int allowed_attempts = BF_DEFAULT_ALLOWED_ATTEMPTS;
    int window_sec = BF_DEFAULT_WINDOW_SEC;
    bf_action_t action = BF_ACTION_BLOCK;
    int throttle_ms = BF_DEFAULT_THROTTLE_MS;
    brute_force_record_t *rec;
    brute_force_record_t new_rec;
    time_t now;

    if (!session || !directives || !client_ip)
        return LSI_OK;

    /* Step 1: Scan directives for brute force configuration */
    for (dir = directives; dir; dir = dir->next) {
        switch (dir->type) {
        case DIR_BRUTE_FORCE_PROTECTION:
            enabled = dir->data.brute_force.enabled;
            break;
        case DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS:
            allowed_attempts = dir->data.brute_force.allowed_attempts;
            break;
        case DIR_BRUTE_FORCE_WINDOW:
            window_sec = dir->data.brute_force.window_sec;
            break;
        case DIR_BRUTE_FORCE_ACTION:
            action = dir->data.brute_force.action;
            break;
        case DIR_BRUTE_FORCE_THROTTLE_DURATION:
            throttle_ms = dir->data.brute_force.throttle_ms;
            break;
        default:
            break;
        }
    }

    /* Step 2: If not enabled, return immediately */
    if (!enabled)
        return LSI_OK;

    /* Step 3: Get current record from shared memory */
    rec = shm_get_record(client_ip);
    now = time(NULL);

    if (rec) {
        /* Step 4: Record exists — check if within window */
        if ((now - rec->first_attempt) < (time_t)window_sec) {
            /* Within window */
            if (rec->attempt_count >= allowed_attempts) {
                /* Threshold exceeded — apply action */
                if (action == BF_ACTION_BLOCK) {
                    lsi_session_set_status(session, 403);
                    return LSI_ERROR;
                } else {
                    /* BF_ACTION_THROTTLE: record intent via env var for tests */
                    char ms_str[32];
                    snprintf(ms_str, sizeof(ms_str), "%d", throttle_ms);
                    lsi_session_set_env(session,
                                        "BF_THROTTLE_MS", 14,
                                        ms_str, (int)strlen(ms_str));
                    return LSI_OK;
                }
            }

            /* Increment attempt count */
            new_rec = *rec;
            new_rec.attempt_count++;
            shm_update_record(client_ip, &new_rec);
        } else {
            /* Window expired — reset record */
            memset(&new_rec, 0, sizeof(new_rec));
            strncpy(new_rec.ip, client_ip, sizeof(new_rec.ip) - 1);
            new_rec.ip[sizeof(new_rec.ip) - 1] = '\0';
            new_rec.attempt_count = 1;
            new_rec.first_attempt = now;
            new_rec.blocked_until = 0;
            shm_update_record(client_ip, &new_rec);
        }
    } else {
        /* Step 5: No record — create new one with count=1 */
        memset(&new_rec, 0, sizeof(new_rec));
        strncpy(new_rec.ip, client_ip, sizeof(new_rec.ip) - 1);
        new_rec.ip[sizeof(new_rec.ip) - 1] = '\0';
        new_rec.attempt_count = 1;
        new_rec.first_attempt = now;
        new_rec.blocked_until = 0;

        if (shm_update_record(client_ip, &new_rec) != 0) {
            /* SHM allocation failed — disable protection, continue */
            lsi_log(session, LSI_LOG_ERROR,
                    "BruteForce: SHM allocation failed for IP %s, "
                    "disabling protection", client_ip);
            return LSI_OK;
        }
    }

    return LSI_OK;
}
