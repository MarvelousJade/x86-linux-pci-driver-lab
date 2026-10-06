#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
# A serial-only, diskless guest. No access to host drives or networking.
set +e
timeout 90 qemu-system-x86_64 -accel "${ACCEL:-tcg}" -m 256 -smp 2 \
    -display none -serial stdio -monitor none -no-reboot -nic none \
    -kernel build/bzImage -initrd build/initramfs.gz \
    -append "console=ttyS0 panic=-1 lab_test=${LAB_TEST:-all}" \
    -L "${QEMU_DATA:-/usr/share/qemu}" -device edu -device edu 2>&1 | tee build/serial.log
rc=${PIPESTATUS[0]}
set -e
test "$rc" = 0
grep -q '^LAB PASS' build/serial.log
! grep -Eq 'BUG:|Oops:|WARNING:|Kernel panic|LAB FAIL' build/serial.log
