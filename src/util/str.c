#include "media_server/util/str.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
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

static int ci_eq(char a, char b)
{
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

size_t str_edit_distance_ci(const char *a, const char *b)
{
    size_t n;
    size_t m;
    size_t *prev;
    size_t *cur;
    size_t result;

    if (a == NULL || b == NULL) {
        return SIZE_MAX;
    }

    n = strlen(a);
    m = strlen(b);
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }

    /* Two-row DP; keep costs for previous and current needle index. */
    prev = calloc(m + 1, sizeof(*prev));
    cur = calloc(m + 1, sizeof(*cur));
    if (prev == NULL || cur == NULL) {
        free(prev);
        free(cur);
        return SIZE_MAX;
    }

    for (size_t j = 0; j <= m; j++) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= n; i++) {
        cur[0] = i;
        for (size_t j = 1; j <= m; j++) {
            size_t cost = ci_eq(a[i - 1], b[j - 1]) ? 0 : 1;
            size_t del = prev[j] + 1;
            size_t ins = cur[j - 1] + 1;
            size_t sub = prev[j - 1] + cost;
            size_t best = del < ins ? del : ins;
            if (sub < best) {
                best = sub;
            }
            cur[j] = best;
        }
        size_t *tmp = prev;
        prev = cur;
        cur = tmp;
    }

    result = prev[m];
    free(prev);
    free(cur);
    return result;
}

bool str_fuzzy_ci(const char *haystack, const char *needle, size_t max_dist)
{
    size_t nlen;
    size_t hlen;
    size_t wmin;
    size_t wmax;

    if (haystack == NULL || needle == NULL) {
        return false;
    }

    if (str_contains_ci(haystack, needle)) {
        return true;
    }

    nlen = strlen(needle);
    hlen = strlen(haystack);
    if (nlen == 0) {
        return true;
    }

    if (str_edit_distance_ci(haystack, needle) <= max_dist) {
        return true;
    }

    /* Window search for short typo needles inside longer haystacks. */
    if (nlen < 2 || max_dist == 0) {
        return false;
    }

    wmin = nlen > max_dist ? nlen - max_dist : 1;
    wmax = nlen + max_dist;
    if (wmax > hlen) {
        wmax = hlen;
    }

    for (size_t w = wmin; w <= wmax; w++) {
        if (w > hlen) {
            break;
        }
        for (size_t i = 0; i + w <= hlen; i++) {
            char window[256];
            if (w >= sizeof(window)) {
                continue;
            }
            memcpy(window, haystack + i, w);
            window[w] = '\0';
            if (str_edit_distance_ci(window, needle) <= max_dist) {
                return true;
            }
        }
    }

    return false;
}

static bool is_word_boundary_before(const char *haystack, size_t pos)
{
    unsigned char c;

    if (pos == 0) {
        return true;
    }
    c = (unsigned char)haystack[pos - 1];
    return !isalnum(c);
}

/* First case-insensitive occurrence of needle; SIZE_MAX if none. */
static size_t find_contains_ci(const char *haystack, const char *needle, size_t nlen,
                               size_t hlen)
{
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[i + j]) ==
                   tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) {
            return i;
        }
    }
    return SIZE_MAX;
}

int str_match_score_ci(const char *haystack, const char *needle, bool fuzzy)
{
    size_t nlen;
    size_t hlen;
    size_t pos;
    size_t dist;
    size_t wmin;
    size_t wmax;
    size_t best_dist;
    size_t best_pos;
    int hpen;

    if (haystack == NULL || needle == NULL) {
        return 0;
    }

    nlen = strlen(needle);
    hlen = strlen(haystack);
    if (nlen == 0) {
        return 1;
    }
    if (hlen == 0) {
        return 0;
    }

    hpen = (int)(hlen < 999 ? hlen : 999);

    /* Exact equality. */
    if (hlen == nlen) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[j]) ==
                   tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) {
            return 1000000;
        }
    }

    pos = find_contains_ci(haystack, needle, nlen, hlen);
    if (pos != SIZE_MAX) {
        if (pos == 0) {
            /* Prefix (and not exact — already handled). */
            return 900000 - hpen;
        }
        if (is_word_boundary_before(haystack, pos)) {
            return 800000 - (int)pos * 100 - hpen;
        }
        return 700000 - (int)pos * 100 - hpen;
    }

    if (!fuzzy || nlen < 2) {
        return 0;
    }

    dist = str_edit_distance_ci(haystack, needle);
    if (dist <= 2) {
        return 300000 - (int)dist * 10000 - hpen;
    }

    /* Window search: prefer closer distance, then earlier position. */
    best_dist = 3;
    best_pos = SIZE_MAX;
    wmin = nlen > 2 ? nlen - 2 : 1;
    wmax = nlen + 2;
    if (wmax > hlen) {
        wmax = hlen;
    }

    for (size_t w = wmin; w <= wmax; w++) {
        if (w > hlen) {
            break;
        }
        for (size_t i = 0; i + w <= hlen; i++) {
            char window[256];
            size_t d;

            if (w >= sizeof(window)) {
                continue;
            }
            memcpy(window, haystack + i, w);
            window[w] = '\0';
            d = str_edit_distance_ci(window, needle);
            if (d <= 2 && (d < best_dist || (d == best_dist && i < best_pos))) {
                best_dist = d;
                best_pos = i;
            }
        }
    }

    if (best_dist > 2) {
        return 0;
    }

    return 200000 - (int)best_dist * 10000 - (int)best_pos * 10 - hpen;
}
