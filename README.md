# media-server

A lightweight media server written in C11. It scans a directory of audio
files and cover art, builds an in-memory catalog, and serves it over HTTP —
including range-request streaming, so any browser, `mpv`, or VLC can seek
within a track.

Built on a small vendored [Mongoose](https://mongoose.ws/) HTTP core with no
other runtime dependencies beyond SQLite (used for optional structured
logging).

## Features

- Recursive library scan with symlink and path-traversal protection
- Artist/album browsing derived from the `Artist/Album/track` folder layout
- Case-insensitive search across titles, artists, albums, and paths
- Paginated JSON listings with correctly escaped strings
- Audio streaming with HTTP Range support (seek works out of the box)
- Cover art serving with correct MIME types
- Pluggable logging: terminal (colored), file, and/or SQLite sinks
- Single-threaded, event-driven; no configuration files required

Supported formats (by extension):

| Kind  | Extensions                        |
|-------|-----------------------------------|
| Audio | `mp3`, `flac`, `ogg`, `m4a`, `wav` |
| Image | `jpg`, `jpeg`, `png`, `webp`      |

## Requirements

- Linux (POSIX.1-2008), GCC or Clang, GNU Make
- `libsqlite3` development headers (e.g. `sqlite3` on Arch,
  `libsqlite3-dev` on Debian/Ubuntu)
- Vendored third-party sources in `third_party/` (see below)

### Third-party dependencies

Two libraries are vendored as source under `third_party/`:

```
third_party/mongoose/mongoose.c     # https://github.com/cesanta/mongoose (amalgamated)
third_party/mongoose/mongoose.h
third_party/Unity/src/unity.c       # https://github.com/ThrowTheSwitch/Unity (test framework)
third_party/Unity/src/unity.h
third_party/Unity/src/unity_internals.h
```

If your checkout is missing them, drop the amalgamated Mongoose pair and the
Unity `src/` directory into those paths.

## Build and run

```bash
make                                  # builds ./media-server (-O2)
./media-server --library-dir ~/Music
```

The server listens on `http://127.0.0.1:8080` by default. Point a client at
it:

```bash
curl http://127.0.0.1:8080/api/tracks | jq
mpv http://127.0.0.1:8080/stream/1
```

### Options

```
--listen <url>         listen URL (default: http://127.0.0.1:8080)
--library-dir <path>   media library root directory
--catalog-db <path>    sqlite catalog snapshot (stable ids / fast boot)
--user-db <path>       sqlite user data (playlists, favourites, history)
--log-level <name>     trace|debug|info|warn|error|fatal (default: info)
--no-terminal-log      disable stderr logging
--log-file <path>      also append logs to a file
--log-db <path>        also append logs to a sqlite database
--log-db-table <name>  sqlite table name (default: app_logs)
-h, --help             show this help
```

Example with all sinks:

```bash
./media-server \
  --listen http://0.0.0.0:8080 \
  --library-dir /srv/music \
  --catalog-db /var/lib/media-server/catalog.db \
  --user-db /var/lib/media-server/user.db \
  --log-level debug \
  --log-file /var/log/media-server.log \
  --log-db /var/lib/media-server/logs.db
```

Shut down cleanly with `Ctrl-C` or `SIGTERM`.

## HTTP API

### Implemented

| Method | Path                      | Description                                    |
|--------|---------------------------|------------------------------------------------|
| GET    | `/api/ping`               | Health check; returns `{"ok":true}`            |
| GET    | `/api/tracks`             | Audio tracks (paginated)                       |
| GET    | `/api/tracks/:id`         | One track                                      |
| GET    | `/api/images`             | Cover images (paginated)                       |
| GET    | `/api/images/:id`         | One image                                      |
| GET    | `/api/artists`            | Artists (paginated)                            |
| GET    | `/api/artists/:id`        | One artist                                     |
| GET    | `/api/artists/:id/albums` | Albums by an artist (paginated)                |
| GET    | `/api/albums`             | Albums (paginated)                             |
| GET    | `/api/albums/:id`         | One album                                      |
| GET    | `/api/albums/:id/tracks`  | Tracks on an album (paginated)                 |
| GET    | `/api/search?q=<text>`    | Search tracks, artists, and albums             |
| GET    | `/api/library/status`     | Scan status and catalog counts                 |
| POST   | `/api/library/scan`       | Background rescan (`?force=1` to restart)      |
| GET    | `/api/playlists`          | List playlists                                 |
| POST   | `/api/playlists`          | Create playlist `{"name":"..."}`               |
| GET    | `/api/playlists/:id`      | One playlist                                   |
| PATCH  | `/api/playlists/:id`      | Rename `{"name":"..."}`                        |
| DELETE | `/api/playlists/:id`      | Delete playlist                                |
| GET    | `/api/playlists/:id/tracks` | Playlist tracks                              |
| PUT    | `/api/playlists/:id/tracks` | Replace tracks `{"track_ids":[...]}`         |
| GET    | `/api/favourites`         | Favourited tracks                              |
| PUT    | `/api/favourites/:id`     | Favourite a track                              |
| DELETE | `/api/favourites/:id`     | Unfavourite                                    |
| GET    | `/api/history`            | Recently played                                |
| POST   | `/api/history`            | Record play `{"track_id":N}`                   |
| GET    | `/api/discover/random`    | Random audio tracks                            |
| GET    | `/api/discover/recent`    | Newest catalog audio ids                       |
| GET    | `/api/discover/recently-played` | From play history                        |
| GET    | `/stream/:id`             | Stream an audio track; supports `Range` seeks  |
| GET    | `/cover/:id`              | Serve an image item (use album `cover_id`)     |

### Pagination

List endpoints accept `limit` (default 50, clamped to 1–200) and `offset`
query parameters, and wrap results in an envelope with the *unpaginated*
total:

```json
{"items":[...],"total":123,"limit":50,"offset":0}
```

### Items and metadata

Track/image objects carry metadata derived from the folder layout
(`Artist/Album/track01.mp3` → artist, album, title):

```json
{"id":1,"kind":"audio","path":"Artist/Album/track01.mp3","filename":"track01.mp3",
 "artist":"Artist","album":"Album","title":"track01"}
```

Catalog item IDs are assigned during the scan starting at 1 and are reused by
relative path when `--catalog-db` is set (stable across restarts and rescans).
Artist and album IDs are synthetic (discovery order) and not persisted.

Album JSON includes `cover_id` (catalog image id, or `null`) matched by
artist/album path meta, preferring `cover.*` / `folder.*` / `front.*`.

Search accepts optional `fuzzy=1` for typo-tolerant matching (edit distance ≤ 2)
in addition to case-insensitive substring match. Results are ranked by relevance
(exact → prefix → word-boundary substring → substring → fuzzy) before
`limit`/`offset` paging.

Playlists, favourites, and history require `--user-db`. Streaming a track also
appends to play history when a user DB is configured.

Errors: unknown IDs, wrong-kind IDs (e.g. streaming an image), and unmatched
paths return `404`; `/api/search` without `q` returns
`400 {"error":"missing_query"}`, and a `q` longer than 255 bytes (or
undecodable) returns `400 {"error":"invalid_query"}`.

## Library scanning

The scan walks `--library-dir` recursively at startup (or loads a prior
snapshot from `--catalog-db` when present and the library root matches).
`POST /api/library/scan` runs a background full rescan, reuses item IDs by
path, rebuilds the browse index, and saves the snapshot when `--catalog-db`
is set.

- Files are classified by extension (case-insensitive); everything else is
  ignored.
- Symlinks are skipped entirely, so links cannot expose files outside the
  library root or create scan loops.
- Unreadable subdirectories are logged and skipped; only an unreadable root
  is a fatal error.
- Nesting is capped at 32 levels.

After the scan, an artist/album browse index is built once and shared by all
requests (the catalog is immutable while the server runs). Rescanning
currently requires a restart (`POST /api/library/scan` is a planned
endpoint).

## Development

```bash
make test        # build and run all unit test suites (Unity)
make test-asan   # same, rebuilt with AddressSanitizer + UBSan
make smoke       # end-to-end HTTP smoke test against a live server
make smoke-api   # live routes vs 501 stubs
make DEBUG=1     # debug build (-g -O0)
make clean
```

Tests live in `tests/`, one suite per module, each linking only the sources
it exercises (see `UNIT_<name>_SRCS` in the Makefile). Fixtures are under
`tests/fixtures/library/`.

Layout:

```
include/media_server/   public headers (api, http, library, media, util)
src/                    implementation, mirrors the header layout
tests/                  Unity test suites + fixtures
scripts/                smoke tests
third_party/            vendored Mongoose and Unity
```

## Security notes

- Binds to `127.0.0.1` by default. There is no authentication or TLS — if
  you listen on `0.0.0.0`, only do so on a network you trust.
- Clients can only reference media by numeric ID; file paths are never
  accepted from the network, and resolved paths reject absolute paths and
  `..` segments as defense in depth.
- Filenames are JSON-escaped in API responses, and control characters in
  request data are stripped before logging.
