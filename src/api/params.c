#include "media_server/api/params.h"

#include <errno.h>
#include <stdlib.h>

int api_parse_id_param(const router_match_t *match, uint32_t *out_id)
{
    const char *raw;
    char *end = NULL;
    unsigned long long value;

    if (match == NULL || out_id == NULL) {
        return -1;
    }

    raw = router_param_get(match, "id");
    if (raw == NULL || raw[0] == '\0') {
        return -1;
    }

    /*
     * strtoull, not strtoul: unsigned long may be 32-bit, which would make
     * the range check below a no-op on such platforms.
     */
    errno = 0;
    value = strtoull(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0' || value == 0 || value > UINT32_MAX) {
        return -1;
    }

    *out_id = (uint32_t)value;
    return 0;
}
