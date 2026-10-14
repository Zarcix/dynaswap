#!/bin/bash

export IMAGE_NAME="fs.qcow2"

qemu-system-x86_64 \
  -machine q35,accel=kvm \
  -cpu host \
  -smp 2 \
  -m 1G \
  -drive file=${IMAGE_NAME},format=qcow2,if=virtio,cache=none,aio=native \
  -net nic -net user \
  -virtfs local,path=../src,mount_tag=src,security_model=mapped-xattr \
  -virtfs local,path=../utils,mount_tag=utils,security_model=mapped-xattr \
  -nographic \
  -serial mon:stdio