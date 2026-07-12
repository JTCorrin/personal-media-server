#!/usr/bin/env bash
# Smoke-test GET /stream/:id and GET /cover/:id
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-18082}"
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

tracks="$(curl -sf "${LISTEN}/api/tracks")"
images="$(curl -sf "${LISTEN}/api/images")"
audio_id="$(printf '%s' "${tracks}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"
image_id="$(printf '%s' "${images}" | sed -n 's/.*"id":\([0-9]*\).*/\1/p' | head -1)"

if [[ -z "${audio_id}" || -z "${image_id}" ]]; then
  echo "failed to resolve audio/image ids from tracks=${tracks} images=${images}" >&2
  exit 1
fi

echo "==> GET /stream/${audio_id}"
headers="$(mktemp)"
curl -sf -D "${headers}" -o /tmp/media-server-stream.out "${LISTEN}/stream/${audio_id}"
grep -qiE '^HTTP/.* 200' "${headers}"
grep -qiE '^Content-Type: *audio/' "${headers}"
echo "OK stream ${audio_id}"
rm -f "${headers}"

echo "==> GET /cover/${image_id}"
headers="$(mktemp)"
curl -sf -D "${headers}" -o /tmp/media-server-cover.out "${LISTEN}/cover/${image_id}"
grep -qiE '^HTTP/.* 200' "${headers}"
grep -qiE '^Content-Type: *image/' "${headers}"
echo "OK cover ${image_id}"
rm -f "${headers}"

echo "==> GET /stream/999 -> 404"
missing="$(curl -s -o /dev/null -w '%{http_code}' "${LISTEN}/stream/999")"
test "${missing}" = "404"
echo "OK ${missing}"

echo "==> Range GET /stream/${audio_id}"
range_headers="$(mktemp)"
curl -s -H 'Range: bytes=0-1023' -D "${range_headers}" -o /tmp/media-server-range.out "${LISTEN}/stream/${audio_id}"
grep -qiE '^HTTP/.* 206' "${range_headers}"
grep -qiE '^Content-Range: *bytes 0-1023/' "${range_headers}"
test "$(wc -c </tmp/media-server-range.out)" -eq 1024
echo "OK 206 Partial Content (1024 bytes)"
rm -f "${range_headers}" /tmp/media-server-range.out

echo "smoke stream: all checks passed"
