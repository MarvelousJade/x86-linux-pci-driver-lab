#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# kbuild does not support spaces in M= paths. Stage only module inputs in /tmp.
stage=$(mktemp -d /tmp/edu-module.XXXXXX)
trap 'rm -rf "$stage"' EXIT
cp driver/Makefile driver/edu_lab.c "$stage/"
test ! -d include || cp -r include "$stage/"
make -C "$1" M="$stage" modules W=1 CC="${CC:-gcc}"
cp "$stage/edu_lab.ko" driver/
