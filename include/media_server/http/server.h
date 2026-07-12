#ifndef MEDIA_SERVER_HTTP_SERVER_H
#define MEDIA_SERVER_HTTP_SERVER_H

#include "media_server/http/router.h"

#include <stdbool.h>

/*
 * Thin HTTP adapter over Mongoose.
 *
 * Responsibilities:
 *   - listen / poll / accept connections
 *   - parse method + URI
 *   - log each request
 *   - dispatch through router_match()
 *   - send a 404 when nothing matches
 *
 * Handlers receive:
 *   req  = opaque request (currently mg_http_message *)
 *   res  = opaque connection for replies (currently mg_connection *)
 *
 * Prefer http_reply_* helpers below instead of calling Mongoose directly.
 */

typedef struct http_server http_server_t;

typedef struct http_server_config {
    const char *listen_url; /* e.g. "http://127.0.0.1:8080" */
    router_t *router;       /* required; owned by caller */
} http_server_config_t;

/* Create a server. Returns NULL on failure. Does not take ownership of router. */
http_server_t *http_server_create(const http_server_config_t *config);

void http_server_destroy(http_server_t *server);

/* True once listening successfully. */
bool http_server_is_listening(const http_server_t *server);

/* Drive the event loop once. timeout_ms is passed to Mongoose poll. */
void http_server_poll(http_server_t *server, int timeout_ms);

/* Reply helpers for route handlers (res is the connection void*). */
void http_reply_text(void *res, int status, const char *content_type, const char *body);
void http_reply_json(void *res, int status, const char *json_body);
void http_reply_not_found(void *res);
/* 501 JSON. path is an optional route pattern included in the body. */
void http_reply_not_implemented(void *res, const char *path);

/*
 * Serve a local file. req must be the mg_http_message* from the handler;
 * res is the connection. content_type may be NULL (Mongoose defaults).
 */
void http_reply_file(void *req, void *res, const char *abs_path, const char *content_type);

#endif /* MEDIA_SERVER_HTTP_SERVER_H */
