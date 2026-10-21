#!/bin/sh
# Builds the firmware in a Linux container (the PlatformIO ESP8266 toolchain has no
# native Apple Silicon build). Usage: tools/docker-build.sh [env ...]  (default: smalltv)
set -e
cd "$(dirname "$0")/.."
ENVS=${*:-smalltv}
ARGS=""
for e in $ENVS; do ARGS="$ARGS -e $e"; done
docker run --rm -v "$PWD":/work -v smalltv-pio:/root/.platformio -w /work python:3.12-slim \
  sh -c "pip install -q --root-user-action=ignore platformio >/dev/null && pio run $ARGS"
