# Client agent brief — media-server

Copy this file into a frontend/client repo (or paste it into an agent chat) as
the starting contract for talking to a running **media-server** instance.

The client only needs a **base URL** (host + port), e.g.
`http://192.168.5.111:8080` or `http://127.0.0.1:8080`. All paths below are
relative to that base. There is no auth and no API key.

For fuller server-side documentation, see the media-server `README.md`
(HTTP API section). Keep this brief in sync when endpoints change.

---

## What the server is

A lightweight C11 media library HTTP server. It scans a music directory,
exposes JSON browse/search APIs, and streams audio / serves cover art by
numeric ID.

- Default listen: `http://127.0.0.1:8080`
- CORS: `Access-Control-Allow-Origin: *` (browser clients from other origins OK)
- JSON `Content-Type: application/json`
- No TLS / no authentication — treat as a trusted LAN service
- Playlists, favourites, and history require the server to be started with
  `--user-db`; otherwise those routes return `400 {"error":"no_user_db"}`

---

## Client bootstrap

1. Let the user enter a base URL (trim trailing `/`).
2. Probe `GET /api/ping` → `{"ok":true}`.
3. Optionally `GET /api/library/status` for counts / scan state.
4. Build media URLs as:
   - stream: `{base}/stream/{trackId}` (HTML5 `<audio>`, Range seeks work)
   - cover: `{base}/cover/{coverId}` (from album `cover_id`; may be `null`)

---

## Conventions

### Pagination

List endpoints accept `limit` (default **50**, clamped **1–200**) and `offset`.
Response envelope:

```json
{"items":[...],"total":123,"limit":50,"offset":0}
```

`total` is the unpaginated count.

### Errors

JSON body shape: `{"error":"<code>"}`. Common cases:

| Status | Meaning |
|--------|---------|
| 400 | Bad query/body, or feature needs `--user-db` / library |
| 404 | Unknown id or wrong item kind |
| 409 | Conflict (e.g. scan already running, library busy) |
| 500 | Server encode/update failure |

### IDs

- **Track / image** ids are catalog ids (stable across restarts when the server
  uses `--catalog-db`).
- **Artist / album** ids are synthetic (discovery order). Refetch after album
  or track metadata PATCH — regrouping can change album ids, including each
  Track object's `album_id`.
- A Track's `cover_id` is the album's catalog image id and is stable under the
  same rules as other image ids.
- Never send filesystem paths; only numeric ids.

---

## Core JSON shapes

**Track** (catalog audio item):

```json
{
  "id": 1,
  "kind": "audio",
  "path": "Artist/Album/track01.mp3",
  "filename": "track01.mp3",
  "artist": "Artist",
  "album": "Album",
  "title": "track01",
  "release_date": null,
  "genre": null,
  "track_number": null,
  "disc_number": null,
  "album_id": 1,
  "cover_id": 3,
  "overridden_fields": []
}
```

`album_id` and `cover_id` are present on every Track response and may be
`null`. They are enriched consistently in track lists/details, search,
discover, playlists, favourites, history, and album-track responses.

**Artist:**

```json
{"id":1,"name":"Artist","album_count":2,"track_count":10}
```

**Album:**

```json
{
  "id": 1,
  "name": "Album",
  "artist": "Artist",
  "artist_id": 1,
  "track_count": 10,
  "release_date": null,
  "genre": null,
  "cover_id": 3
}
```

**Playlist:**

```json
{
  "id": 1,
  "name": "Mix",
  "track_count": 12,
  "created_unix": 1710000000,
  "updated_unix": 1710000000
}
```

**History item:** `{"track":{...Track...},"played_unix":1710000000}`

---

## Endpoints

### Health / library

| Method | Path | Notes |
|--------|------|-------|
| GET | `/api/ping` | `{"ok":true}` |
| GET | `/api/library/status` | `scanning`, counts, `last_error`, etc. |
| POST | `/api/library/scan` | Starts background rescan → `202`. `?force=1` restarts. |

### Catalog browse

| Method | Path | Notes |
|--------|------|-------|
| GET | `/api/tracks` | Paginated tracks |
| GET | `/api/tracks/:id` | One track |
| PATCH | `/api/tracks/:id` | Metadata override (JSON subset; see below) |
| GET | `/api/images` | Paginated images |
| GET | `/api/images/:id` | One image |
| GET | `/api/artists` | Paginated artists |
| GET | `/api/artists/:id` | One artist |
| GET | `/api/artists/:id/albums` | Albums for artist |
| GET | `/api/albums` | Paginated albums |
| GET | `/api/albums/:id` | One album |
| PATCH | `/api/albums/:id` | Override all tracks in album |
| PUT | `/api/albums/:id/cover` | Upload and immediately index a cover |
| GET | `/api/albums/:id/tracks` | Tracks on album |

### Search / discover

| Method | Path | Notes |
|--------|------|-------|
| GET | `/api/search?q=<text>` | Required `q`. Optional `fuzzy=1`. Returns `{q,fuzzy,tracks,artists,albums}` each a page envelope. |
| GET | `/api/discover/random` | Random tracks (paginated) |
| GET | `/api/discover/recent` | Newest catalog tracks |
| GET | `/api/discover/recently-played` | From play history (needs user DB) |

### User data (requires `--user-db`)

| Method | Path | Notes |
|--------|------|-------|
| GET | `/api/playlists` | List playlists |
| POST | `/api/playlists` | Body `{"name":"..."}` → `201` |
| GET | `/api/playlists/:id` | One playlist |
| PATCH | `/api/playlists/:id` | Rename `{"name":"..."}` |
| DELETE | `/api/playlists/:id` | Delete |
| GET | `/api/playlists/:id/tracks` | Playlist tracks |
| PUT | `/api/playlists/:id/tracks` | Replace `{"track_ids":[1,2,...]}` |
| GET | `/api/favourites` | Favourited tracks |
| PUT | `/api/favourites/:id` | Favourite track id |
| DELETE | `/api/favourites/:id` | Unfavourite |
| GET | `/api/history` | Recently played (`track` + `played_unix`) |
| POST | `/api/history` | Record play `{"track_id":N}` |

### Media (binary)

| Method | Path | Notes |
|--------|------|-------|
| GET | `/stream/:id` | Audio bytes; supports HTTP `Range` |
| GET | `/cover/:id` | Image bytes (use album `cover_id`) |

---

## Metadata PATCH bodies

`PATCH /api/tracks/:id` — any subset of:

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

- Dates: `YYYY`, `YYYY-MM`, or `YYYY-MM-DD`
- Omitted fields unchanged; `null` clears an override
- Does **not** rewrite files on disk

`PATCH /api/albums/:id` — `name`, `artist`, `release_date`, `genre`. Response:
`{"updated_track_count":N}`. Refetch albums afterward.

`PUT /api/albums/:id/cover` — raw body with `Content-Type: image/jpeg|png|webp`
(max 10 MiB). Writes `cover.<ext>` beside the album tracks (or in the common
parent for multi-disc folders), indexes it without a full scan, and returns
`200 {"ok":true,"path":"...","cover_id":123}`. Use the returned id at
`/cover/123` immediately; no scan polling is needed. Returns
`400 {"error":"ambiguous_album_dir"}` when tracks share no common parent
directory. One-level album folders are supported through physical-directory
matching. Server failures distinguish filesystem writes (`write_failed`),
album relationship updates (`cover_link_failed`), and catalog persistence
(`catalog_save_failed`). No auth — anyone who can reach the server can write
into the library.

---

## Suggested UI surface

Minimum useful client:

1. Connect (base URL + ping)
2. Browse artists → albums → tracks
3. Search
4. Play via `/stream/:id` + show `/cover/:id` when `cover_id` present
5. Optional: favourites, playlists, history, discover, library scan/status

Gracefully degrade when user-db routes return `no_user_db`.
