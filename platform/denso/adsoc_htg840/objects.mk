#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Florent Kermarrec <florent@enjoy-digital.fr>
# Copyright (c) 2020 Dolu1990 <charles.papon.90@gmail.com>
# Copyright (c) 2024 SwK <karthikeyan.swaminathan.j7c@jp.denso.com>
#

# Compiler flags
platform-cppflags-y =
platform-cflags-y =
platform-asflags-y =
platform-ldflags-y =

# Objects to build
platform-objs-y += platform.o

# Command for platform specific "make run"
platform-runcmd = echo DENSO/ADSoC

PLATFORM_RISCV_XLEN = 64
PLATFORM_RISCV_ABI = lp64
PLATFORM_RISCV_ISA = rv64imafdc_zicsr_zifencei_zba_zbb_zbs
PLATFORM_RISCV_CODE_MODEL = medany

# Blobs to build
FW_TEXT_START?=0x80100000
FW_DYNAMIC=n
FW_JUMP=y
FW_JUMP_ADDR?=0x80200000
FW_JUMP_FDT_ADDR?=0x800F0000
FW_PAYLOAD=n
FW_PAYLOAD_OFFSET?=0x80200000
FW_PAYLOAD_FDT_ADDR?=0x800F0000

ifdef FW_HART_COUNT
platform-cflags-y += -DADSOC_DEFAULT_HART_COUNT=$(FW_HART_COUNT)
endif
