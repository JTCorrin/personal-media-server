#include "media_server/api/stub.h"

#include "media_server/http/server.h"

void handle_not_implemented(const router_match_t *match, void *req, void *res)
{
    (void)req;
    http_reply_not_implemented(res, match != NULL ? match->pattern : NULL);
}
