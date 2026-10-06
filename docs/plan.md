# Implementation checklist and acceptance gates

This is a Linux driver tested against emulated hardware, not physical board bring-up or a BSP.

Preflight: parent directory is not a Git repository (.git only contains browser metadata). No root AGENTS.md or project instructions were found. Existing sibling repositories are outside scope; their changes are untouched. Create a separate project repository. Linux commands run in Arch Linux WSL2; background commands use Windows cmd -> wsl.exe.

1. Environment/discovery: boot an isolated initramfs Linux guest with EDU; verify 1234:11e8; probe/unload/reload; reject a second EDU. Record versions and real serial output.
2. Liveness: C module + fixed-width ioctl header + C++ utility; invert multiple patterns, reject bad commands/pointers; open/close/unload end to end.
3. Computation: 0..12 factorial (12! fits u32, 13! does not); initialize completion before start, ISR acknowledges bit 1; repeated and concurrent calls return exact results.
4. Recovery: bounded wait; retain ownership of timed-out computation until idle AND its interrupt has been consumed. No reset. Test forced timeout, late completion, retry, and missing notification.
5. Lifecycle: open handles survive unbind as objects, not hardware access; subsequent ioctls return ENODEV; in-flight removal and reload pass. Finish evidence, decisions, interview guide.
6. Only after baseline passes: separate intentionally defective learning checkpoints and separately stored solutions; regression fails on checkpoint and passes on main; return checkout to main.

Each increment: implement -> run applicable checks -> investigate/fix -> rerun -> inspect diff -> focused commit using agent identity. Never substitute compilation for integration. No push.

Sources read before implementation:
- https://www.qemu.org/docs/master/specs/edu.html
- https://docs.kernel.org/PCI/pci.html
- https://docs.kernel.org/driver-api/misc_devices.html
- https://docs.kernel.org/scheduler/completion.html
- https://docs.kernel.org/core-api/kref.html

Runtime provisioning is being attempted. Acceptance remains pending until actual guest logs exist.
