#include "media_server/http/server.h"

#include "media_server/util/log.h"

#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct http_server {
    struct mg_mgr mgr;
    struct mg_connection *listener;
    router_t *router;
    bool listening;
};

static void copy_mg_str(char *dst, size_t dst_size, const struct mg_str *src)
{
    size_t n;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL || src->buf == NULL) {
        dst[0] = '\0';
        return;
    }

    n = src->len;
    if (n >= dst_size) {
        n = dst_size - 1;
    }

    memcpy(dst, src->buf, n);
    dst[n] = '\0';
}

static void on_http_message(struct mg_connection *c, struct mg_http_message *hm,
                            router_t *router)
{
    char method[16];
    char path[512];
    router_match_t match;

    copy_mg_str(method, sizeof(method), &hm->method);
    copy_mg_str(path, sizeof(path), &hm->uri);

    LOG_INFO("http", "%s %s", method, path);

    if (router_match(router, method, path, &match) == 0) {
        match.handler(&match, hm, c);
        return;
    }

    http_reply_not_found(c);
}

static void event_handler(struct mg_connection *c, int ev, void *ev_data)
{
    http_server_t *server = (http_server_t *)c->fn_data;

    if (ev != MG_EV_HTTP_MSG || server == NULL || server->router == NULL) {
        return;
    }

    on_http_message(c, (struct mg_http_message *)ev_data, server->router);
}

http_server_t *http_server_create(const http_server_config_t *config)
{
    http_server_t *server;

    if (config == NULL || config->router == NULL || config->listen_url == NULL ||
        config->listen_url[0] == '\0') {
        return NULL;
    }

    server = calloc(1, sizeof(*server));
    if (server == NULL) {
        return NULL;
    }

    server->router = config->router;
    mg_mgr_init(&server->mgr);

    server->listener =
        mg_http_listen(&server->mgr, config->listen_url, event_handler, server);
    if (server->listener == NULL) {
        LOG_ERROR("http", "failed to listen on %s", config->listen_url);
        mg_mgr_free(&server->mgr);
        free(server);
        return NULL;
    }

    server->listening = true;
    LOG_INFO("http", "listening on %s", config->listen_url);
    return server;
}

void http_server_destroy(http_server_t *server)
{
    if (server == NULL) {
        return;
    }

    mg_mgr_free(&server->mgr);
    free(server);
}

bool http_server_is_listening(const http_server_t *server)
{
    return server != NULL && server->listening;
}

void http_server_poll(http_server_t *server, int timeout_ms)
{
    if (server == NULL) {
        return;
    }

    mg_mgr_poll(&server->mgr, timeout_ms);
}

void http_reply_text(void *res, int status, const char *content_type, const char *body)
{
    struct mg_connection *c = (struct mg_connection *)res;
    char headers[128];
    int n;

    if (c == NULL) {
        return;
    }

    if (content_type == NULL) {
        content_type = "text/plain";
    }
    if (body == NULL) {
        body = "";
    }

    /* A truncated header would lose its trailing \r\n and corrupt the
     * response, so fall back to a safe default instead. */
    n = snprintf(headers, sizeof(headers), "Content-Type: %s\r\n", content_type);
    if (n < 0 || (size_t)n >= sizeof(headers)) {
        snprintf(headers, sizeof(headers), "Content-Type: text/plain\r\n");
    }

    mg_http_reply(c, status, headers, "%s", body);
}

void http_reply_json(void *res, int status, const char *json_body)
{
    http_reply_text(res, status, "application/json", json_body);
}

void http_reply_not_found(void *res)
{
    http_reply_json(res, 404, "{\"error\":\"not found\"}");
}

void http_reply_not_implemented(void *res, const char *path)
{
    char body[256];
    int n;

    if (path == NULL || path[0] == '\0') {
        http_reply_json(res, 501, "{\"error\":\"not_implemented\"}");
        return;
    }

    /* Patterns are compile-time constants (no quotes), so snprintf is safe. */
    n = snprintf(body, sizeof(body),
                 "{\"error\":\"not_implemented\",\"path\":\"%s\"}", path);
    if (n < 0 || (size_t)n >= sizeof(body)) {
        http_reply_json(res, 501, "{\"error\":\"not_implemented\"}");
        return;
    }

    http_reply_json(res, 501, body);
}

void http_reply_file(void *req, void *res, const char *abs_path, const char *content_type)
{
    struct mg_connection *c = (struct mg_connection *)res;
    struct mg_http_message *hm = (struct mg_http_message *)req;
    struct mg_http_serve_opts opts;
    char extra[160];

    if (c == NULL || hm == NULL || abs_path == NULL || abs_path[0] == '\0') {
        http_reply_not_found(res);
        return;
    }

    memset(&opts, 0, sizeof(opts));
    if (content_type != NULL && content_type[0] != '\0') {
        int n = snprintf(extra, sizeof(extra), "Content-Type: %s\r\n", content_type);
        /* Only attach the header if it fit; truncation would corrupt it. */
        if (n > 0 && (size_t)n < sizeof(extra)) {
            opts.extra_headers = extra;
        }
    }

    mg_http_serve_file(c, hm, abs_path, &opts);
}
