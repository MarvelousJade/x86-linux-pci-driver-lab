# Design notes (implementation gates in plan.md)

## 1. Kernel module rather than UIO
Constraint: demonstrate PCI resource ownership, kernel MMIO APIs, interrupt acknowledgement, and file lifetime. UIO is a reasonable alternative for a userspace register experiment but moves these responsibilities away from the requested C driver. Selected: one PCI module plus misc device, no generic framework or DMA. Evidence: Linux PCI documentation defines probe/remove ownership and MMIO posting rules; EDU permits 32-bit accesses below 0x80. This is a learning choice, not evidence that a kernel module is always preferable.

## 2. Synchronous serialized interface
Constraint: EDU has one factorial operand/result register, no tags or queue. An async queue would require software request objects and cancellation semantics without more hardware parallelism. Selected: one mutex spanning each ioctl's MMIO and wait; completion embedded in the long-lived device object. Callers queue on a mutex; device computation wait is bounded. No throughput benchmarks claimed. Successful result delivery must require both a real factorial interrupt and hardware idle, never just a stale completion.

## 3. Recovery and lifetime
Constraint: EDU has no documented reset or cancellation. Its implementation clears COMPUTING before acquiring the QEMU big lock to raise FACT_IRQ (bit 1). Therefore idle alone is insufficient to prove an old interrupt cannot arrive. Selected design: timed-out or interrupted work keeps a pending marker. Future factorial calls return EBUSY until idle AND the ISR has acknowledged the old completion. Then synchronize the IRQ before reinitializing completion or submitting new work. Lost notifications can leave the device quarantined; do not invent a reset.

Removal must stop new opens, let bounded in-flight work release the operation mutex, disable factorial notifications, synchronize/free IRQ before unmapping BAR0, and retain the software object for open handles using kref. An open fd pins the module, not the PCI device; unbinding may still occur. Operations on the detached object return ENODEV. This deliberately favors a small bounded wait over complex immediate cancellation of an in-flight ioctl.

Source cross-check: QEMU v9.2.0 hw/misc/edu.c was inspected for FACT_IRQ=1, status/write behavior and the idle-before-interrupt window. Installed QEMU source/version will also be checked before relying on runtime behavior. These notes are decisions, not yet runtime evidence.
