# Userspace ABI and registers

Open `/dev/edu_lab` O_RDWR (mode 0600, root in the disposable guest). `include/edu_lab.h` is shared C/C++ UAPI. `edu_value` is exactly eight bytes: u32 input, u32 result. No pointers, padding, native longs or implicit versioning. Exact ioctl command (including size/direction) is checked; unknown/malformed commands return ENOTTY; invalid copy buffer EFAULT. Output is only valid on success. Only native x86-64 callers are supported/tested; no compat handler.

`EDU_IOC_LIVE`: write input to BAR0+0x04, read its inversion into result. This is real MMIO, not a software inversion in the driver. The C++ utility independently checks `result == ~input`.

Registers used: 0x00 identification (RO); 0x04 liveness (RW). Accesses use writel/readl, 32 bits as required by EDU. Readback flushes posted writes. BAR0 is requested before mapping. No DMA or bus-master enabling.

The operation mutex serializes callers and excludes MMIO teardown. Open acquires a kref while misc core holds its registration lock; deregistration stops new opens before the binding reference is dropped. Each fd release drops its reference. `.owner` pins the module while an fd exists; the fd does not prevent PCI unbinding. The retained detached object is software only and returns ENODEV before any MMIO.

Manual first workflow in a full Linux guest launched with `-device edu`:
```
insmod driver/edu_lab.ko
sudo build/edu-test live 0x12345678
rmmod edu_lab
```
Expected result: edcba987, device closed, then successful module unload. Our initramfs does this automatically in `tests/guest-init.sh`.
