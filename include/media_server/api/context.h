#ifndef MEDIA_SERVER_API_CONTEXT_H
#define MEDIA_SERVER_API_CONTEXT_H

#include "media_server/library/catalog.h"

/*
 * Per-request application state passed as router user_data.
 * library_dir is borrowed (e.g. from argv) and must outlive the server.
 */
typedef struct app_context {
    catalog_t *catalog;
    const char *library_dir; /* may be NULL if no library configured */
} app_context_t;

#endif /* MEDIA_SERVER_API_CONTEXT_H */
