# Scope and limitations

- Linux driver tested against QEMU EDU emulated hardware. No physical board, electrical validation, full BSP, boot firmware, production deployment or customer impact.
- Native x86-64 ABI; one device instance; INTx only. No DMA, mmap, networking, power management, async queue or general hotplug tooling.
- The device computation wait is at most 1000 ms (subject to kernel scheduling); time queued for the mutex is not an end-to-end latency guarantee. Queue acquisition/wait may be interrupted by a signal.
- No documented EDU cancellation/reset. Timeout retains pending ownership. A permanently lost hardware IRQ or genuinely stuck device remains quarantined: restart the guest. Unbind safely detaches handles, but reusing hardware after unresolved computation is not supported. Successful reuse tests involve known completed/acknowledged hardware.
- The hold_completion hook is root-only, defaults off and delays software notification after a real acknowledged interrupt. It does not emulate slow or defective physical electronics. It requires delayed-work lifetime handling absent from an otherwise even smaller driver.
- Probe injection verifies post-map and post-IRQ unwind, not every possible failure of every kernel allocation/API. No mocked tests are used to claim integration.
- Tests exercise sysfs unbind and PCI remove in a disposable guest, not physical surprise removal. Existing fd pins the module, but not the PCI binding. No forced module unload is supported.
- Unsigned out-of-tree module taint is expected in this guest. No Secure Boot deployment, packaging for every distribution, KVM validation, stress benchmarks or proof of all possible scheduler interleavings.
- Rolling Arch download versions may change. Exact tested packages/versions and commands are retained; use matching guest headers and compiler. Static BusyBox and static C++ keep the guest small. Build/setup failures are not hardware incidents.
- Debugging exercises are intentionally introduced on separate checkpoints; never merge them into main. Learner solutions should be verified with actual guest runs.
