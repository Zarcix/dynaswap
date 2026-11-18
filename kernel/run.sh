#!/bin/bash
: << 'C'
This script assumes that you have a fully functioning rootfs to boot into. Without the rootfs-dir, this WILL NOT WORK
C

ROOTFS="$(realpath rootfs-dir)"
SWAP="$(realpath swap.img)"

qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -smp 2 \
    -m 2G \
    -kernel linux/arch/x86/boot/bzImage \
    -append "root=hostshare rootfstype=9p rootflags=trans=virtio,version=9p2000.L,cache=none rw console=ttyS0,115200n8 loglevel=7 nokaslr" \
    -fsdev local,id=rootfs,path="$ROOTFS",security_model=mapped-xattr \
    -device virtio-9p-pci,fsdev=rootfs,mount_tag=hostshare \
    -drive file="$SWAP",format=raw,if=virtio \
    -nographic \
    -gdb tcp::1234