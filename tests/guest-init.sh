#!/bin/sh
export PATH=/bin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
fail() { echo "LAB FAIL: $*"; dmesg; poweroff -f; }
bound_bdf() {
    for p in /sys/bus/pci/drivers/edu_lab/????:??:??.?; do
        [ ! -e "$p" ] || basename "$p"
    done
}
uname -a
count=0
for p in /sys/bus/pci/devices/*; do
    if [ "$(cat "$p/vendor")" = 0x1234 ] && [ "$(cat "$p/device")" = 0x11e8 ]; then
        echo "EDU discovered: $(basename "$p")"
        count=$((count+1))
    fi
done
[ "$count" = 2 ] || fail discovery
for hook in fail_probe fail_after_irq; do
    insmod /lab/edu_lab.ko "$hook=1" || fail injected-load
    [ -z "$(bound_bdf)" ] || fail injected-probe-bound
    rmmod edu_lab || fail injected-unload
done
insmod /lab/edu_lab.ko || fail load
[ "$(bound_bdf | wc -l)" = 1 ] || fail single-instance
case "${lab_test:-all}" in
all)
    /lab/edu-test selftest || fail userspace-baseline
    /lab/edu-test recovery || fail timeout-recovery
    cat /proc/interrupts | grep edu_lab || fail interrupt-evidence
    /lab/edu-test lifecycle "$(bound_bdf)" unbind || fail unbind-open-handle
    rmmod edu_lab || fail unload-after-unbind
    insmod /lab/edu_lab.ko || fail rebind-load
    /lab/edu-test factorial 12 || fail rebind-factorial
    /lab/edu-test lifecycle "$(bound_bdf)" remove || fail removal-open-handle
    ;;
selftest|recovery)
    /lab/edu-test "$lab_test" || fail selected-regression
    ;;
*) fail unknown-test-mode ;;
esac
rmmod edu_lab || fail unload
insmod /lab/edu_lab.ko || fail reload
/lab/edu-test live 0xdeadbeef || fail reload-liveness
/lab/edu-test factorial 12 || fail reload-factorial
rmmod edu_lab || fail second-unload
dmesg
echo "LAB PASS $lab_test"
poweroff -f
