#ifndef MEDIA_SERVER_UTIL_STR_H
#define MEDIA_SERVER_UTIL_STR_H

#include <stdbool.h>
#include <stddef.h>

/* Case-insensitive substring search. Empty needle matches any haystack. */
bool str_contains_ci(const char *haystack, const char *needle);

/*
 * Case-insensitive Levenshtein distance. Returns SIZE_MAX if either arg is NULL.
 * Caps work: if |len(a)-len(b)| > max_dist early-out as max_dist+1 when max_dist
 * is passed via the fuzzy helper; this function computes full distance.
 */
size_t str_edit_distance_ci(const char *a, const char *b);

/*
 * True if contains_ci, or (when fuzzy) edit distance <= max_dist on the full
 * strings or on a window of haystack near needle length.
 * Empty needle matches. max_dist of 0 is equivalent to contains_ci only for
 * non-empty (still allows exact via distance 0).
 */
bool str_fuzzy_ci(const char *haystack, const char *needle, size_t max_dist);

/*
 * Relevance score for ranking search hits. Higher is better; 0 means no match.
 * Exact / prefix / substring outrank fuzzy-only window matches.
 * Empty needle scores 1. NULL args score 0.
 */
int str_match_score_ci(const char *haystack, const char *needle, bool fuzzy);

#endif /* MEDIA_SERVER_UTIL_STR_H */
