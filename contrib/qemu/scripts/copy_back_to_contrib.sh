#!/bin/bash

# run this if you make changes to qemu and want to commit them back into ML tree

QEMU_PATH=${QEMU_PATH:=qemu-2.9.0}
ML=${ML:=magic-lantern}

cp -v *.sh *.py *.gdb gdbopts ../$ML/contrib/qemu/scripts
cp -v --parents */*.gdb ../$ML/contrib/qemu/scripts
cp -v $QEMU_PATH/hw/eos/* ../$ML/contrib/qemu/eos
cp -v $QEMU_PATH/hw/eos/mpu_spells/* ../$ML/contrib/qemu/eos/mpu_spells
cp -v --parents tests/*.sh ../$ML/contrib/qemu/
cp -v --parents tests/*/*.md5 ../$ML/contrib/qemu/

cd $QEMU_PATH
git diff > ../../$ML/contrib/qemu/$QEMU_PATH.patch
cd ..

cd ../$ML/contrib/qemu/
hg diff .
