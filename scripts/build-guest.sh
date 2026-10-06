#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${KDIR:?Set KDIR to guest kernel build headers}"
: "${KERNEL:?Set KERNEL to guest bzImage}"
make KDIR="$KDIR"
root=build/root
rm -rf "$root"
mkdir -p "$root"/{bin,dev,proc,sys,tmp,lab}
cp "${BUSYBOX:-/usr/bin/busybox}" "$root/bin/"
for app in sh mount insmod rmmod cat echo sleep poweroff mkdir grep dmesg basename wc; do
    ln -s busybox "$root/bin/$app"
done
cp driver/edu_lab.ko "$root/lab/"
test ! -f build/edu-test || cp build/edu-test "$root/lab/"
cp tests/guest-init.sh "$root/init"
chmod +x "$root/init"
(cd "$root"; find . -print0 | cpio --null -o --format=newc) | gzip > build/initramfs.gz
cp "$KERNEL" build/bzImage
{
    uname -a
    gcc --version | head -1
    g++ --version | head -1
    make --version | head -1
    ld --version | head -1
    pahole --version
    cpio --version | head -1
    qemu-system-x86_64 --version | head -1
    make -s -C "$KDIR" kernelrelease
    "${BUSYBOX:-/usr/bin/busybox}" | head -1
} > build/environment.txt
