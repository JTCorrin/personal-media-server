#include "media_server/api/ping.h"

#include "media_server/http/server.h"

void handle_ping(const router_match_t *match, void *req, void *res)
{
    (void)match;
    (void)req;
    http_reply_json(res, 200, "{\"ok\":true}");
}
