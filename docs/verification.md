# Verification ledger

Only actual guest runs count as PCI/MMIO/IRQ evidence. Serial logs are verbatim console output (including CRLF). Unsigned out-of-tree module taint is expected; no kernel BUG/Oops/WARNING/panic is accepted.

## Increment 1: discovery/resources
Command: `source scripts/lab-env.sh; bash scripts/build-guest.sh; bash scripts/run-guest.sh`.
PASS under TCG, 2 vCPUs: guest Linux 6.18.55-1-lts, two EDU functions discovered, deliberate post-map probe failure returns EIO for both; subsequent load binds exactly one and rejects other with EBUSY; unload/reload/unload works. Identification register read 0x010000ed. `01-discovery.log`, `environment.txt`, `packages.txt` record evidence and exact packages. Module compiled W=1 without warnings.

Environment setup failures investigated within this uncommitted increment: stale mirror indexes (404, no packages installed); overly broad isolated-tool PATH selected incompatible Bash (fixed by wrappers only); missing pahole wrapper (added). None is a claimed hardware defect. Host shell syntax and git whitespace checks pass; raw logs are excluded from whitespace normalization.

Limitations at this gate: no userspace workflow or interrupts yet. Injection covers post-map unwind, not every possible allocation/API failure. KVM is available as a device node but is not used or verified.

## Increment 2: first end-to-end workflow
Same build/run command: PASS. `02-liveness.log` records load -> C++ open -> four register inversion patterns -> invalid ioctl command/size/direction and null pointer rejection -> close -> unload. Reload liveness 0xdeadbeef -> 0x21524110 also passed. C++ built static with -Wall -Wextra -Werror; module W=1. Initial build found removed `no_llseek` API; corrected to NULL llseek before committing. Static lifetime review found no concrete open/remove/kref issue; this is advisory, not runtime removal evidence.

Limitations: native x86-64 ABI only. Runtime removal, interrupts, and recovery were not yet verified at this gate.

## Increment 3: interrupt-driven factorial
Same build/run command: PASS (`03-factorial.log`). All 0..12 factorials checked against userspace calculations over 130 repeated requests; invalid 13 and u32-max return EINVAL; four simultaneous independent handles completed another 100 exact-result requests. Only the acknowledged real FACT_IRQ path signals completion; no polling path manufactures success. First workflow and clean unload/reload still pass. Build W=1 and C++ warnings-as-errors passed.

QEMU v11.1.2 source was inspected: FACT_IRQ=1; COMPUTING clears before notification is raised. This supports retaining ownership through both idle and acknowledged IRQ. The one-second bound and pending guard are implemented with interrupts as a coherent safety requirement; deterministic timeout testing belongs to the next gate.

Limitations: INTx only, not MSI. Permanent lost notification requires quarantine/guest restart; rebind after unresolved hardware computation is not a supported recovery path. No arbitrary hardware failure or physical unplug validation.
