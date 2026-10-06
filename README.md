# x86 Linux PCI Driver Lab

A small C Linux PCI module for QEMU EDU, with a C++ userspace tester. **Emulated hardware only**, not a complete BSP or physical board validation. No DMA, networking, power management, firmware work, service, or custom emulator.

## Environment

Linux host (including WSL2) requires GCC/G++, make, matching guest kernel and headers, static BusyBox, cpio, gzip and qemu-system-x86_64 with EDU. TCG requires no hardware virtualization. KVM is optional; use `ACCEL=kvm` only if it works on your host.

For Arch: install `qemu-system-x86 linux-lts linux-lts-headers busybox cpio`. On other distributions install equivalent packages; scripts currently expect BusyBox at `/usr/bin/busybox`. The guest is an isolated diskless initramfs, no host disks or network. Do not load this driver on the host.

```
export KDIR=/usr/lib/modules/<guest-version>/build
export KERNEL=/usr/lib/modules/<guest-version>/vmlinuz
bash scripts/build-guest.sh
bash scripts/run-guest.sh
```

Use the **guest** headers, not `uname -r` on WSL. See `docs/plan.md` for increment acceptance gates. `build/environment.txt` and `build/serial.log` are raw generated evidence; milestone copies go in `docs/evidence/` after verification. Discovery/resource cleanup is verified under TCG; remaining workflow gates are pending. See `docs/verification.md`.
