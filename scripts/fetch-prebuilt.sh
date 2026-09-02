#!/bin/bash
set -euo pipefail

# =======================================================================================
# Downloads the prebuilt vendored dependencies (libwebrtc, libmediasoupclient,
# libsdptransform, ONNX Runtime libs and headers) for the current platform from
# GitHub Releases, instead of building them from source (make vendor-deps, 2-4 hours).
#
# Usage: ./scripts/fetch-prebuilt.sh   (or: make fetch-prebuilt)
# Override the release tag with: PREBUILT_TAG=<tag> ./scripts/fetch-prebuilt.sh
# =======================================================================================

readonly SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "${SCRIPTS_DIR}/.." && pwd)"

readonly REPO="saasybyte/saasy-media-engine"
readonly TAG="${PREBUILT_TAG:-prebuilt-v1}"

case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)  PLATFORM="darwin-arm64" ;;
  Linux-x86_64)  PLATFORM="linux-x64" ;;
  *)
    echo "[ERROR] Unsupported platform: $(uname -s) $(uname -m) (supported: macOS arm64, Linux x64)" >&2
    exit 1
    ;;
esac

readonly DEST="${PROJECT_ROOT}/third_party/prebuilt/${PLATFORM}"

if [ -d "${DEST}" ]; then
  echo "[INFO] ${DEST} already exists. Delete it to re-fetch."
  exit 0
fi

readonly ASSET="prebuilt-${PLATFORM}.tar.gz"
readonly URL="https://github.com/${REPO}/releases/download/${TAG}/${ASSET}"

tmpfile="$(mktemp "${TMPDIR:-/tmp}/${ASSET}.XXXXXX")"
trap 'rm -f "${tmpfile}"' EXIT

echo "[INFO] Downloading ${URL}"
curl -fL --progress-bar "${URL}" -o "${tmpfile}"

mkdir -p "${PROJECT_ROOT}/third_party/prebuilt"
echo "[INFO] Extracting to ${DEST}"
tar -xzf "${tmpfile}" -C "${PROJECT_ROOT}/third_party/prebuilt"

if [ ! -d "${DEST}/lib" ]; then
  echo "[ERROR] Extraction did not produce ${DEST}/lib. Unexpected archive layout." >&2
  exit 1
fi

echo "[INFO] Done. Vendored libs available at ${DEST}"
cat "${DEST}/vendor-manifest.txt" 2>/dev/null || true
