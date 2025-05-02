#! /bin/sh

# Intel Puma Toolchain generation script
 
#
# Requirements for this script:
# - GNU Make 3.81 or better
# - unifdef application version 2.4
# - gcc version 3.4.6 
# - gawk/awk version 3.1.5
# - autotools version 2.65
# - module-init-tools 3.2.2
# - texinfo (last tested 4.13)


echo "Intel Puma Toolchain generation script"
echo "-------------------------------------"

export CROSS=
# Buildroot version
BUILDROOT_NAME=buildroot-2013.08.1
UCLIBC_NAME=uClibc-0.9.33  # used to search uClibc config file.
GCC_VERSION=4.7.3  # should match with the version in buildroot config file.
KERNEL_VERSION=2.6.39.3
# Installation directory. Override this variable for a different destination
INTEL_PUMA_TOOLCHAIN_INSTALL_DIR="${INTEL_PUMA_TOOLCHAIN_INSTALL_DIR:-/data/puma5-toolchain}"
# Download directory. Override this variable for a different destination. Directory will be created automatically
export BUILDROOT_DL_DIR="${BUILDROOT_DL_DIR:-`readlink -f $(dirname $0)`/dl}"
STRIP="$INTEL_PUMA_TOOLCHAIN_INSTALL_DIR/usr/bin/armeb-buildroot-linux-uclibcgnueabi-strip"

BUILDROOT_CONFIG=${BUILDROOT_NAME}.config
UCLIBC_CONFIG=${UCLIBC_NAME}.config

fatal() {
  echo "ERROR: $@"
  exit 1
}

log() {
  echo; echo "$@"; echo
}

log "`date`: Intel Puma Toolchain - Toolchain will be installed in:
$INTEL_PUMA_TOOLCHAIN_INSTALL_DIR

If another location is required, please press Ctrl-C now and define INTEL_PUMA_TOOLCHAIN_INSTALL_DIR"

if [ ! -d "$BUILDROOT_DL_DIR" ]; then
  log "If you have already downloaded buildroot packges, point to them with BUILDROOT_DL_DIR env var.
Current value BUILDROOT_DL_DIR=$BUILDROOT_DL_DIR"
fi

echo -n "."
sleep 1
echo -n "."
sleep 1
echo -n "."
sleep 1


KERNEL_HEADERS=`pwd`/$BUILDROOT_NAME/output/toolchain/linux/include

# These configurations were carefully tailored for Puma platform.
# DO NOT MODIFY!
export EXTRA_GCC1_CONFIG_OPTIONS="\
--disable-libmudflap \
--disable-libssp \
--disable-libgomp \
--with-gnu-as \
--disable-shared \
--without-headers \
--with-newlib \
--enable-symvers=gnu \
--enable-clocale=uclibc \
--with-interwork \
--enable-c99 \
--enable-long-long \
--enable-cross \
--disable-checking"

export EXTRA_GCC2_CONFIG_OPTIONS="\
--disable-libmudflap \
--disable-libssp \
--disable-libgomp \
--with-gnu-as \
--enable-threads=posix \
--enable-symvers=gnu \
--enable-clocale=uclibc \
--with-interwork \
--enable-c99 \
--enable-long-long \
--enable-cross \
--enable-checking=release"

# Basic definitions
export GCC_NO_MPFR=y
export BR2_GCC_TARGET_ARCH=armv6z

# Download buildroot in case it doesn't exist
if [ ! -f $BUILDROOT_NAME.tar.bz2 ]; then \
  log "`date`: Intel Puma Toolchain - Downloading buildroot"
  wget -v --proxy=on --passive-ftp -nd http://www.buildroot.org/downloads/$BUILDROOT_NAME.tar.bz2 ;\
fi

# Extract and patch buildroot
[ -n "$BUILDROOT_NAME" -a -d "$BUILDROOT_NAME" ] && rm -rf "$BUILDROOT_NAME"  # we can't patch it twice
log "`date`: Intel Puma Toolchain - Extract and patch buildroot"
tar xjf $BUILDROOT_NAME.tar.bz2
mkdir -p $BUILDROOT_NAME/package/gcc/$GCC_VERSION || fatal "Failed to create gcc patches dir"
cp "${BUILDROOT_NAME}-patches/gcc"/* $BUILDROOT_NAME/package/gcc/$GCC_VERSION/

log "`date`: Intel Puma Toolchain - Extract Intel Puma addins"


#create output directory
mkdir -p $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR || fatal "Failed to create install directory"
#cp -rL $BUILDROOT_NAME/build_armeb/ti-puma5/ $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR

# Copy configuation
if [ ! -f $BUILDROOT_CONFIG ]; then
  log "WARNING: Config file $BUILDROOT_NAME.config doesn't exist. If you have previous config then make copy with new name."
  touch uClibc.config
else
  cat $BUILDROOT_CONFIG  | \
    sed -e 's#$(INTEL_PUMA_TOOLCHAIN_INSTALL_DIR)#'$INTEL_PUMA_TOOLCHAIN_INSTALL_DIR'/#g' \
        -e "s#BR2_DEFAULT_KERNEL_VERSION=.*#BR2_DEFAULT_KERNEL_VERSION=\"${KERNEL_VERSION}\"#" \
        -e "s#BR2_DEFAULT_KERNEL_HEADERS=.*#BR2_DEFAULT_KERNEL_HEADERS=\"${KERNEL_VERSION}\"#" \
        -e "s#BR2_GCC_VERSION=.*#BR2_GCC_VERSION=\"${GCC_VERSION}\"#" > $BUILDROOT_NAME/.config
  cat $UCLIBC_CONFIG  | \
    sed -e 's#$(INTEL_PUMA_TOOLCHAIN_INSTALL_DIR)#'$INTEL_PUMA_TOOLCHAIN_INSTALL_DIR'/#g' \
        -e 's#KERNEL_HEADERS=#KERNEL_HEADERS=\"'$KERNEL_HEADERS'\"#g' > uClibc.config
fi



# Change dir, and configure buildroot
log "`date`: Intel Puma Toolchain - Configuring buildroot"
cd $BUILDROOT_NAME
make oldconfig || fatal "make oldconfig failed"

# Start the build
log "`date`: Intel Puma Toolchain - Starting buildroot compilation"
echo; echo; echo 

#Compile the build root
make || fatal "build root make failed. errors above.";

# Strip the output
log "`date`: Intel Puma Toolchain - Stripping output"
#find $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR/ -type f | xargs file | grep ELF | grep -v ARM | grep 'not stripped' | awk -F: '{print $1}' | xargs strip --strip-unneeded -R .note -R .comment
find $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR/ -type f | xargs file | grep ELF \
  | grep ARM | grep 'not stripped' | awk -F: '{print $1}' \
  | xargs "$STRIP" --strip-unneeded -R .note -R .comment

log "`date`: Intel Puma Toolchain - Done"

