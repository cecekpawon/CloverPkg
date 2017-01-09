#!/bin/bash

# https://github.com/cecekpawon/CloverPkg
# Wed Jan  4 13:34:04 2017
# Taken from 'yod-CloverUpd.sh' (https://github.com/cecekpawon/OSXOLVED)

# HOME
# .
# ├── edk2
# │   ├── Build
# │   ├── CloverPkg
# │   ├── Conf
# ├── opt
# │   └── local

SRC="/Users/${USER}/src"
TOOLCHAIN_DIR="${SRC}/opt/local"

## Toolchain

GCC_BIN="${TOOLCHAIN_DIR}/cross/bin/x86_64-clover-linux-gnu-"
export GCC49_BIN="${GCC_BIN}"
export GCC5_BIN="${GCC_BIN}"
export NASM_PREFIX="${TOOLCHAIN_DIR}/bin/"
export CLANG_BIN="${SRC}/llvm-build/Release/bin/"
export LLVM_BIN="${SRC}/llvm-build/Release/bin/"
export MTOC_BIN="/usr/local/bin/"
export LC_ALL="en_US.UTF-8"
#export LANG="en_US.UTF-8"

F_REV_TXT="rev.txt"

CLOVER_PATH="`dirname "$0"`"
CLOVER_BASEPATH=`basename ${CLOVER_PATH}`

cd $CLOVER_PATH
CLOVER_REVISION=`git rev-list --count HEAD`
echo $CLOVER_REVISION > "${F_REV_TXT}"

cd ..
export WORKSPACE="`pwd`"
EDK2_REVISION=`git rev-list --count HEAD`
echo $EDK2_REVISION > "${F_REV_TXT}"
source ./edksetup.sh "BaseTools"

MYTOOLCHAIN=GCC5 # GCC49 GCC5 XCLANG XCODE5
#DEFINED_OPT="-D EMBED_APTIOFIX -D EMBED_FSINJECT"
DEFINED_OPT=

DEF_REVISION=0
CUSTOM_CONF_PATH="${CLOVER_PATH}/Conf"
CLOVER_BUILD_PATH="${WORKSPACE}/Build/CloverX64/RELEASE_${MYTOOLCHAIN}"
F_VER_H="${CLOVER_PATH}/Version.h"
CLOVER_VERSION="2.3k"
CLOVER_REVISION_SUFFIX="X64"
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
  XCLANG|LLVM)
      dLlvmBin="/usr/bin"
      dLlvmCloverBin="${SRC}/llvm-build/Release/bin"
      if [[ ! -x "${dLlvmCloverBin}/clang" && -x "${dLlvmBin}/clang" ]]; then
        mkdir -p "${dLlvmCloverBin}"
        ln -s "${dLlvmBin}/clang" "${dLlvmCloverBin}/clang"
      fi
    ;;
  #XCODE5)
  #  ;;
esac

cd "${CLOVER_PATH}"

[[ -d "${CLOVER_BUILD_PATH}" ]] && rm -rf "${CLOVER_BUILD_PATH}"

# Gen Version.h

gCloverCmd="build -s -p ${CLOVER_DSC} ${DEFINED_OPT} -a X64 -t ${MYTOOLCHAIN} -b RELEASE -n ${NUMBER_OF_PROCESSORS} -j ${CLOVER_LOG}"
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

# exec command

eval "${gCloverCmd}"

gBuildError=`echo $?`

# copy binary

if [[ $gBuildError -ne 1 ]]; then
  if [[ -d "${QEMU_EFI_PATH}" ]]; then
    dBuildDir="${CLOVER_BUILD_PATH}/X64"
    if [[ ${#gCloverDrivers[@]} -ne 0 ]]; then
      dDir="${QEMU_EFI_PATH}/CLOVER/drivers"
      [[ ! -d "${dDir}" ]] && mkdir -p "${dDir}"
      for drv in "${gCloverDrivers[@]}"
      do
        [[ -f "${dBuildDir}/${drv}.efi" ]] && cp "${dBuildDir}/${drv}.efi" "${dDir}"
      done
    fi
    [[ -f "${dBuildDir}/CLOVERX64.efi" ]] && cp "${dBuildDir}/CLOVERX64.efi" "${QEMU_EFI_PATH}/BOOT/BOOTX64.efi"
  fi
fi
