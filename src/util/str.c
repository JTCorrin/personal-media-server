#include "media_server/util/str.h"

#include <ctype.h>
#include <string.h>

/*
 * Naive O(haystack * needle) scan. Fine for the short metadata strings this
 * is used on; revisit (or push matching into sqlite) if search ever runs
 * over large text.
 */
bool str_contains_ci(const char *haystack, const char *needle)
{
    size_t nlen;
    size_t hlen;

    if (haystack == NULL || needle == NULL) {
        return false;
    }

    nlen = strlen(needle);
    if (nlen == 0) {
        return true;
    }

    hlen = strlen(haystack);
    if (nlen > hlen) {
        return false;
    }

    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[i + j]) ==
                   tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) {
            return true;
        }
    }

    return false;
}
