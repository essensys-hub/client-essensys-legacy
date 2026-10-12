#!/bin/sh
# Recopie dans ce depot les modules portables dont la source de reference
# (et les tests hote NR-gcc-*) est dans essensys-gcc :
#   bp/app/ba_resil.[ch]  -> C/ba_resil.c, H/ba_resil.h
#   bp/i2c/i2c_bounded.h  -> H/i2c_bounded.h (API du pilote I2C borne)
# Usage : scripts/sync-from-essensys-gcc.sh <chemin vers essensys-gcc>
# Feature i2c-surge-protection-2026-10-007, Refs essensys-hub/essensys-board-SC944D#1
set -eu
GCC=${1:?chemin du depot essensys-gcc}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
copy() {
  { printf '/* COPIE de essensys-gcc/%s -- ne pas modifier ici.\n * Source de reference et tests hote (NR-gcc-*) dans essensys-gcc ;\n * resynchroniser avec scripts/sync-from-essensys-gcc.sh.\n * Feature i2c-surge-protection-2026-10-007, Refs essensys-hub/essensys-board-SC944D#1 */\n' "$1"
    cat "$GCC/$1"; } > "$ROOT/$2"
  echo "$1 -> $2"
}
copy bp/app/ba_resil.c C/ba_resil.c
copy bp/app/ba_resil.h H/ba_resil.h
copy bp/i2c/i2c_bounded.h H/i2c_bounded.h
