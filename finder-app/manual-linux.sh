#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
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
    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    # ---
fi

echo "Adding the Image in outdir"
cd "$OUTDIR"
if [ ! -e vmlinux ]
then
    cp linux-stable/vmlinux vmlinux
fi

echo "Creating the staging directory for the root filesystem"

if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p user/bin usr/lib usr/sbin
mkdir -p var/log
# ---

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO: make and install busybox(?)
    make distclean
    make defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
    # ---
else
    cd busybox
fi

# install busybox:
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

# ROOTFS:
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "Should match:"
ls -R ${FINDER_APP_DIR}/shared_lib

# TODO: Add library dependencies to rootfs
cp -r ${FINDER_APP_DIR}/shared_lib/lib/. lib/
cp -r ${FINDER_APP_DIR}/shared_lib/lib64/. lib64/

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 1 5

# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
make clean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} writer

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd "${OUTDIR}/rootfs/home"
cp -r ${FINDER_APP_DIR}/. .
rm -rf shared_lib

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root ./

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR"
find rootfs/ | cpio -H newc -ov --owner root:root > initramfs.cpio
gzip -f initramfs.cpio
