#ifndef MEDIA_SERVER_UTIL_STR_H
#define MEDIA_SERVER_UTIL_STR_H

#include <stdbool.h>

/* Case-insensitive substring search. Empty needle matches any haystack. */
bool str_contains_ci(const char *haystack, const char *needle);

#endif /* MEDIA_SERVER_UTIL_STR_H */
