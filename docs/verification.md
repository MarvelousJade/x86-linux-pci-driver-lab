# Verification ledger

Only actual guest runs count as PCI/MMIO/IRQ evidence. Serial logs are verbatim console output (including CRLF). Unsigned out-of-tree module taint is expected; no kernel BUG/Oops/WARNING/panic is accepted.

## Increment 1: discovery/resources
Command: `source scripts/lab-env.sh; bash scripts/build-guest.sh; bash scripts/run-guest.sh`.
PASS under TCG, 2 vCPUs: guest Linux 6.18.55-1-lts, two EDU functions discovered, deliberate post-map probe failure returns EIO for both; subsequent load binds exactly one and rejects other with EBUSY; unload/reload/unload works. Identification register read 0x010000ed. `01-discovery.log`, `environment.txt`, `packages.txt` record evidence and exact packages. Module compiled W=1 without warnings.

Environment setup failures investigated within this uncommitted increment: stale mirror indexes (404, no packages installed); overly broad isolated-tool PATH selected incompatible Bash (fixed by wrappers only); missing pahole wrapper (added). None is a claimed hardware defect. Host shell syntax and git whitespace checks pass; raw logs are excluded from whitespace normalization.

Limitations at this gate: no userspace workflow or interrupts yet. Injection covers post-map unwind, not every possible allocation/API failure. KVM is available as a device node but is not used or verified.
