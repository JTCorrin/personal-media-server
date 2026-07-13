#ifndef MEDIA_SERVER_API_LIBRARY_H
#define MEDIA_SERVER_API_LIBRARY_H

#include "media_server/http/router.h"

void handle_library_status(const router_match_t *match, void *req, void *res);
void handle_library_scan(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_LIBRARY_H */
