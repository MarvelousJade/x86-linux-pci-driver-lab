#!/bin/sh
export PATH=/bin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
fail() { echo "LAB FAIL: $*"; dmesg; poweroff -f; }
uname -a
count=0
for p in /sys/bus/pci/devices/*; do
    if [ "$(cat "$p/vendor")" = 0x1234 ] && [ "$(cat "$p/device")" = 0x11e8 ]; then
        echo "EDU discovered: $(basename "$p")"
        count=$((count+1))
    fi
done
[ "$count" = 2 ] || fail discovery
insmod /lab/edu_lab.ko fail_probe=1 || fail injected-load
for p in /sys/bus/pci/drivers/edu_lab/????:??:??.?; do
    [ ! -e "$p" ] || fail injected-probe-bound
done
rmmod edu_lab || fail injected-unload
insmod /lab/edu_lab.ko || fail load
count=0
for p in /sys/bus/pci/drivers/edu_lab/????:??:??.?; do
    [ -e "$p" ] && count=$((count+1))
done
[ "$count" = 1 ] || fail single-instance
/lab/edu-test selftest || fail userspace-baseline
/lab/edu-test recovery || fail timeout-recovery
cat /proc/interrupts | grep edu_lab || fail interrupt-evidence
rmmod edu_lab || fail unload
insmod /lab/edu_lab.ko || fail reload
/lab/edu-test live 0xdeadbeef || fail reload-liveness
rmmod edu_lab || fail second-unload
dmesg
echo 'LAB PASS baseline/interrupts/concurrency/recovery/load/unload/reload'
poweroff -f
