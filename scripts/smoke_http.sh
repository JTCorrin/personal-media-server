#!/usr/bin/env bash
# Smoke-test the HTTP server with curl.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-18080}"
LISTEN="http://127.0.0.1:${PORT}"
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

"${BIN}" --listen "${LISTEN}" --log-level info >"${LOG}" 2>&1 &
PID=$!

# Wait until the server is accepting connections.
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

echo "==> GET /api/ping"
body="$(curl -sf "${LISTEN}/api/ping")"
status="$(curl -sf -o /dev/null -w '%{http_code}' "${LISTEN}/api/ping")"
test "${status}" = "200"
test "${body}" = '{"ok":true}'
echo "OK ${status} ${body}"

echo "==> GET /missing -> 404"
missing_status="$(curl -s -o /tmp/media-server-smoke-404.json -w '%{http_code}' "${LISTEN}/missing")"
missing_body="$(cat /tmp/media-server-smoke-404.json)"
rm -f /tmp/media-server-smoke-404.json
test "${missing_status}" = "404"
echo "OK ${missing_status} ${missing_body}"

echo "smoke http: all checks passed"
