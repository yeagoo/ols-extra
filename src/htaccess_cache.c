/**
 * htaccess_cache.c - Hash table cache implementation
 *
 * Uses a simple hash table with separate chaining (linked list per bucket).
 * The hash function is djb2 applied to the file's absolute path string.
 * mtime comparison is used for invalidation: a get with a different mtime
 * returns a miss (-1).
 *
 * Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.6
 */
#include "htaccess_cache.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Global singleton cache                                              */
/* ------------------------------------------------------------------ */
static htaccess_cache_t *g_cache = NULL;

/* ------------------------------------------------------------------ */
/* Hash function — djb2 by Dan Bernstein                               */
/* ------------------------------------------------------------------ */
static size_t hash_string(const char *str, size_t num_buckets)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++) != 0)
        hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
    return hash % num_buckets;
}

/* ------------------------------------------------------------------ */
/* Estimate memory usage of a directive list (capped at 2 KB)          */
/* ------------------------------------------------------------------ */
static size_t estimate_directives_memory(const htaccess_directive_t *head)
{
    size_t total = 0;
    for (const htaccess_directive_t *d = head; d; d = d->next) {
        total += sizeof(htaccess_directive_t);
        if (d->name)  total += strlen(d->name) + 1;
        if (d->value) total += strlen(d->value) + 1;

        switch (d->type) {
        case DIR_REDIRECT:
        case DIR_REDIRECT_MATCH:
            if (d->data.redirect.pattern)
                total += strlen(d->data.redirect.pattern) + 1;
            break;
        case DIR_FILES_MATCH:
            if (d->data.files_match.pattern)
                total += strlen(d->data.files_match.pattern) + 1;
            total += estimate_directives_memory(d->data.files_match.children);
            break;
        case DIR_SETENVIF:
        case DIR_BROWSER_MATCH:
            if (d->data.envif.attribute)
                total += strlen(d->data.envif.attribute) + 1;
            if (d->data.envif.pattern)
                total += strlen(d->data.envif.pattern) + 1;
            break;
        default:
            break;
        }
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* Free a single cache entry (fields + node)                           */
/* ------------------------------------------------------------------ */
static void cache_entry_free(cache_entry_t *entry)
{
    if (!entry)
        return;
    free(entry->filepath);
    htaccess_directives_free(entry->directives);
    free(entry);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int htaccess_cache_init(size_t initial_buckets)
{
    if (initial_buckets == 0)
        initial_buckets = 64;

    htaccess_cache_t *cache = calloc(1, sizeof(htaccess_cache_t));
    if (!cache)
        return -1;

    cache->buckets = calloc(initial_buckets, sizeof(cache_entry_t *));
    if (!cache->buckets) {
        free(cache);
        return -1;
    }

    cache->num_buckets = initial_buckets;
    cache->num_entries = 0;
    g_cache = cache;
    return 0;
}

int htaccess_cache_get(const char *filepath, time_t current_mtime,
                       htaccess_directive_t **out_directives)
{
    if (!g_cache || !filepath || !out_directives)
        return -1;

    size_t idx = hash_string(filepath, g_cache->num_buckets);
    cache_entry_t *entry = g_cache->buckets[idx];

    while (entry) {
        if (strcmp(entry->filepath, filepath) == 0) {
            /* Found the path — check mtime */
            if (entry->mtime == current_mtime) {
                *out_directives = entry->directives;
                return 0;  /* Cache hit */
            }
            return -1;     /* mtime mismatch → miss */
        }
        entry = entry->chain_next;
    }

    return -1; /* Not found → miss */
}

int htaccess_cache_put(const char *filepath, time_t mtime,
                       htaccess_directive_t *directives)
{
    if (!g_cache || !filepath)
        return -1;

    size_t idx = hash_string(filepath, g_cache->num_buckets);

    /* Check if an entry for this path already exists */
    cache_entry_t *prev = NULL;
    cache_entry_t *entry = g_cache->buckets[idx];

    while (entry) {
        if (strcmp(entry->filepath, filepath) == 0) {
            /* Replace existing entry's data */
            htaccess_directives_free(entry->directives);
            entry->directives = directives;
            entry->mtime = mtime;
            entry->memory_usage = sizeof(cache_entry_t)
                                + strlen(filepath) + 1
                                + estimate_directives_memory(directives);
            return 0;
        }
        prev = entry;
        entry = entry->chain_next;
    }

    /* New entry */
    cache_entry_t *new_entry = calloc(1, sizeof(cache_entry_t));
    if (!new_entry)
        return -1;

    new_entry->filepath = strdup(filepath);
    if (!new_entry->filepath) {
        free(new_entry);
        return -1;
    }

    new_entry->mtime = mtime;
    new_entry->directives = directives;
    new_entry->memory_usage = sizeof(cache_entry_t)
                            + strlen(filepath) + 1
                            + estimate_directives_memory(directives);
    new_entry->chain_next = NULL;

    /* Insert at head of bucket chain */
    new_entry->chain_next = g_cache->buckets[idx];
    g_cache->buckets[idx] = new_entry;
    g_cache->num_entries++;

    return 0;
}

void htaccess_cache_destroy(void)
{
    if (!g_cache)
        return;

    for (size_t i = 0; i < g_cache->num_buckets; i++) {
        cache_entry_t *entry = g_cache->buckets[i];
        while (entry) {
            cache_entry_t *next = entry->chain_next;
            cache_entry_free(entry);
            entry = next;
        }
    }

    free(g_cache->buckets);
    free(g_cache);
    g_cache = NULL;
}
