#!/bin/bash

QEMU_PATH=${QEMU_PATH:=qemu-1.6.0}
make -C $QEMU_PATH || exit
$QEMU_PATH/arm-softmmu/qemu-system-arm -M $1
