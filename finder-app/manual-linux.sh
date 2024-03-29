#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j4 HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    
fi

echo "Adding the Image in outdir"
sudo cp -a ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR} 

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

ROOTFS=${OUTDIR}/rootfs
if [ -d "${ROOTFS}" ]; then
    rm -r ${ROOTFS}
fi

mkdir -p "${ROOTFS}"
cd "${ROOTFS}"

mkdir bin dev stc home lib lib64 proc sbin sys tmp
mkdir -p usr/bin usr/sbin usr/lib var/log

cd "${OUTDIR}"

if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig   
else
    cd busybox
fi

make HOSTCC=gcc-9 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX="${OUTDIR}/rootfs" install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${ROOTFS}/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${ROOTFS}/bin/busybox | grep "Shared library"

SYSROOT=$(aarch64-none-linux-gnu-gcc --print-sysroot)
cd ${ROOTFS}

sudo cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib64
sudo cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib
sudo cp -a ${SYSROOT}/lib64/libm.so.6 lib64
sudo cp -a ${SYSROOT}/lib64/libresolv.so.2 lib64
sudo cp -a ${SYSROOT}/lib64/libc.so.6 lib64
sudo cp -a ${SYSROOT}/lib64/ld-2.31.so lib64
sudo cp -a ${SYSROOT}/lib64/libm-2.31.so lib64
sudo cp -a ${SYSROOT}/lib64/libresolv-2.31.so lib64
sudo cp -a ${SYSROOT}/lib64/libc-2.31.so lib64

sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

mkdir -p ${ROOTFS}/home/conf
cp -a ${FINDER_APP_DIR}/finder.sh  ${ROOTFS}/home
cp -a ${FINDER_APP_DIR}/conf/username.txt  ${ROOTFS}/home/conf
cp -a ${FINDER_APP_DIR}/conf/assignment.txt  ${ROOTFS}/home/conf
cp -a ${FINDER_APP_DIR}/finder-test.sh  ${ROOTFS}/home
cp -a ${FINDER_APP_DIR}/writer  ${ROOTFS}/home
cp -a ${FINDER_APP_DIR}/autorun-qemu.sh  ${ROOTFS}/home

cd ${ROOTFS}
sudo chown -R root:root *

find . | sudo cpio --quiet -H newc -o | gzip -9 -n >"${OUTDIR}/initramfs.cpio.gz"
