# Userspace ABI and registers

Open `/dev/edu_lab` O_RDWR (mode 0600, root in the disposable guest). `include/edu_lab.h` is shared C/C++ UAPI. `edu_value` is exactly eight bytes: u32 input, u32 result. No pointers, padding, native longs or implicit versioning. Exact ioctl command (including size/direction) is checked; unknown/malformed commands return ENOTTY; invalid copy buffer EFAULT. Output is only valid on success. Only native x86-64 callers are supported/tested; no compat handler.

`EDU_IOC_LIVE`: write input to BAR0+0x04, read its inversion into result. This is real MMIO, not a software inversion in the driver. The C++ utility independently checks `result == ~input`.

`EDU_IOC_FACTORIAL`: input 0..12 only; output the device's 32-bit factorial result. 12! = 479001600 fits u32; 13! = 6227020800 does not. Invalid input returns EINVAL before MMIO start. One operation mutex owns the hardware through wait and readback. Four independent handles may call concurrently; no hardware queue exists.

Registers used: 0x00 identification (RO); 0x04 liveness (RW); 0x08 operand/result (RW); 0x20 status (bit 0 busy, bit 7 request completion IRQ); 0x24 interrupt status (RO); 0x64 interrupt acknowledgement (WO). FACT_IRQ is bit 0 (source cross-checked against QEMU v11.1.2 hw/misc/edu.c). Accesses use writel/readl, 32 bits as required by EDU. Readback flushes posted writes. BAR0 is requested before mapping. No DMA or bus-master enabling.

The operation mutex serializes callers and excludes MMIO teardown. Open acquires a kref while misc core holds its registration lock; deregistration stops new opens before the binding reference is dropped. Each fd release drops its reference. `.owner` pins the module while an fd exists; the fd does not prevent PCI unbinding. The retained detached object is software only and returns ENODEV before any MMIO.

Interrupt workflow: reinitialize embedded completion and mark pending under irq_lock -> enable factorial notification -> write operand -> flush start -> sleep for up to 1000 ms -> ISR reads status, returns IRQ_NONE if not ours, acknowledges reported bits and flushes -> mark actual factorial interrupt seen and complete waiter only when idle -> read result. The ISR never takes op_lock. Completion before the waiter sleeps is valid.

Root-only `hold_completion` is a deterministic learning/test parameter (default false). While true, the ISR still acknowledges the actual hardware IRQ, but software notification is deferred by delayed work until the parameter is false. Pending work cannot be reused until both IRQ acknowledgement and notification delivery are recorded. This exercises actual wait expiry and a late software completion; it does not simulate slow physical hardware or manufacture an IRQ. Removal frees IRQ first, then synchronously cancels delayed work before freeing the object.

Run `build/edu-test recovery` in the guest to hold delivery, observe the one-second ETIMEDOUT, check immediate EBUSY, release delivery, then verify a distinct factorial result. The same command tests signal interruption/recovery. Never enable the hook in ordinary use.

A signal returns EINTR to userspace (kernel ERESTARTSYS/restart rules apply). Timeout returns ETIMEDOUT. Neither cancels hardware. A pending request remains owned until hardware is idle AND its real IRQ has been acknowledged and the notification delivered. New work returns EBUSY in the meantime. irq_lock excludes old ISR signalling during reinitialization; idle alone is not sufficient. If notification is permanently lost, factorial remains quarantined; restart the disposable guest rather than pretend to reset EDU. Liveness still works. Mutex queueing time is not included in the device's one-second wait budget.

Teardown masks INTx at PCI level in addition to clearing notification enable; free_irq synchronizes the handler before BAR unmapping. No bus mastering/DMA was enabled. Reuse after unresolved hardware work is outside the supported recovery contract; restart the guest in that case.

Manual first workflow in a full Linux guest launched with `-device edu`:
```
insmod driver/edu_lab.ko
sudo build/edu-test live 0x12345678
rmmod edu_lab
```
Expected result: edcba987, device closed, then successful module unload. Our initramfs does this automatically in `tests/guest-init.sh`. The second workflow is `build/edu-test factorial 12` after loading, expected 479001600 verified through the ISR path.
