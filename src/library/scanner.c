#include "media_server/library/scanner.h"

#include "media_server/media/kind.h"
#include "media_server/util/log.h"
#include "media_server/util/path.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static int scan_dir(const char *library_root, const char *rel_dir, catalog_t *catalog)
{
    char abs_dir[CATALOG_PATH_MAX];
    DIR *dir;
    struct dirent *entry;

    if (rel_dir[0] == '\0') {
        if (strlen(library_root) >= sizeof(abs_dir)) {
            return -1;
        }
        memcpy(abs_dir, library_root, strlen(library_root) + 1);
    } else if (path_join(abs_dir, sizeof(abs_dir), library_root, rel_dir) != 0) {
        return -1;
    }

    dir = opendir(abs_dir);
    if (dir == NULL) {
        LOG_ERROR("scanner", "opendir(%s): %s", abs_dir, strerror(errno));
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child_rel[CATALOG_PATH_MAX];
        char child_abs[CATALOG_PATH_MAX];
        struct stat st;
        media_kind_t kind;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (rel_dir[0] == '\0') {
            if (strlen(entry->d_name) >= sizeof(child_rel)) {
                continue;
            }
            memcpy(child_rel, entry->d_name, strlen(entry->d_name) + 1);
        } else if (path_join(child_rel, sizeof(child_rel), rel_dir, entry->d_name) !=
                   0) {
            continue;
        }

        if (path_join(child_abs, sizeof(child_abs), library_root, child_rel) != 0) {
            continue;
        }

        if (stat(child_abs, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (scan_dir(library_root, child_rel, catalog) != 0) {
                closedir(dir);
                return -1;
            }
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        kind = media_kind_from_path(child_rel);
        if (kind == MEDIA_KIND_NONE) {
            continue;
        }

        if (catalog_add(catalog, kind, child_rel) != 0) {
            LOG_WARN("scanner", "failed to add %s", child_rel);
        }
    }

    closedir(dir);
    return 0;
}

int scanner_scan(const char *library_root, catalog_t *catalog)
{
    if (library_root == NULL || library_root[0] == '\0' || catalog == NULL) {
        return -1;
    }

    return scan_dir(library_root, "", catalog);
}
