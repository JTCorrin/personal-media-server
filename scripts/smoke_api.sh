#!/usr/bin/env bash
# Smoke-test live API routes vs 501 stubs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-18083}"
LISTEN="http://127.0.0.1:${PORT}"
LIBRARY="${ROOT}/tests/fixtures/library"
BIN="${ROOT}/media-server"
LOG="$(mktemp)"
PID=""

cleanup() {
  if [[ -n "${PID}" ]] && kill -0 "${PID}" 2>/dev/null; then
    kill "${PID}" 2>/dev/null || true
    wait "${PID}" 2>/dev/null || true
  fi
  rm -f "${LOG}"
}
trap cleanup EXIT

cd "${ROOT}"
make media-server >/dev/null

"${BIN}" --listen "${LISTEN}" --library-dir "${LIBRARY}" --log-level info >"${LOG}" 2>&1 &
PID=$!

for _ in $(seq 1 50); do
  if curl -sf "${LISTEN}/api/ping" >/dev/null 2>&1; then
    break
  fi
  if ! kill -0 "${PID}" 2>/dev/null; then
    echo "server exited early; log:" >&2
    cat "${LOG}" >&2
    exit 1
  fi
  sleep 0.1
done

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
album_id="$(printf '%s' "${albums}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
expect_status GET "/api/albums/${album_id}" 200
album_tracks="$(curl -sf "${LISTEN}/api/albums/${album_id}/tracks")"
if ! printf '%s' "${album_tracks}" | grep -q 'track01.mp3'; then
  echo "FAIL /api/albums/${album_id}/tracks missing track01: ${album_tracks}" >&2
  exit 1
fi
echo "OK GET /api/albums/${album_id}/tracks"

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

echo "==> stub routes (501)"
expect_status POST /api/library/scan 501
expect_status GET /api/library/status 501
expect_status GET /api/playlists 501

echo "smoke api: all checks passed"
