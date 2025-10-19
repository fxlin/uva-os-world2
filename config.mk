# will be included by all targets, kernel & user apps

ARMGNU ?= aarch64-linux-gnu
CC = $(ARMGNU)-gcc-9
CPP = $(ARMGNU)-g++-9

VOS_HOME ?= /home/xzl/workspace-p1-kernel

VOS_REPO_PATH ?= $(VOS_HOME)/p1-kernel-next/

KERNEL_PATH ?= $(VOS_HOME)/p1-kernel-next/kernel
USER_PATH ?= $(VOS_HOME)/p1-kernel-next/usr

LIBC_HOME ?= $(VOS_HOME)/newlib
LIBC_INC_PATH ?= $(LIBC_HOME)/newlib/libc/include 
LIBC_BUILD_PATH ?= $(LIBC_HOME)/build

LIBVOS_PATH=${USER_PATH}/libvos
LIBVORBIS_PATH=${USER_PATH}/libvorbis
LIBMINISDL_PATH=${USER_PATH}/libminisdl

# for all built static libs 
LIB_BUILD_PATH=${USER_PATH}/build-lib

CRT0=${LIB_BUILD_PATH}/crt0.o
CRT0CPP=${LIB_BUILD_PATH}/crt0cpp.o
CRTI=${LIB_BUILD_PATH}/crti.o
CRTN=${LIB_BUILD_PATH}/crtn.o

# whether output verbose build commands? V=1 will do
ifeq ($(V),1)
        VB=
else
        VB=@
endif