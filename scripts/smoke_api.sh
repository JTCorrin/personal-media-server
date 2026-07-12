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
if ! printf '%s' "${tracks}" | grep -q '"artist"'; then
  echo "FAIL /api/tracks missing artist field: ${tracks}" >&2
  exit 1
fi
echo "OK GET /api/tracks includes artist"

audio_id="$(printf '%s' "${tracks}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
if [[ -z "${audio_id}" ]]; then
  echo "FAIL could not parse audio id from: ${tracks}" >&2
  exit 1
fi
expect_status GET "/api/tracks/${audio_id}" 200
expect_status GET /api/tracks/999 404

images="$(curl -sf "${LISTEN}/api/images")"
if ! printf '%s' "${images}" | grep -q '"kind":"image"'; then
  echo "FAIL /api/images expected image items: ${images}" >&2
  exit 1
fi
echo "OK GET /api/images -> 200"

echo "==> stub routes (501)"
expect_status GET /api/artists 501
expect_status GET /api/albums 501
expect_status GET /api/search 501
expect_status POST /api/library/scan 501
expect_status GET /api/library/status 501
expect_status GET /api/playlists 501

echo "smoke api: all checks passed"
