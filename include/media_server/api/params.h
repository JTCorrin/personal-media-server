#ifndef MEDIA_SERVER_API_PARAMS_H
#define MEDIA_SERVER_API_PARAMS_H

#include "media_server/http/router.h"

#include <stdint.h>

/* Parse the ":id" path param as a non-zero uint32. Returns 0 on success. */
int api_parse_id_param(const router_match_t *match, uint32_t *out_id);

#endif /* MEDIA_SERVER_API_PARAMS_H */
