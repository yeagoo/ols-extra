/**
 * htaccess_shm.c - Shared memory management implementation
 *
 * In-memory hash table implementation for brute force IP tracking.
 * Uses a simple open-addressing hash table with linear probing.
 *
 * In production, this would be backed by /dev/shm/ols/ with file locks
 * for inter-process synchronization. The in-memory version is sufficient
 * for testing and single-process use.
 *
 * Validates: Requirements 12.2, 12.3, 12.4
 */
#include "htaccess_shm.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal hash table structure                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    brute_force_record_t *records;  /* Array of records */
    int                  *occupied; /* 1 if slot is in use, 0 otherwise */
    size_t                capacity; /* Max number of records */
    size_t                count;    /* Current number of records */
} shm_store_t;

static shm_store_t *g_store = NULL;

/* ------------------------------------------------------------------ */
/*  Hash function (djb2)                                               */
/* ------------------------------------------------------------------ */

static size_t hash_ip(const char *ip, size_t capacity)
{
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*ip++) != 0)
        hash = ((hash << 5) + hash) + (unsigned long)c;

    return hash % capacity;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

int shm_init(const char *shm_path, size_t max_records)
{
    (void)shm_path; /* Unused in in-memory implementation */

    if (max_records == 0)
        return -1;

    if (g_store) {
        shm_destroy(); /* Clean up previous instance */
    }

    g_store = (shm_store_t *)calloc(1, sizeof(shm_store_t));
    if (!g_store)
        return -1;

    g_store->records = (brute_force_record_t *)calloc(max_records,
                                                       sizeof(brute_force_record_t));
    if (!g_store->records) {
        free(g_store);
        g_store = NULL;
        return -1;
    }

    g_store->occupied = (int *)calloc(max_records, sizeof(int));
    if (!g_store->occupied) {
        free(g_store->records);
        free(g_store);
        g_store = NULL;
        return -1;
    }

    g_store->capacity = max_records;
    g_store->count = 0;

    return 0;
}

brute_force_record_t *shm_get_record(const char *ip)
{
    size_t idx, start;

    if (!g_store || !ip)
        return NULL;

    start = hash_ip(ip, g_store->capacity);
    idx = start;

    do {
        if (g_store->occupied[idx] &&
            strncmp(g_store->records[idx].ip, ip, sizeof(g_store->records[idx].ip) - 1) == 0) {
            return &g_store->records[idx];
        }
        if (!g_store->occupied[idx]) {
            return NULL; /* Empty slot means IP not found */
        }
        idx = (idx + 1) % g_store->capacity;
    } while (idx != start);

    return NULL; /* Table full, not found */
}

int shm_update_record(const char *ip, const brute_force_record_t *record)
{
    size_t idx, start;

    if (!g_store || !ip || !record)
        return -1;

    /* Try to find existing record first */
    start = hash_ip(ip, g_store->capacity);
    idx = start;

    do {
        if (g_store->occupied[idx] &&
            strncmp(g_store->records[idx].ip, ip, sizeof(g_store->records[idx].ip) - 1) == 0) {
            /* Update existing record */
            memcpy(&g_store->records[idx], record, sizeof(brute_force_record_t));
            strncpy(g_store->records[idx].ip, ip, sizeof(g_store->records[idx].ip) - 1);
            g_store->records[idx].ip[sizeof(g_store->records[idx].ip) - 1] = '\0';
            return 0;
        }
        if (!g_store->occupied[idx]) {
            /* Empty slot — insert new record */
            memcpy(&g_store->records[idx], record, sizeof(brute_force_record_t));
            strncpy(g_store->records[idx].ip, ip, sizeof(g_store->records[idx].ip) - 1);
            g_store->records[idx].ip[sizeof(g_store->records[idx].ip) - 1] = '\0';
            g_store->occupied[idx] = 1;
            g_store->count++;
            return 0;
        }
        idx = (idx + 1) % g_store->capacity;
    } while (idx != start);

    return -1; /* Table full */
}

int shm_cleanup_expired(time_t now)
{
    size_t i;
    int cleaned = 0;

    if (!g_store)
        return 0;

    for (i = 0; i < g_store->capacity; i++) {
        if (!g_store->occupied[i])
            continue;

        brute_force_record_t *rec = &g_store->records[i];

        /*
         * A record is expired if:
         * - It has a blocked_until time that has passed, OR
         * - It has no block and the first_attempt is old enough
         *   (we use a generous 2x window as cleanup threshold)
         *
         * For simplicity, remove records where blocked_until > 0
         * and blocked_until <= now (block expired).
         * Also remove records where first_attempt < now (window passed)
         * and they are not currently blocked.
         */
        int expired = 0;

        if (rec->blocked_until > 0 && rec->blocked_until <= now) {
            expired = 1;
        } else if (rec->blocked_until == 0 && rec->first_attempt > 0 &&
                   rec->first_attempt < now) {
            /* Record with no active block and old first_attempt */
            expired = 1;
        }

        if (expired) {
            memset(rec, 0, sizeof(brute_force_record_t));
            g_store->occupied[i] = 0;
            g_store->count--;
            cleaned++;
        }
    }

    return cleaned;
}

void shm_destroy(void)
{
    if (!g_store)
        return;

    free(g_store->records);
    free(g_store->occupied);
    free(g_store);
    g_store = NULL;
}
