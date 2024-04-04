/*
 * Copyright (C) 2024 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

.global exec_code
.type exec_code, %function

// x0 contains the address to jump to and x1 contains the address of
// the `struct arm64_regs`
exec_code:
    // Preserve callee-saved registers
    stp x29, x30, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!

    // The code we jump to may not necessarily follow proper calling convention,
    // so we need to save x1 and restore it after the call
    str x1, [sp, #-16]!

    // Call the code at address in x0
    blr x0

    // Restore the saved x1 into x29, as we don't want to clobber the x1 that
    // was set when calling the user's assembly code
    ldr x29, [sp], #16

    // Populate the `struct arm64_regs` with the state of the registers
    stp x0, x1, [x29, #0]
    stp x2, x3, [x29, #16]
    stp x4, x5, [x29, #32]
    stp x6, x7, [x29, #48]
    stp x8, x9, [x29, #64]
    stp x10, x11, [x29, #80]
    stp x12, x13, [x29, #96]
    stp x14, x15, [x29, #112]
    stp x16, x17, [x29, #128]
    stp x18, x19, [x29, #144]
    stp x20, x21, [x29, #160]
    stp x22, x23, [x29, #176]
    stp x24, x25, [x29, #192]
    stp x26, x27, [x29, #208]
    str x28, [x29, #224]

    // Restore callee-saved registers
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16

    ret
