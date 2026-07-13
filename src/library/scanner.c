#include "media_server/library/scanner.h"

#include "media_server/media/kind.h"
#include "media_server/util/log.h"
#include "media_server/util/path.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*
 * Hard cap on directory nesting. Each recursion level uses ~1.5 KB of stack
 * for the path buffers, and without a cap a symlink cycle or an adversarial
 * deep tree could exhaust the stack.
 */
#define SCANNER_MAX_DEPTH 32

#ifdef MEDIA_SERVER_TEST_SCAN_HOLD
/* Test-only hook: production builds must never let a library file pause scans. */
#define SCANNER_HOLD_NAME ".media_server_scan_hold"
static int wait_for_hold_release(const char *library_root,
                                 const atomic_bool *cancel)
{
    char hold_path[CATALOG_PATH_MAX];
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 5 * 1000 * 1000};

    if (path_join(hold_path, sizeof(hold_path), library_root, SCANNER_HOLD_NAME) !=
        0) {
        return -1;
    }

    while (access(hold_path, F_OK) == 0) {
        if (cancel != NULL && atomic_load(cancel)) {
            return 1;
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}
#endif

static int scan_dir(const char *library_root, const char *rel_dir, catalog_t *catalog,
                    int depth, const atomic_bool *cancel)
{
    char abs_dir[CATALOG_PATH_MAX];
    DIR *dir;
    struct dirent *entry;

    if (cancel != NULL && atomic_load(cancel)) {
        return 1;
    }

    if (depth > SCANNER_MAX_DEPTH) {
        LOG_WARN("scanner", "skipping %s: max depth (%d) exceeded", rel_dir,
                 SCANNER_MAX_DEPTH);
        return 0;
    }

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
        int rc;

        if (cancel != NULL && atomic_load(cancel)) {
            closedir(dir);
            return 1;
        }

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

        /*
         * lstat (not stat) so symlinks are seen as symlinks and skipped.
         * Following them would let a link inside the library expose files
         * outside the library root, and a directory link cycle would recurse
         * forever.
         */
        if (lstat(child_abs, &st) != 0) {
            continue;
        }

        if (S_ISLNK(st.st_mode)) {
            LOG_DEBUG("scanner", "skipping symlink %s", child_rel);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            rc = scan_dir(library_root, child_rel, catalog, depth + 1, cancel);
            if (rc == 1) {
                closedir(dir);
                return 1;
            }
            if (rc != 0) {
                LOG_WARN("scanner", "skipping unreadable directory %s", child_rel);
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
            LOG_ERROR("scanner", "failed to add %s", child_rel);
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return 0;
}

int scanner_scan_cancelable(const char *library_root, catalog_t *catalog,
                            const atomic_bool *cancel)
{
    if (library_root == NULL || library_root[0] == '\0' || catalog == NULL) {
        return -1;
    }

#ifdef MEDIA_SERVER_TEST_SCAN_HOLD
    {
        int hold_rc = wait_for_hold_release(library_root, cancel);
        if (hold_rc != 0) {
            return hold_rc;
        }
    }
#endif

    return scan_dir(library_root, "", catalog, 0, cancel);
}

int scanner_scan(const char *library_root, catalog_t *catalog)
{
    return scanner_scan_cancelable(library_root, catalog, NULL);
}
