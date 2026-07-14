# personal-media-server

A lightweight media server written in C. It scans a directory of audio
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
- TagLib C development headers and pkg-config metadata (e.g. `taglib` on Arch,
  `libtagc0-dev` on Debian/Ubuntu)
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

### Client agent brief

[`CLIENT_AGENTS.md`](CLIENT_AGENTS.md) is a short, copy-pasteable overview of
the server and HTTP endpoints for AI agents (or humans) building a frontend
client. Point the client at a base URL such as `http://127.0.0.1:8080` or a
LAN IP; no auth is required. Prefer that file over scraping this README when
bootstrapping a separate client repo.

### Test playground UI

`tests/fixtures/web/index.html` is a static page (Tailwind CDN) for manual API
testing. It is not served by the media server — open the file in a browser (or
any static file host) while the server is running on `http://127.0.0.1:8080`.
The page defaults to that base URL (editable in the header).

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
| PATCH  | `/api/tracks/:id`         | Override track metadata                        |
| GET    | `/api/images`             | Cover images (paginated)                       |
| GET    | `/api/images/:id`         | One image                                      |
| GET    | `/api/artists`            | Artists (paginated)                            |
| GET    | `/api/artists/:id`        | One artist                                     |
| GET    | `/api/artists/:id/albums` | Albums by an artist (paginated)                |
| GET    | `/api/albums`             | Albums (paginated)                             |
| GET    | `/api/albums/:id`         | One album                                      |
| PATCH  | `/api/albums/:id`         | Override metadata for every track in album     |
| PUT    | `/api/albums/:id/cover`   | Upload and immediately index a cover image     |
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

Audio metadata is read from embedded tags first (ID3, Vorbis comments, MP4,
RIFF, and the other formats supported by TagLib). Missing fields fall back to
the folder layout and filename. Common untagged names such as
`Artist - Album - 03 - Track title.flac`, `03 - Track title.flac`, and
`03 Track title.flac` become title `Track title` and track number `3`.
Tracks also expose optional release date, genre, track/disc numbers, and which
fields are overridden:

```json
{"id":1,"kind":"audio","path":"Artist/Album/track01.mp3","filename":"track01.mp3",
 "artist":"Artist","album":"Album","title":"track01","release_date":null,
 "genre":null,"track_number":null,"disc_number":null,"album_id":1,"cover_id":3,
 "overridden_fields":[]}
```

Every audio Track object includes nullable `album_id` and `cover_id`, including
tracks nested in search, discover, playlists, favourites, history, and album
track responses. This lets clients navigate to the album and render
`/cover/:cover_id` without additional album lookups.

### Metadata overrides

`PATCH /api/tracks/:id` accepts any subset of:

```json
{
  "title": "Correct title",
  "artist": "Correct artist",
  "album": "Correct album",
  "release_date": "2024-03-02",
  "genre": "Rock",
  "track_number": 3,
  "disc_number": 1
}
```

Dates accept `YYYY`, `YYYY-MM`, or `YYYY-MM-DD`. Track and disc numbers must
be between 1 and 65535. Omitted fields are unchanged; setting a field to
`null` removes its override and restores the scanned value. Empty strings are
valid overrides for optional text fields, while `title` must be non-empty.

`PATCH /api/albums/:id` accepts `name`, `artist`, `release_date`, and `genre`.
It resolves the current synthetic album to its tracks and updates all of them
transactionally. The response is `{"updated_track_count":N}`; refetch albums
afterward because changing artist/name can regroup albums and change synthetic
album IDs.

`PUT /api/albums/:id/cover` accepts raw image bytes with
`Content-Type: image/jpeg`, `image/png`, or `image/webp` (max 10 MiB). The
server writes `cover.<ext>` next to the album's tracks (never accepts a client
path) and updates the catalog immediately without a full library rescan. For
multi-disc layouts whose tracks live under a shared parent (e.g.
`Artist/Album/CD1` and `Artist/Album/CD2`), the cover is written in that common
parent (`Artist/Album/cover.jpg`). If owned tracks share no common directory,
the response is `400 {"error":"ambiguous_album_dir"}`. A successful response is
`200 {"ok":true,"path":"Artist/Album/cover.jpg","cover_id":123}`; the returned
id can be used at `/cover/123` immediately. This endpoint writes into
`--library-dir`; like the rest of the API there is no authentication.

Overrides have precedence over embedded tags and filename/path metadata. With
`--catalog-db`, they
survive rescans and restarts in a path-keyed `metadata_overrides` table.
Without `--catalog-db`, updates are in-memory only and disappear on the next
rescan or restart. PATCH never writes embedded tags and never renames or moves
media files.

Catalog item IDs are assigned during the scan starting at 1 and are reused by
relative path when `--catalog-db` is set (stable across restarts and rescans).
Artist and album IDs are synthetic (discovery order) and not persisted.

Album JSON includes `cover_id` (catalog image id, or `null`) matched by
artist/album path meta, preferring `cover.*` / `folder.*` / `front.*`.
Track JSON exposes that same image id as `cover_id`; its `album_id` is the
current synthetic album id and must be refetched after metadata regrouping.

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

The server canonicalizes `--library-dir` before using or persisting it. At
startup it loads a valid prior snapshot from `--catalog-db` for fast service,
then schedules a background scan to reconcile files changed while it was
stopped. A missing, corrupt, incompatible, or wrong-root snapshot is discarded
atomically and replaced by a full scan.

`POST /api/library/scan` runs a background full rescan, reuses item IDs by
path, rebuilds the browse index, and saves the snapshot when `--catalog-db`
is set. The in-memory catalog is swapped only after the scan, browse-index
build, and configured snapshot save all succeed.

- Files are classified by extension (case-insensitive); everything else is
  ignored.
- Symlinks are skipped entirely, so links cannot expose files outside the
  library root or create scan loops.
- Unreadable subdirectories are logged and skipped; only an unreadable root
  is a fatal error.
- Nesting is capped at 32 levels.

Catalog and browse-index generations are immutable after construction.
Background rescans build a new generation and atomically swap it under a
mutex, so requests see either the old complete generation or the new complete
generation.

Playlist, favourite, and history rows intentionally live in a separate user
database. Entries referencing tracks removed by a rescan are retained but
omitted from expanded API responses; this preserves history if the track later
returns, but can make persisted counts larger than the number of currently
available items.

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
  `PUT /api/albums/:id/cover` can write image files under `--library-dir`.
- Clients can only reference media by numeric ID; file paths are never
  accepted from the network, and resolved paths reject absolute paths and
  `..` segments as defense in depth.
- Filenames are JSON-escaped in API responses, and control characters in
  request data are stripped before logging.

## Author notes

This project was started as I wanted to learn more C and I wanted to do that using the mental model I have from working with python and node and other such higher level languages. I particularly wanted to maintain a resemblance to the pattern `request -> route -> controller -> services/utils -> response` as this was how I usually write an API.

AI was integral to the building of this server. I used it to guide me and explain things to me. There are still things here I am yet to wrap my head around but ultimately this has been a lot of fun building
