# x86 Linux PCI Driver Lab

One C Linux PCI module, a minimal misc ioctl ABI, and a C++ test utility for **QEMU EDU emulated hardware** in an x86-64 Linux guest. Not physical board bring-up or a complete BSP. No DMA, networking, power management, firmware work, service or custom emulator.

## Run

Requires a Linux host (WSL2 works), GCC/G++, make, matching guest kernel/headers, static BusyBox, cpio, gzip and QEMU with EDU. TCG is the default; no hardware virtualization required. Do **not** load the module into WSL/host Linux.

Arch users can install equivalent packages normally or use the isolated setup used for recorded verification (downloads signed packages without upgrading the host):
```
bash scripts/isolated-arch-tools.sh
source scripts/lab-env.sh  # check kernel directory if package versions changed
bash scripts/build-guest.sh
bash scripts/run-guest.sh
```

On other Linux setups:
```
export KDIR=/path/to/guest/kernel/build
export KERNEL=/path/to/guest/bzImage
export BUSYBOX=/path/to/static/busybox
bash scripts/build-guest.sh
bash scripts/run-guest.sh
```
Use the **guest** headers, not `uname -r` on WSL. The serial-only guest has no disks or networking. QEMU boots the existing EDU model (`-device edu`), not a custom emulator. Two EDU functions test single-instance rejection. Tests shut down automatically and the host script requires `LAB PASS` and no kernel BUG/Oops/WARNING/panic. `LAB_TEST=selftest` or `LAB_TEST=recovery` selects focused regression runs.

Manual workflows in a full guest:
```
insmod driver/edu_lab.ko
sudo build/edu-test live 0x12345678  # edcba987, checked by utility
sudo build/edu-test factorial 12    # 479001600, interrupt-driven
rmmod edu_lab                     # utility has closed its handle
```

## Architecture

PCI probe -> request/map BAR0 -> shared INTx handler -> `/dev/edu_lab`.
C++ open -> eight-byte ioctl -> serialized MMIO operation -> verified result -> close.
Factorial: prepare completion before start -> wait up to one second -> acknowledge actual IRQ -> read result. Timeout retains ownership until old notification is resolved; no invented reset. Unbind retains fd software lifetime while rejecting further MMIO.

## Read next

- [Acceptance checklist](docs/plan.md) and [environment](docs/environment.md)
- [Registers, ABI and request lifecycle](docs/abi.md)
- [Actual verification ledger](docs/verification.md) and [raw logs](docs/evidence/)
- [Three engineering decisions](docs/design.md)
- [Known limitations](docs/limitations.md)
- [Interview introduction, walkthrough and code-tracing questions](docs/interview.md)
- [Learning exercises (no solutions)](docs/exercises.md)

Discovery, both workflows, boundaries, concurrency, timeout/recovery, open-handle removal and reload are guest-verified. Three full repeat runs and both focused regressions passed. Two intentionally faulty learning checkpoints and separately verified reference solutions are preserved; see the exercise guide. Main contains no introduced driver defects. Agent-owned implementation/testing is not attributed to the learner; no invented personal stories.
