#!/usr/bin/env bash
# Smoke-test library scan + GET /api/tracks with curl.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-18081}"
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

echo "==> GET /api/tracks"
tracks="$(curl -sf "${LISTEN}/api/tracks")"
status="$(curl -sf -o /dev/null -w '%{http_code}' "${LISTEN}/api/tracks")"
test "${status}" = "200"
echo "${tracks}" | grep -q '"items"'
echo "${tracks}" | grep -q 'track01.mp3'
echo "${tracks}" | grep -q '"kind":"audio"'
echo "${tracks}" | grep -q '"artist"'
echo "${tracks}" | grep -q '"total"'
echo "OK tracks ${status}"

echo "==> GET /api/images"
images="$(curl -sf "${LISTEN}/api/images")"
img_status="$(curl -sf -o /dev/null -w '%{http_code}' "${LISTEN}/api/images")"
test "${img_status}" = "200"
echo "${images}" | grep -q '"items"'
echo "${images}" | grep -q 'cover.jpg'
echo "${images}" | grep -q '"kind":"image"'
echo "OK images ${img_status}"

echo "smoke library: all checks passed"
