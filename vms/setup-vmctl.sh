#!/bin/bash
# WARNING: This deletes every folder created by vmctl and creates a complete fresh install.

VMCTLLOC=$HOME/Repositories/vmctl/
ARCHLOC=$HOME/Repositories/archbase/

sudo rm -rf img/nvme.qcow2 log/ run/ state/

IMG=$PWD/img/
if [[ ! -d "$IMG" ]]
then
    echo "Creating img folder."
    mkdir img
fi

SEED=$PWD/img/seed.img
if [ ! -f "$SEED" ]; then
    cd img
    $VMCTLLOC/contrib/generate-cloud-config-seed.sh $HOME/.ssh/id_rsa.pub
    cd ..
fi

BASE=$PWD/img/base.qcow2
if [ ! -f "$BASE" ]; then
    cd img
    echo "Getting image"
    # wget https://cloud-images.ubuntu.com/focal/current/focal-server-cloudimg-amd64.img
    cp $ARCHLOC/archbase.qcow2 base.qcow2
    # echo "Renaming to base.qcow2"
    # mv focal-server-cloudimg-amd64.img base.qcow2
    echo "Resizing to 8G"
    qemu-img resize base.qcow2 8G
    cd ..
fi

