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

#ifndef ASM_H
#define ASM_H

// We only store registers up to x28, the others are not useful for us
struct arm_regs {
  unsigned long x0;
  unsigned long x1;
  unsigned long x2;
  unsigned long x3;
  unsigned long x4;
  unsigned long x5;
  unsigned long x6;
  unsigned long x7;
  unsigned long x8;
  unsigned long x9;
  unsigned long x10;
  unsigned long x11;
  unsigned long x12;
  unsigned long x13;
  unsigned long x14;
  unsigned long x15;
  unsigned long x16;
  unsigned long x17;
  unsigned long x18;
  unsigned long x19;
  unsigned long x20;
  unsigned long x21;
  unsigned long x22;
  unsigned long x23;
  unsigned long x24;
  unsigned long x25;
  unsigned long x26;
  unsigned long x27;
  unsigned long x28;
} __attribute__((packed));

int exec_asm(char *buf, size_t len, struct arm_regs *regs);

#endif  // ASM_H
