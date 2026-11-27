#!/usr/bin/env bash
# fetch-deps.sh - aggiorna le dipendenze di terze parti vendorizzate.
#
# Uso:    ./scripts/fetch-deps.sh [--os-packages]
#         --os-packages: stampa (e su Debian/Ubuntu installa con apt) i
#         pacchetti SDL2 necessari dal sistema.
#
# cJSON è VENDORIZZATO nel repo (third_party/cJSON, licenza MIT) così il
# progetto compila sempre anche offline. Questo script serve solo per
# aggiornarlo a una versione nuova.
set -e
CJSON_VERSION="${CJSON_VERSION:-v1.7.18}"
CJSON_BASE="https://raw.githubusercontent.com/DaveGamble/cJSON/${CJSON_VERSION}"

if [ "${1:-}" = "--os-packages" ]; then
    echo "Pacchetti richiesti (Debian/Ubuntu):"
    echo "  sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \\"
    echo "       libsdl2-mixer-dev libudev-dev build-essential"
    exit 0
fi

echo "Scarico cJSON ${CJSON_VERSION}..."
mkdir -p third_party/cJSON
curl -fsSL "${CJSON_BASE}/cJSON.c" -o third_party/cJSON/cJSON.c
curl -fsSL "${CJSON_BASE}/cJSON.h" -o third_party/cJSON/cJSON.h
echo "cJSON ${CJSON_VERSION} aggiornato in third_party/cJSON/ (ricordati di commitare)."
