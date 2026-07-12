#ifndef MEDIA_SERVER_API_STUB_H
#define MEDIA_SERVER_API_STUB_H

#include "media_server/http/router.h"

/*
 * Shared 501 stub for routes registered but not implemented yet.
 * Playback controls (play/pause/stop/seek) are intentionally omitted: clients
 * own those; seek is HTTP Range on GET /stream/:id.
 */
void handle_not_implemented(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_STUB_H */
