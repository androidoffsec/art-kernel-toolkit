#!/bin/sh
#
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

MODULE_DIR=/sys/kernel/debug/art

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

fail() {
    echo -e "${RED}Test failed${NC}"
    exit 1
}

log() {
    echo -e "${YELLOW}[+] $1${NC}"
}

assert_eq() {
    local plugin_path="$1"
    local expected_val="$2"

    actual_val=$(cat "${MODULE_DIR}/${plugin_path}")
    echo -n "Asserting ${plugin_path} value ${actual_val} equals expected value of ${expected_val}... "
    [ "$actual_val" == "$expected_val" ] || fail
    echo -e "${GREEN}Success${NC}"
}

to_uppercase() {
    echo $1 | tr "a-z" "A-Z"
}

to_lowercase() {
    echo $1 | tr "A-A" "a-z"
}

divide_hex() {
    local arg1="$1"
    local arg2="$2"

    # Convert arguments to uppercase and strip any leading 0x prefix in order to
    # use `bc`
    arg1="$(to_uppercase $arg1)"
    arg1="${arg1#0X}"

    arg2="$(to_uppercase $arg2)"
    arg2="${arg2#0X}"

    # Compute result of division, the output is in base 10
    local result=$(echo "obase=10; ibase=16; $arg1 / $arg2" | bc)

    # Output result in lowercase with leading 0x prefix
    printf "0x%x\n" $result
}

KERNEL_VERSION=$(uname -r | cut -d'-' -f1)

AARCH64=0
if [[ $(uname -m) == aarch64 ]]; then
    AARCH64=1
fi

ACK=0
if grep "lede.ack=1" /proc/cmdline > /dev/null 2>&1 ; then
    ACK=1
fi

QEMU=0
if grep "art-kt-qemu-test" /proc/cmdline > /dev/null 2>&1 ; then
    QEMU=1
fi

log "Test whether module is loaded"
lsmod | grep art_kernel_toolkit || fail

log "Test whether module directory exists"
test -d "${MODULE_DIR}" || fail

log "Test kmalloc alloc"
echo 0x400 > "${MODULE_DIR}/kmalloc/alloc"
assert_eq kmalloc/size 0x400

VA=$(cat "${MODULE_DIR}/kmalloc/va")
# echo $VA | grep 0xffff || fail

log "Verifying PFN and physical address"
PAGE_SIZE=0x1000
PA=$(cat "${MODULE_DIR}/kmalloc/pa")
assert_eq kmalloc/pfn $(divide_hex "$PA" $PAGE_SIZE)

log "Test addr"
echo $VA > "${MODULE_DIR}/addr/va"
assert_eq addr/pa $PA
echo $PA > "${MODULE_DIR}/addr/pa"
assert_eq addr/va $VA

log "Test kaslr"
cat "${MODULE_DIR}/kaslr/offset"

log "Test vmem"
echo $VA > "${MODULE_DIR}/vmem/addr"
assert_eq vmem/addr $VA
echo 0xdeadbeefbabecafe > "${MODULE_DIR}/vmem/val"
assert_eq vmem/val 0xdeadbeefbabecafe

log "Test pmem (ram)"
echo $PA > "${MODULE_DIR}/pmem/addr"
# assert_eq pmem/addr $PA
echo 0xbabecafedeadbeef > "${MODULE_DIR}/vmem/val"
assert_eq pmem/val 0xbabecafedeadbeef

# The "pmem read (mmio)" test reads an ARM specific register, so only run it on
# ARM. Only run on QEMU since the register address might change on other systems
if [[ $AARCH64 == 1 ]] && [[ $QEMU == 1 ]]; then
    log "Test pmem read (mmio)"
    # GIC_DIST base of 0x08000000 plus offset of 8
    GICD_IIDR_ADDR=0x0000000008000008
    echo $GICD_IIDR_ADDR > "${MODULE_DIR}/pmem/addr"
    assert_eq pmem/addr $GICD_IIDR_ADDR
    assert_eq pmem/val 0x43b # JEP 106 code for ARM
else
    log "Skipping pmem read (mmio) test"
fi

log "Test pmem bytes"
echo $PA > "${MODULE_DIR}/pmem/addr"
echo -n '56 67 89 ab cd ef' | xxd -r -p > /d/art/pmem/bytes

echo 6 > "${MODULE_DIR}/pmem/bytes-read-size"
actual_val=$(xxd -p /d/art/pmem/bytes)
expected_val=566789abcdef
echo -n "Asserting $actual_val equals $expected_val... "
[ "$actual_val" == "$expected_val" ] || fail
echo -e "${GREEN}Success${NC}"

log "Test kallsyms lookup"
SYM_NAME=__sys_setuid
echo $SYM_NAME > "${MODULE_DIR}/kallsyms/lookup_name"
assert_eq kallsyms/lookup_name $SYM_NAME
setuid_addr=$(cat "${MODULE_DIR}/kallsyms/addr")
echo $setuid_addr | grep ffff || fail

log "Test kmalloc free"
echo $VA > "${MODULE_DIR}/kmalloc/free"

# Only run SMC tests for aarch64. Don't run this when running QEMU as there may
# not be any EL3 handler for it
if [[ $AARCH64 == 1 ]] && [[ $QEMU == 0 ]]; then
    log "Test SMC"
    # Execute SMCCC_VERSION with some unused arguments in different formats
    echo 0x80000000 0xdeadbeef 0777 42 > "${MODULE_DIR}/smc/cmd"
    # Result is SMC Version 1.2, unused arguments are returned as is
    assert_eq smc/result "0x10002 0xdeadbeef 0x1ff 0x2a"
else
    log "Skipping SMC test"
fi

if [[ $AARCH64 == 1 ]]; then
    log "Test asm"

    assert_eq asm/x0 0x0000000000000000
    assert_eq asm/x9 0x0000000000000000
    assert_eq asm/x28 0x0000000000000000

    # mov x0, 042; mov x9, 42; mov x28, 0x42
    ASM_CODE="400480d2490580d25c0880d2"

    # For some reason if the `xxd > /d/art/asm/asm` write fails, the exit code is
    # still zero. This doesn't happen for `cat`, so we write to a temporary file and
    # `cat` that into the `asm` file
    echo $ASM_CODE | xxd -r -p > /tmp/asm_code

    cat /tmp/asm_code > "${MODULE_DIR}/asm/asm"

    assert_eq asm/x0 0x0000000000000022
    assert_eq asm/x9 0x000000000000002a
    assert_eq asm/x28 0x0000000000000042

    # Verify zero and multi-CPU asm writes fails
    echo 0 > "${MODULE_DIR}/asm/cpumask"
    cat /tmp/asm_code > "${MODULE_DIR}/asm/asm" && fail

    echo 0x3 > "${MODULE_DIR}/asm/cpumask"
    cat /tmp/asm_code > "${MODULE_DIR}/asm/asm" && fail

    log "Test msr"

    # Set register by writing in individual fields
    echo 3 > "${MODULE_DIR}/msr/op0"
    echo 1 > "${MODULE_DIR}/msr/op1"
    echo 1 > "${MODULE_DIR}/msr/CRn"
    echo 1 > "${MODULE_DIR}/msr/CRm"
    echo 1 > "${MODULE_DIR}/msr/op2"
    assert_eq msr/regname s3_1_c1_c1_1

    # Set register by writing uppercase and lowercase common names
    echo sctlr_el1 > "${MODULE_DIR}/msr/regname"
    assert_eq msr/regname s3_0_c1_c0_0

    assert_eq msr/op0 3
    assert_eq msr/op1 0
    assert_eq msr/CRn 1
    assert_eq msr/CRm 0
    assert_eq msr/op2 0

    echo SCTLR_EL1 > "${MODULE_DIR}/msr/regname"
    assert_eq msr/regname s3_0_c1_c0_0

    # For some reason, EPAN is not enabled for android13-5.10-lts kernels
    if [[ $ACK == 1 ]] && [[ $KERNEL_VERSION == 5.10.* ]]; then
        # Enabled flags: UCI SPAN TSCXT NTWE UCT DZE I SED SA0 SA C M
        SCTLR_EL1_VAL=0x34f4d91d
    else
        # Enabled flags: EPAN UCI SPAN TSCXT NTWE UCT DZE I SED SA0 SA C M
        SCTLR_EL1_VAL=0x200000034f4d91d
    fi

    # Verify register value
    assert_eq msr/msr $SCTLR_EL1_VAL

    # Verify zero and multi-CPU reads fails
    echo 0 > "${MODULE_DIR}/msr/cpumask"
    cat "${MODULE_DIR}/msr/msr" && fail

    echo 0x3 > "${MODULE_DIR}/msr/cpumask"
    cat "${MODULE_DIR}/msr/msr" && fail

    # Set cpumask back to its original value
    echo 0x1 > "${MODULE_DIR}/msr/cpumask"

    # Disable EPAN and SPAN
    echo 0x3474d91d > "${MODULE_DIR}/msr/msr"
    assert_eq msr/msr 0x3474d91d

    # The original value should still be set on a different CPU
    echo 0x2 > "${MODULE_DIR}/msr/cpumask"
    assert_eq msr/msr $SCTLR_EL1_VAL
else
    log "Skipping asm test"
    log "Skipping msr test"
fi

echo -e "${GREEN}[+] All tests passed ${NC}"
