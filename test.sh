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

AARCH64=0
if [[ $(uname -m) == aarch64 ]]; then
    AARCH64=1
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

log "Test pmem"
echo $PA > "${MODULE_DIR}/pmem/addr"
# assert_eq pmem/addr $PA
echo 0xbabecafedeadbeef > "${MODULE_DIR}/vmem/val"
assert_eq pmem/val 0xbabecafedeadbeef

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

echo -e "${GREEN}[+] All tests passed ${NC}"
