# Interview preparation

These are descriptions of the project, not claims about your personal work. Agent-authored code and tests are recorded in Git. Say what you actually implemented, reviewed, ran or debugged yourself; do not present agent investigation as your own.

## 60-second introduction

“This is a small Linux PCI driver lab tested against QEMU's EDU device in an x86-64 Linux guest. The C module discovers the PCI function, owns and maps its BAR, and exposes two synchronous ioctls through a misc device. A C++ utility checks register inversion and factorial results. The interesting parts are shared interrupt acknowledgement, initializing completion before starting work, and keeping a timed-out request owned until its old notification is finished. Open file handles retain a software object after unbind but cannot access unmapped registers. Tests run in a diskless guest, with logs distinguishing ordinary hardware emulation from deliberately delayed software completion. This is not physical board bring-up or a complete BSP. My next step is to trace the code and investigate the separate learning defects.”

Use only verified ledger results when discussing specific checks.

## Three-minute technical walkthrough

**0:00–0:40 — discovery.** Start at edu_ids and edu_probe. Linux matches 1234:11e8. The driver reserves one supported instance, enables memory decoding, checks BAR0, requests the region and maps it. It disables stale interrupt signalling, initializes a shared INTx handler, then registers a root-only misc node. Error labels release only resources already acquired. There is no DMA or bus-master enable.

**0:40–1:15 — first workflow.** Follow Device in edu-test.cpp, then edu_open and edu_ioctl. An fd takes a kref. The liveness ioctl copies eight bytes from userspace, writes the input with writel, then reads the hardware inversion with readl. Userspace checks the returned value independently. Exact commands are validated; bogus buffers return EFAULT. A readback also flushes the posted write. Closing drops the fd reference.

**1:15–2:10 — computation and recovery.** EDU has one operand/result register, so one operation mutex owns the request through its wait. Only 0..12 is accepted because the result is 32-bit. Under irq_lock, prepare pending state and reinitialize completion before enabling and starting computation. The ISR checks shared interrupt status, acknowledges it, verifies factorial/idle and completes the waiter. Completion can arrive before the waiter sleeps. A timeout or signal does not cancel EDU. New work gets EBUSY until the old IRQ and notification are resolved. The hook delays software delivery after a real IRQ, allowing deterministic failure-path tests; it isn't a hardware reset.

**2:10–3:00 — lifetime and evidence.** Removal deregisters the misc node before taking the operation mutex, avoiding an open/removal lock inversion. It waits for bounded active work, marks the object removed, masks notification and PCI INTx, frees/synchronizes IRQ, cancels deferred work and only then unmaps resources. Open handles keep the object alive but further ioctls fail ENODEV. Trace the real guest tests and serial evidence. Explain that TCG emulation, one architecture and small valid inputs do not prove physical behavior, arbitrary hung-device recovery or production readiness.

## Questions to answer by tracing code

- Why is 12 accepted but 13 rejected? Where does rejection happen relative to hardware writes?
- Which paths can signal done? Why can idle alone not retire an old request?
- Why does op_lock span the wait, while the ISR only takes irq_lock?
- What protects completion reinitialization from an old ISR/notifier?
- How are shared INTx and posted acknowledgement writes handled?
- What is still alive after unbind? What does `.owner` protect that kref does not?
- Why must free_irq happen before BAR unmapping, and before canceling work?
- Can a timed-out operation be cancelled? What if notification never arrives?
- What exactly does hold_completion inject, and what does it not prove?
- Where do probe failure labels unwind each resource?

## Your debugging notes (fill after doing exercises)

Use `Problem -> hypothesis -> evidence -> decision -> fix -> verification`.
Record commands, observed output, rejected hypotheses and diff/commit. Leave hypotheses and reasoning blank until you actually investigate. After you share those notes, we can prepare accurate stories from them. There are no invented incidents, customers, benchmarks or personal discovery claims here.
