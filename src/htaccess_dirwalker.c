/**
 * htaccess_dirwalker.c - Directory hierarchy traversal implementation
 *
 * Walks from document root to target directory, collecting .htaccess
 * directives at each level via the cache, then merges them with
 * child-overrides-parent semantics.
 *
 * Validates: Requirements 13.1, 13.2, 13.3
 */
#include "htaccess_dirwalker.h"
#include "htaccess_cache.h"
#include "htaccess_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Maximum directory depth we support */
#define MAX_DIR_DEPTH 64

/* Maximum path length */
#define MAX_PATH_LEN 4096

/* ------------------------------------------------------------------ */
/* Internal: deep-copy a single directive node                         */
/* ------------------------------------------------------------------ */
static htaccess_directive_t *copy_directive(const htaccess_directive_t *src)
{
    htaccess_directive_t *d = calloc(1, sizeof(htaccess_directive_t));
    if (!d)
        return NULL;

    d->type = src->type;
    d->line_number = src->line_number;
    if (src->name)  d->name  = strdup(src->name);
    if (src->value) d->value = strdup(src->value);
    d->data = src->data;
    d->next = NULL;

    /* Deep-copy type-specific heap fields */
    switch (src->type) {
    case DIR_REDIRECT:
    case DIR_REDIRECT_MATCH:
        if (src->data.redirect.pattern)
            d->data.redirect.pattern = strdup(src->data.redirect.pattern);
        break;
    case DIR_FILES_MATCH:
        if (src->data.files_match.pattern)
            d->data.files_match.pattern = strdup(src->data.files_match.pattern);
        /* Deep-copy children */
        d->data.files_match.children = NULL;
        if (src->data.files_match.children) {
            htaccess_directive_t *child_head = NULL;
            htaccess_directive_t *child_tail = NULL;
            for (const htaccess_directive_t *c = src->data.files_match.children;
                 c; c = c->next) {
                htaccess_directive_t *cc = copy_directive(c);
                if (!cc) break;
                if (!child_head)
                    child_head = cc;
                else
                    child_tail->next = cc;
                child_tail = cc;
            }
            d->data.files_match.children = child_head;
        }
        break;
    case DIR_SETENVIF:
    case DIR_BROWSER_MATCH:
        if (src->data.envif.attribute)
            d->data.envif.attribute = strdup(src->data.envif.attribute);
        if (src->data.envif.pattern)
            d->data.envif.pattern = strdup(src->data.envif.pattern);
        break;
    default:
        break;
    }

    return d;
}

/* ------------------------------------------------------------------ */
/* Internal: deep-copy a directive list                                */
/* ------------------------------------------------------------------ */
static htaccess_directive_t *copy_directive_list(const htaccess_directive_t *src)
{
    htaccess_directive_t *head = NULL;
    htaccess_directive_t *tail = NULL;

    for (const htaccess_directive_t *s = src; s; s = s->next) {
        htaccess_directive_t *d = copy_directive(s);
        if (!d) {
            htaccess_directives_free(head);
            return NULL;
        }
        if (!head)
            head = d;
        else
            tail->next = d;
        tail = d;
    }
    return head;
}

/* ------------------------------------------------------------------ */
/* Internal: check if two directives match for override purposes       */
/*                                                                     */
/* "Same type" means same directive_type_t AND same identifying key.   */
/* For Header directives, the key is the header name.                  */
/* For PHP directives, the key is the ini setting name.                */
/* For most others, just the type is sufficient.                       */
/* ------------------------------------------------------------------ */
static int directives_match_for_override(const htaccess_directive_t *a,
                                         const htaccess_directive_t *b)
{
    if (a->type != b->type)
        return 0;

    switch (a->type) {
    /* Header directives: match by header name */
    case DIR_HEADER_SET:
    case DIR_HEADER_UNSET:
    case DIR_HEADER_APPEND:
    case DIR_HEADER_MERGE:
    case DIR_HEADER_ADD:
    case DIR_REQUEST_HEADER_SET:
    case DIR_REQUEST_HEADER_UNSET:
        if (a->name && b->name)
            return strcmp(a->name, b->name) == 0;
        return 0;

    /* PHP directives: match by ini setting name */
    case DIR_PHP_VALUE:
    case DIR_PHP_FLAG:
    case DIR_PHP_ADMIN_VALUE:
    case DIR_PHP_ADMIN_FLAG:
        if (a->name && b->name)
            return strcmp(a->name, b->name) == 0;
        return 0;

    /* SetEnv: match by variable name */
    case DIR_SETENV:
        if (a->name && b->name)
            return strcmp(a->name, b->name) == 0;
        return 0;

    /* ExpiresByType: match by MIME type */
    case DIR_EXPIRES_BY_TYPE:
        if (a->name && b->name)
            return strcmp(a->name, b->name) == 0;
        return 0;

    /* ErrorDocument: match by error code */
    case DIR_ERROR_DOCUMENT:
        return a->data.error_doc.error_code == b->data.error_doc.error_code;

    /* For these, just matching by type is enough (singleton directives) */
    case DIR_ORDER:
    case DIR_EXPIRES_ACTIVE:
    case DIR_BRUTE_FORCE_PROTECTION:
    case DIR_BRUTE_FORCE_ALLOWED_ATTEMPTS:
    case DIR_BRUTE_FORCE_WINDOW:
    case DIR_BRUTE_FORCE_ACTION:
    case DIR_BRUTE_FORCE_THROTTLE_DURATION:
        return 1;

    /* Allow/Deny: match by value (CIDR or "all") */
    case DIR_ALLOW_FROM:
    case DIR_DENY_FROM:
        if (a->value && b->value)
            return strcmp(a->value, b->value) == 0;
        return 0;

    /* Redirect: match by source path */
    case DIR_REDIRECT:
        if (a->name && b->name)
            return strcmp(a->name, b->name) == 0;
        return 0;

    /* RedirectMatch: match by pattern */
    case DIR_REDIRECT_MATCH:
        if (a->data.redirect.pattern && b->data.redirect.pattern)
            return strcmp(a->data.redirect.pattern,
                          b->data.redirect.pattern) == 0;
        return 0;

    /* FilesMatch: match by pattern */
    case DIR_FILES_MATCH:
        if (a->data.files_match.pattern && b->data.files_match.pattern)
            return strcmp(a->data.files_match.pattern,
                          b->data.files_match.pattern) == 0;
        return 0;

    /* SetEnvIf/BrowserMatch: match by attribute + pattern + name */
    case DIR_SETENVIF:
    case DIR_BROWSER_MATCH:
        if (a->name && b->name && a->data.envif.pattern && b->data.envif.pattern)
            return strcmp(a->name, b->name) == 0 &&
                   strcmp(a->data.envif.pattern, b->data.envif.pattern) == 0;
        return 0;

    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Internal: merge child directives into parent list                   */
/*                                                                     */
/* For each child directive, if a matching parent directive exists,     */
/* replace it. Otherwise, append the child directive to the end.       */
/* Returns the merged list (may be a new head).                        */
/* ------------------------------------------------------------------ */
static htaccess_directive_t *merge_directives(htaccess_directive_t *parent,
                                              const htaccess_directive_t *child)
{
    if (!child)
        return parent;
    if (!parent)
        return copy_directive_list(child);

    for (const htaccess_directive_t *c = child; c; c = c->next) {
        /* Check if this child directive overrides an existing parent */
        int replaced = 0;
        htaccess_directive_t *prev = NULL;
        for (htaccess_directive_t *p = parent; p; p = p->next) {
            if (directives_match_for_override(p, c)) {
                /* Replace parent directive in-place with child copy */
                htaccess_directive_t *replacement = copy_directive(c);
                if (!replacement) break;
                replacement->next = p->next;
                if (prev)
                    prev->next = replacement;
                else
                    parent = replacement;
                /* Free the old parent node */
                p->next = NULL;
                htaccess_directives_free(p);
                replaced = 1;
                break;
            }
            prev = p;
        }

        if (!replaced) {
            /* Append child directive to end of parent list */
            htaccess_directive_t *copy = copy_directive(c);
            if (!copy) continue;
            htaccess_directive_t *tail = parent;
            while (tail->next)
                tail = tail->next;
            tail->next = copy;
        }
    }

    return parent;
}

/* ------------------------------------------------------------------ */
/* Internal: try to read and parse a file, then cache it               */
/* ------------------------------------------------------------------ */
static htaccess_directive_t *read_and_cache(const char *htaccess_path)
{
    struct stat st;
    if (stat(htaccess_path, &st) != 0)
        return NULL; /* File doesn't exist or can't stat — skip */

    FILE *fp = fopen(htaccess_path, "r");
    if (!fp)
        return NULL;

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        return NULL;
    }

    char *content = malloc((size_t)fsize + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t nread = fread(content, 1, (size_t)fsize, fp);
    fclose(fp);
    content[nread] = '\0';

    htaccess_directive_t *dirs = htaccess_parse(content, nread, htaccess_path);
    free(content);

    /* Cache the result (cache takes ownership of dirs) */
    htaccess_cache_put(htaccess_path, st.st_mtime, dirs);

    /* We need to return a copy since cache owns the original */
    htaccess_directive_t *cached = NULL;
    htaccess_cache_get(htaccess_path, st.st_mtime, &cached);
    return cached;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

htaccess_directive_t *htaccess_dirwalk(lsi_session_t *session,
                                       const char *doc_root,
                                       const char *target_dir)
{
    (void)session; /* Not used currently; reserved for logging */

    if (!doc_root || !target_dir)
        return NULL;

    size_t root_len = strlen(doc_root);
    size_t target_len = strlen(target_dir);

    /* Strip trailing slashes from doc_root for comparison */
    while (root_len > 1 && doc_root[root_len - 1] == '/')
        root_len--;

    /* target_dir must start with doc_root */
    if (target_len < root_len)
        return NULL;
    if (strncmp(doc_root, target_dir, root_len) != 0)
        return NULL;

    /* Collect directory paths from root to target */
    char paths[MAX_DIR_DEPTH][MAX_PATH_LEN];
    int num_paths = 0;

    /* First path: doc_root itself */
    if (root_len < MAX_PATH_LEN) {
        memcpy(paths[num_paths], doc_root, root_len);
        paths[num_paths][root_len] = '\0';
        num_paths++;
    }

    /* Walk through remaining path components */
    const char *rest = target_dir + root_len;
    size_t current_len = root_len;

    while (*rest && num_paths < MAX_DIR_DEPTH) {
        /* Skip leading slash */
        if (*rest == '/') {
            rest++;
            continue;
        }

        /* Find end of this path component */
        const char *slash = strchr(rest, '/');
        size_t comp_len = slash ? (size_t)(slash - rest) : strlen(rest);

        if (current_len + 1 + comp_len >= MAX_PATH_LEN)
            break;

        /* Build path up to this component */
        memcpy(paths[num_paths], doc_root, root_len);
        paths[num_paths][root_len] = '\0';

        /* Append all components up to and including this one */
        const char *p = target_dir + root_len;
        size_t path_len = root_len;
        int components_added = 0;
        int target_components = num_paths; /* how many components to add */

        while (*p && components_added < target_components) {
            if (*p == '/') {
                p++;
                continue;
            }
            const char *next_slash = strchr(p, '/');
            size_t clen = next_slash ? (size_t)(next_slash - p) : strlen(p);
            paths[num_paths][path_len] = '/';
            memcpy(paths[num_paths] + path_len + 1, p, clen);
            path_len += 1 + clen;
            paths[num_paths][path_len] = '\0';
            p = next_slash ? next_slash + 1 : p + clen;
            components_added++;
        }

        num_paths++;
        current_len += 1 + comp_len;
        rest = slash ? slash : rest + comp_len;
    }

    /* Simpler approach: rebuild paths array properly */
    num_paths = 0;

    /* Build all directory paths from root to target */
    char current_path[MAX_PATH_LEN];
    if (root_len >= MAX_PATH_LEN)
        return NULL;

    memcpy(current_path, doc_root, root_len);
    current_path[root_len] = '\0';

    /* Add doc_root */
    strncpy(paths[num_paths], current_path, MAX_PATH_LEN - 1);
    paths[num_paths][MAX_PATH_LEN - 1] = '\0';
    num_paths++;

    /* Walk remaining components */
    rest = target_dir + root_len;
    while (*rest && num_paths < MAX_DIR_DEPTH) {
        if (*rest == '/') {
            rest++;
            continue;
        }
        const char *slash = strchr(rest, '/');
        size_t comp_len = slash ? (size_t)(slash - rest) : strlen(rest);

        size_t cur_len = strlen(current_path);
        if (cur_len + 1 + comp_len >= MAX_PATH_LEN)
            break;

        current_path[cur_len] = '/';
        memcpy(current_path + cur_len + 1, rest, comp_len);
        current_path[cur_len + 1 + comp_len] = '\0';

        strncpy(paths[num_paths], current_path, MAX_PATH_LEN - 1);
        paths[num_paths][MAX_PATH_LEN - 1] = '\0';
        num_paths++;

        rest = slash ? slash + 1 : rest + comp_len;
    }

    /* Now process each directory level */
    htaccess_directive_t *merged = NULL;

    for (int i = 0; i < num_paths; i++) {
        /* Construct .htaccess path for this directory */
        char htaccess_path[MAX_PATH_LEN];
        int written = snprintf(htaccess_path, MAX_PATH_LEN,
                               "%s/.htaccess", paths[i]);
        if (written < 0 || written >= MAX_PATH_LEN)
            continue;

        /* Try cache first */
        htaccess_directive_t *level_dirs = NULL;
        struct stat st;
        time_t mtime = 0;

        if (stat(htaccess_path, &st) == 0)
            mtime = st.st_mtime;

        int cache_hit = htaccess_cache_get(htaccess_path, mtime, &level_dirs);

        if (cache_hit != 0) {
            /* Cache miss — try to read and parse the file */
            level_dirs = read_and_cache(htaccess_path);
        }

        if (level_dirs) {
            /* Merge this level's directives into the accumulated result */
            merged = merge_directives(merged, level_dirs);
        }
        /* If no directives at this level, skip (doesn't affect inheritance) */
    }

    return merged;
}
