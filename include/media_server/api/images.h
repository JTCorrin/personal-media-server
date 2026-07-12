#ifndef MEDIA_SERVER_API_IMAGES_H
#define MEDIA_SERVER_API_IMAGES_H

#include "media_server/http/router.h"

void handle_images(const router_match_t *match, void *req, void *res);
void handle_image_by_id(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_IMAGES_H */
