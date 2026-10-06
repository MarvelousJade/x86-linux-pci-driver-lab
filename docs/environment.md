# Reproducible environment

The development host is Arch Linux under WSL2, x86-64. Initial cached package metadata returned HTTP 404 for QEMU/kernel packages; the transaction failed before installing anything. We resolved current packages using a separate pacman database and extracted signed packages under `/opt/pci-lab-tools`. No system upgrade or WSL kernel replacement was done.

```
bash scripts/isolated-arch-tools.sh
source scripts/lab-env.sh
bash scripts/build-guest.sh
bash scripts/run-guest.sh
```

The environment file pins the observed guest kernel directory; if downloading newer rolling packages, adjust KDIR and KERNEL to the directory actually extracted. `packages.txt` in the tools directory records resolved filenames. Source scripts do not pin mirrors forever. Store generated environment/evidence when reproducing.

QEMU uses TCG by default, two CPUs, 256 MiB, two EDU devices to test single-instance rejection, no network or disk. The guest uses the extracted Linux kernel, static BusyBox and our module/test utility. It mounts proc, sysfs and devtmpfs. Successful serial boot must print LAB PASS and contain no BUG/Oops/WARNING/panic. A host timeout, failed command or missing PASS fails the run.

Kernel modules built out of tree cannot use spaces in kbuild's M path. `build-module.sh` stages the module into a fresh temporary directory and copies the .ko back, preserving this workspace's path. Never load the .ko into WSL: headers target the QEMU guest.

Versions actually observed so far: host GCC 14.2.1; isolated GCC 16.2.1 20260810; QEMU 11.1.2; guest kernel package 6.18.55-1-lts; static BusyBox 1.36.1. Runtime verification is pending; exact generated output will be recorded after successful boot.
