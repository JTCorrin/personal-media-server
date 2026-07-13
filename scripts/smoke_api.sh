#!/usr/bin/env bash
# Smoke-test live API routes vs 501 stubs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-18083}"
LISTEN="http://127.0.0.1:${PORT}"
LIBRARY="${ROOT}/tests/fixtures/library"
BIN="${ROOT}/media-server"
LOG="$(mktemp)"
CATALOG_DB="$(mktemp --suffix=.db)"
USER_DB="$(mktemp --suffix=.db)"
PID=""

cleanup() {
  rm -f "${LIBRARY}/.media_server_scan_hold" 2>/dev/null || true
  if [[ -n "${PID}" ]] && kill -0 "${PID}" 2>/dev/null; then
    kill "${PID}" 2>/dev/null || true
    wait "${PID}" 2>/dev/null || true
  fi
  rm -f "${LOG}" "${CATALOG_DB}" "${USER_DB}"
}
trap cleanup EXIT

cd "${ROOT}"
make media-server >/dev/null

start_server() {
  "${BIN}" --listen "${LISTEN}" --library-dir "${LIBRARY}" \
    --catalog-db "${CATALOG_DB}" --user-db "${USER_DB}" --log-level info >"${LOG}" 2>&1 &
  PID=$!

  for _ in $(seq 1 50); do
    if curl -sf "${LISTEN}/api/ping" >/dev/null 2>&1; then
      return 0
    fi
    if ! kill -0 "${PID}" 2>/dev/null; then
      echo "server exited early; log:" >&2
      cat "${LOG}" >&2
      exit 1
    fi
    sleep 0.1
  done
  echo "server did not become ready" >&2
  exit 1
}

stop_server() {
  if [[ -n "${PID}" ]] && kill -0 "${PID}" 2>/dev/null; then
    kill "${PID}" 2>/dev/null || true
    wait "${PID}" 2>/dev/null || true
  fi
  PID=""
}

start_server

expect_status() {
  local method="$1"
  local path="$2"
  local want="$3"
  local got

  got="$(curl -s -o /dev/null -w '%{http_code}' -X "${method}" "${LISTEN}${path}")"
  if [[ "${got}" != "${want}" ]]; then
    echo "FAIL ${method} ${path}: expected ${want}, got ${got}" >&2
    exit 1
  fi
  echo "OK ${method} ${path} -> ${got}"
}

echo "==> live routes"
expect_status GET /api/ping 200

tracks="$(curl -sf "${LISTEN}/api/tracks")"
if ! printf '%s' "${tracks}" | grep -q '"items"'; then
  echo "FAIL /api/tracks missing items envelope: ${tracks}" >&2
  exit 1
fi
if ! printf '%s' "${tracks}" | grep -q '"artist"'; then
  echo "FAIL /api/tracks missing artist field: ${tracks}" >&2
  exit 1
fi
echo "OK GET /api/tracks envelope"

audio_id="$(printf '%s' "${tracks}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
if [[ -z "${audio_id}" ]]; then
  echo "FAIL could not parse audio id from: ${tracks}" >&2
  exit 1
fi
expect_status GET "/api/tracks/${audio_id}" 200
expect_status GET /api/tracks/999 404

paged="$(curl -sf "${LISTEN}/api/tracks?limit=1&offset=0")"
if ! printf '%s' "${paged}" | grep -q '"limit":1'; then
  echo "FAIL pagination limit missing: ${paged}" >&2
  exit 1
fi
echo "OK GET /api/tracks?limit=1"

images="$(curl -sf "${LISTEN}/api/images")"
if ! printf '%s' "${images}" | grep -q '"kind":"image"'; then
  echo "FAIL /api/images expected image items: ${images}" >&2
  exit 1
fi
echo "OK GET /api/images -> 200"

artists="$(curl -sf "${LISTEN}/api/artists")"
if ! printf '%s' "${artists}" | grep -q '"name":"Artist"'; then
  echo "FAIL /api/artists expected Artist: ${artists}" >&2
  exit 1
fi
artist_id="$(printf '%s' "${artists}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
expect_status GET "/api/artists/${artist_id}" 200
expect_status GET "/api/artists/${artist_id}/albums" 200
expect_status GET /api/artists/999 404

albums="$(curl -sf "${LISTEN}/api/albums")"
if ! printf '%s' "${albums}" | grep -q '"name":"Album"'; then
  echo "FAIL /api/albums expected Album: ${albums}" >&2
  exit 1
fi
if ! printf '%s' "${albums}" | grep -qE '"cover_id":[1-9][0-9]*'; then
  echo "FAIL /api/albums expected non-null cover_id: ${albums}" >&2
  exit 1
fi
album_id="$(printf '%s' "${albums}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
cover_id="$(printf '%s' "${albums}" | sed -n 's/.*"cover_id":\([0-9]*\).*/\1/p' | head -1)"
expect_status GET "/api/albums/${album_id}" 200
expect_status GET "/cover/${cover_id}" 200
album_tracks="$(curl -sf "${LISTEN}/api/albums/${album_id}/tracks")"
if ! printf '%s' "${album_tracks}" | grep -q 'track01.mp3'; then
  echo "FAIL /api/albums/${album_id}/tracks missing track01: ${album_tracks}" >&2
  exit 1
fi
echo "OK GET /api/albums/${album_id}/tracks (cover_id=${cover_id})"

search="$(curl -sf "${LISTEN}/api/search?q=Artist")"
if ! printf '%s' "${search}" | grep -q '"tracks"'; then
  echo "FAIL /api/search missing tracks: ${search}" >&2
  exit 1
fi
if ! printf '%s' "${search}" | grep -q '"artists"'; then
  echo "FAIL /api/search missing artists: ${search}" >&2
  exit 1
fi
echo "OK GET /api/search?q=Artist"
expect_status GET /api/search 400

echo "==> library scan/status"
SCAN_BODY="$(mktemp)"
HOLD="${LIBRARY}/.media_server_scan_hold"
status="$(curl -sf "${LISTEN}/api/library/status")"
if ! printf '%s' "${status}" | grep -q '"scanning":false'; then
  echo "FAIL /api/library/status expected scanning false: ${status}" >&2
  exit 1
fi
if ! printf '%s' "${status}" | grep -q '"has_library":true'; then
  echo "FAIL /api/library/status expected has_library: ${status}" >&2
  exit 1
fi
echo "OK GET /api/library/status -> 200"

touch "${HOLD}"
scan="$(curl -s -o "${SCAN_BODY}" -w '%{http_code}' -X POST "${LISTEN}/api/library/scan")"
if [[ "${scan}" != "202" ]]; then
  rm -f "${HOLD}"
  echo "FAIL POST /api/library/scan expected 202, got ${scan}" >&2
  exit 1
fi
if ! grep -q '"status":"started"' "${SCAN_BODY}"; then
  rm -f "${HOLD}"
  echo "FAIL POST /api/library/scan body: $(cat "${SCAN_BODY}")" >&2
  exit 1
fi
echo "OK POST /api/library/scan -> 202 started"

# Wait until status reports scanning (hold keeps the worker parked).
for _ in $(seq 1 100); do
  status="$(curl -sf "${LISTEN}/api/library/status")"
  if printf '%s' "${status}" | grep -q '"scanning":true'; then
    break
  fi
  sleep 0.05
done
if ! printf '%s' "${status}" | grep -q '"scanning":true'; then
  rm -f "${HOLD}"
  echo "FAIL expected scanning true while held: ${status}" >&2
  exit 1
fi

busy="$(curl -s -o "${SCAN_BODY}" -w '%{http_code}' -X POST "${LISTEN}/api/library/scan")"
if [[ "${busy}" != "409" ]]; then
  rm -f "${HOLD}"
  echo "FAIL POST /api/library/scan while busy expected 409, got ${busy}" >&2
  exit 1
fi
if ! grep -q 'scan_in_progress' "${SCAN_BODY}"; then
  rm -f "${HOLD}"
  echo "FAIL busy body: $(cat "${SCAN_BODY}")" >&2
  exit 1
fi
echo "OK POST /api/library/scan -> 409 while in progress"

force="$(curl -s -o "${SCAN_BODY}" -w '%{http_code}' -X POST \
  "${LISTEN}/api/library/scan?force=1")"
if [[ "${force}" != "202" ]]; then
  rm -f "${HOLD}"
  echo "FAIL POST /api/library/scan?force=1 expected 202, got ${force}" >&2
  exit 1
fi
if ! grep -q '"status":"restarted"' "${SCAN_BODY}"; then
  rm -f "${HOLD}"
  echo "FAIL force body: $(cat "${SCAN_BODY}")" >&2
  exit 1
fi
echo "OK POST /api/library/scan?force=1 -> 202 restarted"

rm -f "${HOLD}" "${SCAN_BODY}"

for _ in $(seq 1 100); do
  status="$(curl -sf "${LISTEN}/api/library/status")"
  if printf '%s' "${status}" | grep -q '"scanning":false'; then
    break
  fi
  sleep 0.05
done
if ! printf '%s' "${status}" | grep -q '"scanning":false'; then
  echo "FAIL scan did not finish: ${status}" >&2
  exit 1
fi
if ! printf '%s' "${status}" | grep -q '"last_scan_ok":true'; then
  echo "FAIL expected last_scan_ok: ${status}" >&2
  exit 1
fi
echo "OK library scan completed"

echo "==> catalog-db id stability across restart"
stable_id="${audio_id}"
expect_status GET "/api/tracks/${stable_id}" 200
expect_status GET "/stream/${stable_id}" 200
stop_server
start_server
expect_status GET "/api/tracks/${stable_id}" 200
expect_status GET "/stream/${stable_id}" 200
echo "OK catalog id ${stable_id} stable after restart"

echo "==> playlists / favourites / history / discover / fuzzy"
pl_body="$(mktemp)"
code="$(curl -s -o "${pl_body}" -w '%{http_code}' -X POST \
  -H 'Content-Type: application/json' \
  -d '{"name":"Smoke Mix"}' "${LISTEN}/api/playlists")"
if [[ "${code}" != "201" ]]; then
  echo "FAIL POST /api/playlists expected 201, got ${code}: $(cat "${pl_body}")" >&2
  exit 1
fi
pl_id="$(sed -n 's/.*"id":\([0-9]*\).*/\1/p' "${pl_body}" | head -1)"
if [[ -z "${pl_id}" ]]; then
  echo "FAIL could not parse playlist id: $(cat "${pl_body}")" >&2
  exit 1
fi
echo "OK POST /api/playlists -> 201 id=${pl_id}"

code="$(curl -s -o /dev/null -w '%{http_code}' -X PUT \
  -H 'Content-Type: application/json' \
  -d "{\"track_ids\":[${audio_id}]}" "${LISTEN}/api/playlists/${pl_id}/tracks")"
if [[ "${code}" != "200" ]]; then
  echo "FAIL PUT playlist tracks expected 200, got ${code}" >&2
  exit 1
fi
expect_status GET "/api/playlists/${pl_id}/tracks" 200
expect_status GET /api/playlists 200

expect_status PUT "/api/favourites/${audio_id}" 200
expect_status GET /api/favourites 200
expect_status DELETE "/api/favourites/${audio_id}" 200

code="$(curl -s -o /dev/null -w '%{http_code}' -X POST \
  -H 'Content-Type: application/json' \
  -d "{\"track_id\":${audio_id}}" "${LISTEN}/api/history")"
if [[ "${code}" != "200" ]]; then
  echo "FAIL POST /api/history expected 200, got ${code}" >&2
  exit 1
fi
expect_status GET /api/history 200
expect_status GET /api/discover/random 200
expect_status GET /api/discover/recent 200
expect_status GET /api/discover/recently-played 200

fuzzy="$(curl -sf "${LISTEN}/api/search?q=Artst&fuzzy=1")"
if ! printf '%s' "${fuzzy}" | grep -q '"fuzzy":true'; then
  echo "FAIL fuzzy flag missing: ${fuzzy}" >&2
  exit 1
fi
if ! printf '%s' "${fuzzy}" | grep -q '"artists"'; then
  echo "FAIL fuzzy search missing artists: ${fuzzy}" >&2
  exit 1
fi
echo "OK GET /api/search?q=Artst&fuzzy=1"

code="$(curl -s -o /dev/null -w '%{http_code}' -X DELETE "${LISTEN}/api/playlists/${pl_id}")"
if [[ "${code}" != "200" ]]; then
  echo "FAIL DELETE playlist expected 200, got ${code}" >&2
  exit 1
fi
rm -f "${pl_body}"
echo "OK playlists/favourites/history/discover/fuzzy"

echo "smoke api: all checks passed"
