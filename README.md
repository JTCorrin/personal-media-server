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
- JSON track listing with correctly escaped filenames
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
  --log-level debug \
  --log-file /var/log/media-server.log \
  --log-db /var/lib/media-server/logs.db
```

Shut down cleanly with `Ctrl-C` or `SIGTERM`.

## HTTP API

### Implemented

| Method | Path          | Description                                    |
|--------|---------------|------------------------------------------------|
| GET    | `/api/ping`   | Health check; returns `{"ok":true}`            |
| GET    | `/api/tracks` | All catalog items (audio and images) as JSON   |
| GET    | `/stream/:id` | Stream an audio item; supports `Range` headers |
| GET    | `/cover/:id`  | Serve an image item                            |

`/api/tracks` returns an array of catalog items. IDs are assigned during the
scan, starting at 1:

```json
[
  {"id":1,"kind":"audio","path":"Artist/Album/track01.mp3","filename":"track01.mp3"},
  {"id":2,"kind":"image","path":"Artist/Album/cover.jpg","filename":"cover.jpg"}
]
```

Unknown IDs, IDs of the wrong kind (e.g. streaming an image), and unmatched
paths all return `404` with a JSON error body.

### Planned (return `501 Not Implemented`)

Browse/metadata (`/api/tracks/:id`, `/api/artists...`, `/api/albums...`,
`/api/images...`), search (`/api/search`), library management
(`POST /api/library/scan`, `/api/library/status`), and playlists
(`/api/playlists...`) are registered as stubs and respond with `501` until
implemented.

## Library scanning

The scan runs once at startup, walking `--library-dir` recursively:

- Files are classified by extension (case-insensitive); everything else is
  ignored.
- Symlinks are skipped entirely, so links cannot expose files outside the
  library root or create scan loops.
- Unreadable subdirectories are logged and skipped; only an unreadable root
  is a fatal error.
- Nesting is capped at 32 levels.

Rescanning currently requires a restart (`POST /api/library/scan` is a
planned endpoint).

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
