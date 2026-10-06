# Source this for the optional isolated Arch tools setup.
export TOOLS=${TOOLS:-/opt/pci-lab-tools}
export PATH="$TOOLS/wrappers:$PATH"
export KDIR="$TOOLS/usr/lib/modules/6.18.55-1-lts/build"
export KERNEL="$TOOLS/usr/lib/modules/6.18.55-1-lts/vmlinuz"
export BUSYBOX="$TOOLS/usr/bin/busybox"
export QEMU_DATA="$TOOLS/usr/share/qemu"
export CC="$TOOLS/wrappers/gcc"
export CXX="$TOOLS/wrappers/g++"
