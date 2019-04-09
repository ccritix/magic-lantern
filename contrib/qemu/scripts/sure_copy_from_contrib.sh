#!/bin/bash

# run this if you want to revert to the QEMU version
# from the magic-lantern/contrib/qemu directory
# (e.g. an older changeset, or just to undo your edits)

QEMU_NAME=${QEMU_NAME:=qemu-2.5.0}
ML_PATH=${ML_PATH:=../magic-lantern}

# copy files added to QEMU sources
echo "copying files..."
cp -v $ML_PATH/contrib/qemu/eos/* $QEMU_NAME/hw/eos/
cp -v $ML_PATH/contrib/qemu/eos/mpu_spells/* $QEMU_NAME/hw/eos/mpu_spells/
cp -v $ML_PATH/contrib/qemu/eos/dbi/* $QEMU_NAME/hw/eos/dbi/
cp -v $ML_PATH/src/backtrace.[ch] $QEMU_NAME/hw/eos/dbi/
cp -vr $ML_PATH/contrib/qemu/tests/* tests/
cp -vr $ML_PATH/contrib/qemu/scripts/* .

if [ "$1" != "-q" ]; then
    # discard local changes and re-apply our patch
    DATE=$(date '+%Y-%m-%d_%H-%M-%S')
    cd $QEMU_NAME
    git diff > $QEMU_NAME-$DATE.patch
    echo "Local changes backed up to $QEMU_NAME/$QEMU_NAME-$DATE.patch"
    git checkout .
    patch -N -p1 < ../$ML_PATH/contrib/qemu/$QEMU_NAME.patch
fi
