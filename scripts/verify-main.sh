#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash -n scripts/*.sh tests/*.sh
bash scripts/build-guest.sh
for run in 1 2 3; do
    LAB_TEST=all bash scripts/run-guest.sh
    cp build/serial.log "build/all-$run.log"
done
LAB_TEST=selftest bash scripts/run-guest.sh
cp build/serial.log build/selftest.log
LAB_TEST=recovery bash scripts/run-guest.sh
cp build/serial.log build/recovery.log
