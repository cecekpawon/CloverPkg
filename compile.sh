#!/bin/bash

# https://github.com/cecekpawon/CloverPkg
# Wed Jan  4 13:34:04 2017
# Taken from 'yod-CloverUpd.sh' (https://github.com/cecekpawon/OSXOLVED)

# $HOME                   (/Users/username/)
# .
# └── src
#     ├── edk2
#     │   ├── Build       (Compiled binaries)
#     │   ├── CloverPkg   (CloverPkg sources)
#     └── opt
#         └── local       (Build tools)

SRC="/Users/${USER}/src"
TOOLCHAIN_DIR="${SRC}/opt/local"

## Toolchain

GCC_BIN="${TOOLCHAIN_DIR}/cross/bin/x86_64-clover-linux-gnu-"
export GCC49_BIN="${GCC_BIN}"
export GCC5_BIN="${GCC_BIN}"
export TOOLCHAIN_BIN="${TOOLCHAIN_DIR}/bin"
export NASM_BIN="${TOOLCHAIN_BIN}/"
export NASM_PREFIX="${TOOLCHAIN_BIN}/"
export MTOC_BIN="${TOOLCHAIN_BIN}/"
export LC_ALL="en_US.UTF-8"
#export LANG="en_US.UTF-8"

EDK2_REVISION_MAGIC=8925
F_REV_TXT="rev.txt"

CLOVER_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CLOVER_BASEPATH=`basename ${CLOVER_PATH}`

cd $CLOVER_PATH
CLOVER_REVISION=`git rev-list --count HEAD`
echo $CLOVER_REVISION > "${F_REV_TXT}"

cd ..
export WORKSPACE="`pwd`"
if [[ -e "${F_REV_TXT}" ]]; then
  EDK2_REVISION=`cat "${F_REV_TXT}"`
else
  EDK2_REMOTE=`git ls-remote --get-url`
  EDK2_REVISION=`svn info $EDK2_REMOTE | sed -nE 's/^.*evision: *([0-9]+).*$/\1/p'`
  echo $((EDK2_REVISION - EDK2_REVISION_MAGIC)) > "${F_REV_TXT}"
fi
source ./edksetup.sh "BaseTools"

MYTOOLCHAIN=GCC5 # GCC49 GCC5 XCODE5
#BUILD_OPTIONS="-D EMBED_APTIOFIX -D EMBED_FSINJECT"
BUILD_OPTIONS=

DEF_REVISION=0
CUSTOM_CONF_PATH="${CLOVER_PATH}/Conf"
CLOVER_BUILD_PATH="${WORKSPACE}/Build/CloverPkg/RELEASE_${MYTOOLCHAIN}"
F_VER_H="${CLOVER_PATH}/Version.h"
CLOVER_VERSION="2.3k"
CLOVER_REVISION_SUFFIX=""
CLOVER_DSC="${CLOVER_PATH}/${CLOVER_BASEPATH}.dsc"
CLOVER_LOG="${CLOVER_PATH}/${CLOVER_BASEPATH}.log"
QEMU_EFI_PATH="/Volumes/XDATA/QVM/DISK/EFI"
NUMBER_OF_PROCESSORS=`sysctl -n hw.logicalcpu`

gCloverDrivers=("FSInject" "OsxAptioFixDrv")

[[ -e "${CUSTOM_CONF_PATH}/target.txt" ]] && export CONF_PATH=${CUSTOM_CONF_PATH:-}

case "${MYTOOLCHAIN}" in
  GCC49|GCC5)
      gMake=$(which make)
      gGNUmake="${GCC5_BIN}make"
      if [[ -x "${gMake}" && ! -x "${gGNUmake}" ]]; then
        ln -s "${gMake}" "${gGNUmake}"
      fi
    ;;
  #XCODE5)
  #    BUILD_OPTIONS="${BUILD_OPTIONS} -D NO_MSABI_VA_FUNCS"
  #  ;;
esac

export BUILD_OPTIONS

cd "${CLOVER_PATH}"

[[ -d "${CLOVER_BUILD_PATH}" ]] && rm -rf "${CLOVER_BUILD_PATH}"

# Gen Version.h

gCloverCmd="build -p ${CLOVER_DSC} ${BUILD_OPTIONS} -a X64 -t ${MYTOOLCHAIN} -b RELEASE -n ${NUMBER_OF_PROCESSORS} -j ${CLOVER_LOG}"
gCloverCmdStr=""
for c in $gCloverCmd; do gCloverCmdStr="${gCloverCmdStr} ${c##*/}"; done
read -rd '' gCloverCmdStr <<< "${gCloverCmdStr}"

gDate=$(date '+%Y-%m-%d %H:%M:%S')

[[ -f "${CLOVER_PATH}/${F_REV_TXT}" ]] && CLOVER_REVISION=`cat "${CLOVER_PATH}/${F_REV_TXT}" | sed -e 's/[^0-9\.]//g'`
[[ ! $CLOVER_REVISION =~ ^[0-9]+$ ]] && CLOVER_REVISION=$DEF_REVISION
CLOVER_REVISION="${CLOVER_REVISION}${CLOVER_REVISION_SUFFIX}"

[[ -f "${WORKSPACE}/${F_REV_TXT}" ]] && EDK2_REVISION=`cat "${WORKSPACE}/${F_REV_TXT}" | sed -e 's/[^0-9\.]//g'`
[[ ! $EDK2_REVISION =~ ^[0-9]+$ ]] && EDK2_REVISION=$DEF_REVISION

echo "#define CLOVER_BUILDDATE \"${gDate}\"" > "${F_VER_H}"
echo "#define EDK2_REVISION \"${EDK2_REVISION}\"" >> "${F_VER_H}"
echo "#define CLOVER_VERSION \"${CLOVER_VERSION}\"" >> "${F_VER_H}"
echo "#define CLOVER_REVISION \"${CLOVER_REVISION}\"" >> "${F_VER_H}"
echo "#define CLOVER_BUILDINFOS_STR \"${gCloverCmdStr}\"" >> "${F_VER_H}"

# clean build

[[ -d "${CLOVER_BUILD_PATH}" ]] && rm -rf "${CLOVER_BUILD_PATH}"

# exec command

eval "${gCloverCmd}"

gBuildError=`echo $?`

# copy binary

if [[ $gBuildError -ne 1 ]]; then
  if [[ -d "${QEMU_EFI_PATH}" ]]; then
    dBuildDir="${CLOVER_BUILD_PATH}/X64"
    if [[ ${#gCloverDrivers[@]} -ne 0 ]]; then
      dDir="${QEMU_EFI_PATH}/CLOVER/Driver"
      if [[ -d "${dDir}" ]]; then
        for drv in "${gCloverDrivers[@]}"
        do
          [[ -f "${dBuildDir}/${drv}.efi" ]] && cp "${dBuildDir}/${drv}.efi" "${dDir}"
        done
      fi
    fi
    [[ -f "${dBuildDir}/Clover.efi" ]] && cp "${dBuildDir}/Clover.efi" "${QEMU_EFI_PATH}/BOOT/BOOTX64.efi"
  fi
fi
