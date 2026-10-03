#!/bin/sh
# Produces a re-encoded copy of assets/ for the web build.
#
#   tools/optimize_web_assets.sh <source-dir> <output-dir>
#
# The originals are never modified: the desktop builds and the repository keep
# the full-quality artwork, and only the browser download is shrunk. The game
# finds the re-encoded files through resolveAssetPath() in src/assets.c, so no
# asset path in the source changes.
#
#   PNG with no transparency -> JPEG      (roughly 6x smaller)
#   PNG that uses alpha      -> pngquant  (roughly 3x smaller, alpha kept)
#   WAV                      -> OGG       (roughly 5x smaller)
#   everything else          -> copied
#
# Every step degrades gracefully: when a tool is missing the file is copied
# unchanged, so the build still produces a working release, just a larger one.
set -eu

SRC=${1:?usage: optimize_web_assets.sh <source-dir> <output-dir>}
OUT=${2:?usage: optimize_web_assets.sh <source-dir> <output-dir>}

JPEG_QUALITY=${DXBALL_JPEG_QUALITY:-3}   # ffmpeg -qscale:v, 2 best .. 31 worst
PNG_QUALITY_LADDER=${DXBALL_PNG_QUALITY_LADDER:-"75-95 55-92 0-90"}
OGG_QUALITY=${DXBALL_OGG_QUALITY:-4}     # ffmpeg -qscale:a

have() { command -v "$1" >/dev/null 2>&1; }

HAVE_FFMPEG=0;   have ffmpeg   && HAVE_FFMPEG=1
HAVE_PNGQUANT=0; have pngquant && HAVE_PNGQUANT=1
HAVE_OXIPNG=0;   have oxipng   && HAVE_OXIPNG=1

# Prefer libvorbis where the ffmpeg build has it; otherwise fall back to
# ffmpeg's own Vorbis encoder, which needs -strict -2 because it is still
# marked experimental.
HAVE_VORBIS=0
VORBIS_ENCODER=vorbis
VORBIS_CHANNELS=""
if [ "$HAVE_FFMPEG" -eq 1 ]; then
  if ffmpeg -hide_banner -encoders 2>/dev/null | grep -q ' libvorbis '; then
    HAVE_VORBIS=1
    VORBIS_ENCODER=libvorbis
    VORBIS_CHANNELS=""
  elif ffmpeg -hide_banner -encoders 2>/dev/null | grep -q ' vorbis '; then
    HAVE_VORBIS=1
    VORBIS_ENCODER=vorbis
    # ffmpeg's built-in Vorbis encoder refuses mono input, and most of the
    # sound effects are mono. Upmixing to stereo costs almost nothing once
    # Vorbis channel coupling has run over two identical channels.
    VORBIS_CHANNELS="-ac 2"
  fi
fi

if [ "$HAVE_FFMPEG" -eq 0 ]; then
  echo "  note: ffmpeg not found - PNGs stay PNG and WAVs stay WAV (larger download)" >&2
fi
if [ "$HAVE_PNGQUANT" -eq 0 ]; then
  echo "  note: pngquant not found - transparent PNGs are copied as-is (larger download)" >&2
fi
if [ "$HAVE_FFMPEG" -eq 1 ] && [ "$HAVE_VORBIS" -eq 0 ]; then
  echo "  note: no Vorbis encoder in ffmpeg - audio stays WAV (larger download)" >&2
fi

mkdir -p "$OUT"

errlog=$(mktemp "${TMPDIR:-/tmp}/dxball-encode.XXXXXX")
trap 'rm -f "$errlog"' EXIT INT TERM

# Reports 1 when a PNG has pixels below the requested alpha floor. The intro
# screens are almost opaque (minimum alpha 235), so their tiny translucency can
# be dropped for a much smaller JPEG. Other images require full opacity.
uses_alpha() {
  _ct=$(python3 - "$1" <<'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f:
    print(f.read(26)[25])
PY
)
  # Colour types 4 and 6 are the ones carrying an alpha channel at all.
  case "$_ct" in
    4|6) ;;
    *) return 1 ;;
  esac

  [ "$HAVE_FFMPEG" -eq 1 ] || return 0   # cannot inspect: assume alpha matters

  _min=$(ffmpeg -nostdin -v error -i "$1" -vf alphaextract -pix_fmt gray -f rawvideo - 2>/dev/null \
    | python3 -c "
import sys
d = sys.stdin.buffer.read()
print(min(d) if d else 255)")
  [ "${_min:-0}" -lt "${2:-255}" ]
}

converted=0
copied=0

find "$SRC" -type f | LC_ALL=C sort | while IFS= read -r src; do
  rel=${src#"$SRC"/}
  dstdir="$OUT/$(dirname "$rel")"
  mkdir -p "$dstdir"
  base=$(basename "$rel")
  stem=${base%.*}

  case "$base" in
    *.png)
      alpha_floor=255
      case "$rel" in
        ui/level_intros/level_intro_*.png) alpha_floor=235 ;;
      esac
      if [ "$HAVE_FFMPEG" -eq 1 ] && ! uses_alpha "$src" "$alpha_floor"; then
        # Opaque and nearly opaque intro screens are much smaller as JPEGs.
        out="$dstdir/$stem.jpg"
        if [ ! -f "$out" ] || [ "$src" -nt "$out" ]; then
          # -pix_fmt is explicit so the mjpeg encoder never has to negotiate a
          # format, which differs between ffmpeg builds. On any failure the
          # original PNG is kept: a bigger download beats a broken build, and
          # resolveAssetPath() in src/assets.c finds either name.
          if ! ffmpeg -nostdin -v error -y -i "$src" -pix_fmt yuvj420p \
                      -qscale:v "$JPEG_QUALITY" "$out" 2>"$errlog"; then
            echo "  warning: JPEG encode failed for $rel, keeping the PNG" >&2
            sed 's/^/    /' "$errlog" >&2 || true
            rm -f "$out"
            cp "$src" "$dstdir/$base"
          fi
        fi
        # A previous build may have kept the PNG before JPEG conversion became
        # available. Do not package both copies on an incremental build.
        if [ -f "$out" ]; then
          rm -f "$dstdir/$base"
        fi
      else
        out="$dstdir/$base"
        if [ ! -f "$out" ] || [ "$src" -nt "$out" ]; then
          if [ "$HAVE_PNGQUANT" -eq 1 ]; then
            # pngquant exits 98/99 when the palette cannot reach the quality
            # floor, which the detailed sprite atlases never do at a high one.
            # Walk the floor down rather than giving up: the last rung has a
            # floor of 0 and always succeeds, so the original is only kept if
            # pngquant fails outright.
            quantized=0
            for q in $PNG_QUALITY_LADDER; do
              if pngquant --quality="$q" --speed 1 --strip \
                          --force --output "$out" "$src" 2>/dev/null; then
                quantized=1
                break
              fi
            done
            [ "$quantized" -eq 1 ] || cp "$src" "$out"
            [ "$HAVE_OXIPNG" -eq 1 ] && oxipng -o2 -q --strip safe "$out" 2>/dev/null || true
          else
            cp "$src" "$out"
          fi
        fi
      fi
      ;;
    *.wav)
      # Vorbis rather than MP3: raylib streams both, but MP3's encoder padding
      # puts an audible gap at the loop point of the level and menu music.
      out="$dstdir/$stem.ogg"
      if [ "$HAVE_VORBIS" -eq 1 ]; then
        if [ ! -f "$out" ] || [ "$src" -nt "$out" ]; then
          if ! ffmpeg -nostdin -v error -y -i "$src" $VORBIS_CHANNELS -c:a $VORBIS_ENCODER \
                      -strict -2 -qscale:a "$OGG_QUALITY" "$out" 2>"$errlog"; then
            echo "  warning: OGG encode failed for $rel, keeping the WAV" >&2
            sed 's/^/    /' "$errlog" >&2 || true
            rm -f "$out"
            cp "$src" "$dstdir/$base"
          fi
        fi
      else
        out="$dstdir/$base"
        [ -f "$out" ] && [ ! "$src" -nt "$out" ] || cp "$src" "$out"
      fi
      ;;
    *)
      out="$dstdir/$base"
      [ -f "$out" ] && [ ! "$src" -nt "$out" ] || cp "$src" "$out"
      ;;
  esac
done

# Drop anything left over from a previous run whose source has since gone.
find "$OUT" -type f | while IFS= read -r built; do
  rel=${built#"$OUT"/}
  dir=$(dirname "$rel")
  base=$(basename "$rel")
  stem=${base%.*}
  if [ ! -f "$SRC/$rel" ] &&
     [ ! -f "$SRC/$dir/$stem.png" ] &&
     [ ! -f "$SRC/$dir/$stem.wav" ]; then
    rm -f "$built"
  fi
done

# Counted in python rather than with stat(1), whose size format differs
# between the BSD and GNU versions.
python3 - "$SRC" "$OUT" <<'PY'
import os, sys


def total(root):
    n = 0
    for base, _, files in os.walk(root):
        for f in files:
            n += os.path.getsize(os.path.join(base, f))
    return n


a, b = total(sys.argv[1]), total(sys.argv[2])
ratio = (a / b) if b else 0
print(f"  assets: {a/1048576:.1f} MB -> {b/1048576:.1f} MB ({ratio:.1f}x smaller)")
PY
