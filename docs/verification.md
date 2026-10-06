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

## Increment 4: timeout and recovery
Same build/run command: PASS (`04-recovery.log`). Root-only hold_completion defers software delivery after actual hardware IRQ acknowledgement. The actual kernel wait expired in 1023 ms; the next distinct request returned EBUSY while the old delivery was held. Releasing the hook allowed 12! (not old 8!) and subsequent 7! to pass. Liveness remained usable during quarantine. A signal interrupted a held completion wait, then recovery passed. /proc/interrupts records the EDU shared INTx line. All earlier baseline/concurrency/unload checks passed.

This is deliberately injected deferred notification, not slow hardware or mocked PCI/MMIO. Keeping the hold set demonstrates continued quarantine, not a reset. Device wait is bounded; mutex queue time is not. Removal with queued deferred work remains the next verification gate.

Final static review found no concrete driver blocker, but identified a signal-test evidence weakness: EINTR alone did not establish that hardware work had already started. The lifecycle increment strengthens synchronization before signalling and checks pending EBUSY after interruption. Shared /proc/interrupts counts indicate IRQ dispatch, not themselves EDU acknowledgement; this disposable guest's line has only edu_lab registered. ISR source and deferred hook establish acknowledgement. No reproduced driver bug is claimed from this review.

## Increment 5: lifecycle and full baseline
Command: `source scripts/lab-env.sh; bash scripts/verify-main.sh`. PASS: three fresh full guest runs plus focused selftest and recovery runs, rebuilt after strengthening the signal test. Logs `05-lifecycle.log`, `05-repeat-{2,3}.log` and `05-focused-*.log` preserve exact output. Device wait expiry was 1016/1014/1013 ms in the full runs.

Each full run includes: both post-map and post-IRQ probe-failure unwind; two EDU functions with exactly one bound; liveness/interface/boundary/repeat/concurrent tests; timeout and in-flight signal recovery; module unload refused while fd pins it; sysfs unbind and PCI remove with an open fd, active held wait and queued notifier; subsequent liveness/factorial/unknown ioctls on that fd return ENODEV; misc node disappears; after closing, unload and reload compute 12! correctly. IRQ line 11 has only edu_lab registered in these guests. No BUG/Oops/WARNING/panic appeared.

Static review is advisory; three repeats are evidence, not proof of all interleavings. Limitations remain in limitations.md. All baseline acceptance gates pass against emulated hardware.

## Increment 6: learning checkpoint provenance
A: learning-exercise-a (`0fc5d0b`) intentionally introduced after baseline; liveness passed, factorial failed with ETIMEDOUT in the real guest. Reference restoration `1f459b5` passed focused selftest and the full suite.

B: learning-exercise-b (`640d47d`) independently introduced after A's reference restoration; ordinary selftest passed, recovery regression failed at the required refusal check. Reference restoration `554eacc` passed focused recovery and full suite. Fault/fixed/baseline serial logs are stored as exercise-*.log. Root causes and patches are separately stored under solutions/; learner instructions contain no solutions or unrequested hints. These are agent-authored educational defects, not production incidents or learner discoveries.

Main's driver is byte-identical to verified-baseline and the final reference restorations. No faulty commit was merged into main. Final returned-main verification: `source scripts/lab-env.sh; bash scripts/verify-main.sh` PASS after rebuilding from main: three full guest runs and focused selftest/recovery. Logs `06-final-main-{1,2,3}.log`, `06-final-selftest.log`, `06-final-recovery.log`. Full-run wait expiry: 1011/1026/1006 ms. Both workflows, concurrency, timeout/late notification, in-flight signal recovery, active-request unbind/removal, detached fd errors and reload passed again. No BUG/Oops/WARNING/panic. `git diff --quiet verified-baseline -- driver include tools tests` and reverse patch applicability checks passed. Final diff was inspected before committing; no push or history rewrite.
