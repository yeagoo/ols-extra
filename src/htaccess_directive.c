/**
 * htaccess_directive.c - Directive memory management
 *
 * Implements htaccess_directives_free() which walks a directive linked list
 * and releases all dynamically allocated memory, including type-specific
 * strings and nested children (FilesMatch).
 *
 * Validates: Requirements 2.2
 */
#include "htaccess_directive.h"
#include <stdlib.h>

/**
 * Free a single directive node's owned memory (not the node itself).
 */
static void directive_free_fields(htaccess_directive_t *dir)
{
    if (!dir)
        return;

    free(dir->name);
    free(dir->value);

    switch (dir->type) {
    case DIR_REDIRECT:
    case DIR_REDIRECT_MATCH:
        free(dir->data.redirect.pattern);
        break;

    case DIR_FILES_MATCH:
        free(dir->data.files_match.pattern);
        /* Recursively free nested children */
        htaccess_directives_free(dir->data.files_match.children);
        break;

    case DIR_SETENVIF:
    case DIR_BROWSER_MATCH:
        free(dir->data.envif.attribute);
        free(dir->data.envif.pattern);
        break;

    default:
        /* No additional heap allocations for other types */
        break;
    }
}

void htaccess_directives_free(htaccess_directive_t *head)
{
    htaccess_directive_t *cur = head;
    while (cur) {
        htaccess_directive_t *next = cur->next;
        directive_free_fields(cur);
        free(cur);
        cur = next;
    }
}
