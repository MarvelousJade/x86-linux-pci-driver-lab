#!/bin/bash
# Optional reproducible setup for an Arch host with stale package indexes.
# Downloads signed packages; never installs into the host root or upgrades it.
set -euo pipefail
TOOLS=${TOOLS:-/opt/pci-lab-tools}
DB=${DB:-/tmp/pci-lab-pacman}
mkdir -p "$DB" "$TOOLS"
pacman --dbpath "$DB" -Syw --noconfirm qemu-system-x86 linux-lts linux-lts-headers busybox cpio gcc
pacman --dbpath "$DB" -Sp --print-format '%f' qemu-system-x86 linux-lts linux-lts-headers busybox cpio gcc > "$TOOLS/packages.txt"
while read -r package; do
    tar -xf "/var/cache/pacman/pkg/$package" -C "$TOOLS"
done < "$TOOLS/packages.txt"
mkdir -p "$TOOLS/wrappers"
# Invoke the extracted dynamic loader explicitly; don't leak library settings
# into host commands. QEMU firmware and loadable modules also need explicit paths.
for tool in qemu-system-x86_64 gcc g++ cpio pahole; do
    printf '#!/bin/bash\nexec %q --library-path %q %q "$@"\n' \
        "$TOOLS/usr/lib/ld-linux-x86-64.so.2" "$TOOLS/usr/lib" "$TOOLS/usr/bin/$tool" \
        > "$TOOLS/wrappers/$tool"
    chmod +x "$TOOLS/wrappers/$tool"
done
printf 'Tools extracted to %s. See docs/environment.md for use.\n' "$TOOLS"
