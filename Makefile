# Copyright (C) 2024 Google LLC
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

ifneq ($(KERNELRELEASE),)

obj-m += art-kernel-toolkit.o
art-kernel-toolkit-y := src/art.o
art-kernel-toolkit-y += src/addr.o
art-kernel-toolkit-y += src/kallsyms.o
art-kernel-toolkit-y += src/kaslr.o
art-kernel-toolkit-y += src/kmalloc.o
art-kernel-toolkit-y += src/mount.o
art-kernel-toolkit-y += src/pmem.o
art-kernel-toolkit-y += src/vmem.o
art-kernel-toolkit-y += src/art_debugfs.o

art-kernel-toolkit-$(CONFIG_ARM64) += src/arm_helpers.o
art-kernel-toolkit-$(CONFIG_ARM64) += src/asm.o
art-kernel-toolkit-$(CONFIG_ARM64) += src/msr.o

art-kernel-toolkit-$(CONFIG_ARM64) += src/hvc.o src/smc.o src/smccc.o

CWD := $(ROOT_DIR)/$(KERNEL_DIR)/$(M)

ccflags-y := -Wall -Werror
ccflags-y += -D'pr_fmt(fmt)=KBUILD_MODNAME ": %s(): " fmt, __func__'

ldflags-y := -T$(CWD)/layout.lds

# For some reason when building ACK for x86_64, we get some -Wframe-address
# warnings from the ftrace code, so we disable this warning
ccflags-$(CONFIG_X86_64) += $(call cc-disable-warning,frame-address,)

else

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(CURDIR)
W ?= 1

all: modules

modules_install: modules

modules modules_install clean help:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) W=$(W) $(@)

compile_commands.json: modules
	$(KERNEL_SRC)/source/scripts/clang-tools/gen_compile_commands.py -d $(KERNEL_SRC) $(M)

.PHONY: modules modules_install clean help

endif
