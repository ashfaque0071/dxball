#!/bin/sh
# Builds the WebAssembly release of DX Ball with Emscripten and packages it for
# itch.io. See the "Web build" section of README.md for prerequisites.
#
#   ./build_web.sh              build dist/web/ and dist/dxball-web-itch.zip
#   ./build_web.sh --clean      discard the cached raylib build first
#   ./build_web.sh --no-zip     build dist/web/ only
set -eu

cd "$(dirname "$0")"

# --- Pinned dependency ------------------------------------------------------
# raylib 5.5 is the version already vendored for the Windows build in raylib/,
# so the web build is compiled against the same release. The archive is
# verified against this checksum before it is unpacked.
RAYLIB_VERSION=5.5
RAYLIB_SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
RAYLIB_URL="https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz"

DEPS_DIR=build/web-deps
RAYLIB_SRC="${DEPS_DIR}/raylib-${RAYLIB_VERSION}/src"
OUT_DIR=dist/web
ZIP_PATH=dist/dxball-web-itch.zip

clean=0
make_zip=1
for arg in "$@"; do
  case "$arg" in
    --clean)  clean=1 ;;
    --no-zip) make_zip=0 ;;
    -h|--help)
      echo "Usage: ./build_web.sh [--clean] [--no-zip]"
      exit 0 ;;
    *)
      echo "Error: unknown option '$arg'." >&2
      echo "Usage: ./build_web.sh [--clean] [--no-zip]" >&2
      exit 2 ;;
  esac
done

die() { echo "Error: $*" >&2; exit 1; }

# --- Toolchain --------------------------------------------------------------
# emcc is used if it is already on PATH; otherwise the standard emsdk
# environment scripts are sourced from $EMSDK or $HOME/emsdk.
if ! command -v emcc >/dev/null 2>&1; then
  for candidate in "${EMSDK:-}" "$HOME/emsdk" /usr/local/emsdk /opt/emsdk; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/emsdk_env.sh" ]; then
      echo "Activating Emscripten from $candidate"
      EMSDK_QUIET=1
      export EMSDK_QUIET
      # shellcheck disable=SC1090
      . "$candidate/emsdk_env.sh" >/dev/null 2>&1 || true

      # emsdk_env.sh locates itself through $BASH_SOURCE and documents support
      # for bash, zsh and ksh only -- it cannot find itself under dash, which
      # is /bin/sh on Debian and Ubuntu (and therefore on CI runners). Wire the
      # paths up directly when sourcing did not put emcc on PATH.
      if ! command -v emcc >/dev/null 2>&1 && [ -x "$candidate/upstream/emscripten/emcc" ]; then
        EMSDK="$candidate"
        EM_CONFIG="$candidate/.emscripten"
        PATH="$candidate/upstream/emscripten:$candidate:$PATH"
        export EMSDK EM_CONFIG PATH

        # emcc runs on the SDK's bundled node...
        for node_bin in "$candidate"/node/*/bin/node; do
          if [ -x "$node_bin" ]; then
            EMSDK_NODE="$node_bin"
            PATH="$(dirname "$node_bin"):$PATH"
            export EMSDK_NODE PATH
            break
          fi
        done

        # ...and the emcc wrapper invokes whichever python3 is on PATH, which
        # must be 3.10 or newer. Prefer the SDK's bundled interpreter when it
        # ships one, rather than depending on the host's python3.
        for py_bin in "$candidate"/python/*/bin/python3; do
          if [ -x "$py_bin" ]; then
            EMSDK_PYTHON="$py_bin"
            PATH="$(dirname "$py_bin"):$PATH"
            export EMSDK_PYTHON PATH
            break
          fi
        done
      fi
      break
    fi
  done
fi

if ! command -v emcc >/dev/null 2>&1; then
  cat >&2 <<'MSG'
Error: Emscripten (emcc) was not found.

Install and activate the Emscripten SDK, then run this script again:

    git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
    cd ~/emsdk
    ./emsdk install latest
    ./emsdk activate latest
    . ./emsdk_env.sh

This script also finds an SDK automatically at $EMSDK, ~/emsdk, /usr/local/emsdk
or /opt/emsdk, so after installing it you can just run ./build_web.sh again.
MSG
  exit 1
fi

command -v make >/dev/null 2>&1 || die "'make' is required to build raylib. Install the Xcode Command Line Tools (macOS) or build-essential (Linux)."
command -v curl >/dev/null 2>&1 || die "'curl' is required to download the raylib source."

if [ "$make_zip" -eq 1 ] && ! command -v zip >/dev/null 2>&1; then
  die "'zip' is required to package the release. Install it, or build without packaging: ./build_web.sh --no-zip"
fi

[ -d assets ] || die "assets/ is missing. Run this script from the repository root."

echo "Emscripten: $(emcc --version | head -1)"

# --- raylib for the web -----------------------------------------------------
if [ "$clean" -eq 1 ]; then
  echo "Removing cached dependencies in ${DEPS_DIR}"
  rm -rf "$DEPS_DIR"
fi

mkdir -p "$DEPS_DIR"

if [ ! -f "${RAYLIB_SRC}/libraylib.a" ]; then
  archive="${DEPS_DIR}/raylib-${RAYLIB_VERSION}.tar.gz"

  if [ ! -f "$archive" ]; then
    echo "Downloading raylib ${RAYLIB_VERSION}"
    curl -fL --retry 3 -o "$archive.part" "$RAYLIB_URL" \
      || die "could not download $RAYLIB_URL"
    mv "$archive.part" "$archive"
  fi

  if command -v shasum >/dev/null 2>&1; then
    actual=$(shasum -a 256 "$archive" | awk '{print $1}')
  elif command -v sha256sum >/dev/null 2>&1; then
    actual=$(sha256sum "$archive" | awk '{print $1}')
  else
    actual="$RAYLIB_SHA256"
    echo "Warning: no shasum/sha256sum available; skipping checksum verification." >&2
  fi

  if [ "$actual" != "$RAYLIB_SHA256" ]; then
    rm -f "$archive"
    die "raylib ${RAYLIB_VERSION} checksum mismatch (got $actual). The download was removed; re-run to try again."
  fi

  if [ ! -d "${DEPS_DIR}/raylib-${RAYLIB_VERSION}" ]; then
    echo "Unpacking raylib ${RAYLIB_VERSION}"
    tar xzf "$archive" -C "$DEPS_DIR"
  fi

  echo "Building raylib ${RAYLIB_VERSION} for PLATFORM_WEB"
  ( cd "$RAYLIB_SRC" && make PLATFORM=PLATFORM_WEB RAYLIB_BUILD_MODE=RELEASE -B >/dev/null )
  [ -f "${RAYLIB_SRC}/libraylib.a" ] || die "the raylib web build did not produce libraylib.a"
fi

echo "raylib: ${RAYLIB_SRC}/libraylib.a"

# --- Game -------------------------------------------------------------------
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

SOURCES="src/main.c src/render.c src/assets.c src/audio.c src/game.c \
src/gameplay.c src/input.c src/menus.c src/hud.c src/bricks.c src/levels.c \
src/ball.c src/powerups.c src/storage.c"

for f in $SOURCES; do
  [ -f "$f" ] || die "source file $f is missing"
done

# Guard against a source file being added to src/ without being listed above.
on_disk=$(ls src/*.c | tr '\n' ' ')
for f in $on_disk; do
  case " $SOURCES " in
    *" $f "*) ;;
    *) die "$f exists in src/ but is not listed in build_web.sh (add it there and to run.sh / build.bat)" ;;
  esac
done

echo "Compiling and linking the WebAssembly build"

# EXPORTED_RUNTIME_METHODS notes:
#   FS / IDBFS / addRunDependency / removeRunDependency  used by src/web/pre.js
#     and src/web/library_dxball.js to mount and flush the save directory.
#   HEAPF32  miniaudio's Web Audio callback (inside raylib's raudio.c) reads
#     Module.HEAPF32.buffer on every audio frame. Emscripten 6 no longer places
#     the heap views on Module unless they are exported, and without this the
#     audio callback throws on every buffer.
# shellcheck disable=SC2086
emcc -std=c99 -Wall -Wextra -Wno-unused-parameter -O3 \
  $SOURCES \
  -o "$OUT_DIR/index.html" \
  -I "$RAYLIB_SRC" \
  "${RAYLIB_SRC}/libraylib.a" \
  -DPLATFORM_WEB \
  -sUSE_GLFW=3 \
  -sGL_ENABLE_GET_PROC_ADDRESS=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_MEMORY=134217728 \
  -sMAXIMUM_MEMORY=1073741824 \
  -sSTACK_SIZE=4194304 \
  -sFORCE_FILESYSTEM=1 \
  -lidbfs.js \
  -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,addRunDependency,removeRunDependency,HEAPF32 \
  -sEXPORTED_FUNCTIONS=_main,_ma_device__on_notification_unlocked \
  -sMODULARIZE=0 \
  -sASSERTIONS=0 \
  --closure 0 \
  --shell-file src/web/shell.html \
  --pre-js src/web/pre.js \
  --js-library src/web/library_dxball.js \
  --preload-file assets@/assets

[ -f "$OUT_DIR/index.html" ] || die "the link step did not produce $OUT_DIR/index.html"

# raylib's license must travel with the binary it is linked into.
cp raylib_LICENSE.txt "$OUT_DIR/raylib_LICENSE.txt"
cp assets/fonts/OFL-Cinzel.txt "$OUT_DIR/OFL-Cinzel.txt"
cp assets/fonts/OFL-CinzelDecorative.txt "$OUT_DIR/OFL-CinzelDecorative.txt"

# --- Manifest ---------------------------------------------------------------
( cd "$OUT_DIR" && find . -type f ! -name MANIFEST.txt \
    | sed 's|^\./||' | LC_ALL=C sort > ../.filelist.tmp )

{
  echo "DX Ball - Wizarding World : web release contents"
  echo "built: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "raylib: ${RAYLIB_VERSION}"
  echo "emscripten: $(emcc --version | head -1 | sed 's/^emcc (.*) //')"
  echo
  if command -v shasum >/dev/null 2>&1; then
    ( cd "$OUT_DIR" && xargs -I{} shasum -a 256 "{}" < ../.filelist.tmp )
  elif command -v sha256sum >/dev/null 2>&1; then
    ( cd "$OUT_DIR" && xargs -I{} sha256sum "{}" < ../.filelist.tmp )
  else
    ( cd "$OUT_DIR" && xargs -I{} ls -l "{}" < ../.filelist.tmp )
  fi
} > "$OUT_DIR/MANIFEST.txt"
rm -f dist/.filelist.tmp

# --- Package ----------------------------------------------------------------
if [ "$make_zip" -eq 1 ]; then
  rm -f "$ZIP_PATH"
  # Zipped from inside $OUT_DIR so index.html lands at the archive root, which
  # is what itch.io requires of an HTML game upload.
  ( cd "$OUT_DIR" && zip -q -r -X "../$(basename "$ZIP_PATH")" . -x '.*' -x '*/.*' )
  [ -f "$ZIP_PATH" ] || die "packaging failed"
fi

# --- Summary ----------------------------------------------------------------
# Sizes are summed from the actual file lengths rather than du(1), which
# reports allocated disk blocks and overstates a directory of large files.
echo
echo "Web release:  $OUT_DIR/"
( cd "$OUT_DIR" && find . -type f -exec ls -l {} + | sort -k5,5nr \
    | awk '{ total += $5; name = $NF; sub(/^\.\//, "", name);
             printf "    %-28s %8.2f MB\n", name, $5/1048576 }
           END { printf "  extracted size: %.2f MB in %d files\n", total/1048576, NR }' \
    | sort -r )

if [ "$make_zip" -eq 1 ]; then
  echo
  echo "itch.io upload: $ZIP_PATH"
  awk -v n="$(wc -c < "$ZIP_PATH")" 'BEGIN { printf "  zip size: %.2f MB\n", n/1048576 }'
  echo "  entries at the archive root (itch.io requires index.html here):"
  unzip -Z1 "$ZIP_PATH" | sed 's|^|    |'
  if unzip -Z1 "$ZIP_PATH" | grep -q '/'; then
    die "the archive contains a subdirectory; index.html must be at its root"
  fi
  unzip -tqq "$ZIP_PATH" >/dev/null || die "the generated archive failed its integrity check"
fi

echo
echo "Test locally with:  python3 -m http.server 8000 --directory $OUT_DIR"
echo "then open:          http://localhost:8000/"
