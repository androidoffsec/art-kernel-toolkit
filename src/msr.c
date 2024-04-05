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

#include "art.h"
#include "asm.h"
#include "linux/printk.h"

#include <linux/ctype.h>
#include <linux/debugfs.h>

static cpumask_t cpu_mask = CPU_MASK_CPU0;

struct sysreg {
  const char *name;
  uint32_t op0;
  uint32_t op1;
  uint32_t CRn;
  uint32_t CRm;
  uint32_t op2;
};

static struct sysreg sysreg = {.op0 = 2};

static struct sysreg sysregs[] = {
    {"sctlr_el1", 3, 0, 1, 0, 0},
};

static uint32_t msr_mrs_encode(uint32_t ins) {
  uint32_t o0 = sysreg.op0 - 2;

  ins |= o0 << 19;
  ins |= sysreg.op1 << 16;
  ins |= sysreg.CRn << 12;
  ins |= sysreg.CRm << 8;
  ins |= sysreg.op2 << 5;

  return ins;
}

static int msr_read_op(void *data, uint64_t *value) {
  int res;
  struct arm64_regs regs = {0};

  // MRS x0, <sysreg>
  uint32_t mrs_ins = msr_mrs_encode(0xd5300000);

  if (cpumask_weight(&cpu_mask) != 1) {
    pr_err("Exactly one CPU must be selected\n");
    return -EINVAL;
  }

  res = exec_asm((uint8_t *)&mrs_ins, sizeof(mrs_ins), &regs, &cpu_mask);
  if (res < 0) {
    return res;
  }

  *value = regs.x0;

  return res;
}

static int msr_write_op(void *data, uint64_t value) {
  // Unused, but required by `exec_asm`
  struct arm64_regs regs = {0};

  uint32_t insns[] = {
      // LDR x0, $pc+16'
      0x58000080,
      // MSR <sysreg>, x0
      msr_mrs_encode(0xd5100000),
      // RET
      0xd65f03c0,
      // padding
      0x00000000,
      // <value>
      value & 0xFFFFFFFF,
      value >> 32,
  };

  return exec_asm((uint8_t *)&insns, sizeof(insns), &regs, &cpu_mask);
}

DEFINE_DEBUGFS_ATTRIBUTE(msr_fops, msr_read_op, msr_write_op, "0x%llx\n");

static int parse_regname(char *regname) {
  uint32_t tmp[5];
  int i;
  char *s = regname;

  // Convert `regname` to lowercase, and terminate string at newline
  while (*s) {
    if (*s == '\n') {
      *s = '\0';
      break;
    }
    *s = tolower(*s);
    s++;
  }

  for (i = 0; i < ARRAY_SIZE(sysregs); i++) {
    pr_debug("Comparing %s to %s\n", regname, sysregs[i].name);
    if (strcmp(regname, sysregs[i].name) == 0) {
      sysreg.op0 = sysregs[i].op0;
      sysreg.op1 = sysregs[i].op1;
      sysreg.CRn = sysregs[i].CRn;
      sysreg.CRm = sysregs[i].CRm;
      sysreg.op2 = sysregs[i].op2;
      return 0;
    }
  }

  // Replace non-numeric characters with spaces
  s = regname;
  while (*s) {
    if (*s < '0' || *s > '9') {
      *s = ' ';
    }
    s++;
  }

  if (sscanf(regname, "%u %u %u %u %u", &tmp[0], &tmp[1], &tmp[2], &tmp[3],
             &tmp[4])
      != 5) {
    return -EINVAL;
  }

  sysreg.op0 = tmp[0];
  sysreg.op1 = tmp[1];
  sysreg.CRn = tmp[2];
  sysreg.CRm = tmp[3];
  sysreg.op2 = tmp[4];

  return 0;
}

static ssize_t regname_write_op(struct file *fp, const char __user *user_buffer,
                                size_t count, loff_t *position) {
  char regname[256] = {0};
  int bytes_written;
  int res;

  bytes_written = simple_write_to_buffer(regname, sizeof(regname), position,
                                         user_buffer, count);
  if (bytes_written < 0) {
    return bytes_written;
  }

  res = parse_regname(regname);
  if (res < 0) {
    return res;
  }

  return bytes_written;
}

static ssize_t regname_read_op(struct file *fp, char __user *user_buffer,
                               size_t count, loff_t *position) {
  char regname[256] = {0};
  int available;

  available =
      snprintf(regname, sizeof(regname), "s%u_%u_c%u_c%u_%u\n", sysreg.op0,
               sysreg.op1, sysreg.CRn, sysreg.CRm, sysreg.op2);

  return simple_read_from_buffer(user_buffer, count, position, regname,
                                 available);
}

struct file_operations regname_fops = {
    .read = regname_read_op,
    .write = regname_write_op,
};

static int msr_init(struct dentry *parent) {
  debugfs_create_file("msr", 0666, parent, NULL, &msr_fops);
  debugfs_create_ulong("cpumask", 0666, parent, &cpumask_bits(&cpu_mask)[0]);
  debugfs_create_file("regname", 0666, parent, NULL, &regname_fops);
  debugfs_create_u32("op0", 0666, parent, &sysreg.op0);
  debugfs_create_u32("op1", 0666, parent, &sysreg.op1);
  debugfs_create_u32("CRn", 0666, parent, &sysreg.CRn);
  debugfs_create_u32("CRm", 0666, parent, &sysreg.CRm);
  debugfs_create_u32("op2", 0666, parent, &sysreg.op2);

  return 0;
}

static char *const HELP =
    "# Read SCTLR_EL1\n"
    "$ echo sctlr_el1 > /d/art/msr/regname\n"
    "$ cat /d/art/msr/regname\n"
    "s3_0_c1_c0_0\n"
    "$ cat /d/art/msr/msr\n"
    "0x200000034f4d91d\n"
    "\n"
    "# Set cpumask to CPU 0 and CPU 1\n"
    "$ echo 0x3 > /d/art/msr/cpumask\n"
    "\n"
    "# Disable EPAN and SPAN on CPU 0 and CPU 1\n"
    "$ echo 0x3474d91d > /d/art/msr/msr\n"
    "\n"
    "# Set CPU mask back to individual CPUs when reading\n"
    "$ echo 0x1 > /d/art/msr/cpumask\n"
    "\n"
    "# EPAN and SPAN are now unset on CPU 0 and CPU 1\n"
    "$ cat /d/art/msr/msr\n"
    "0x3474d91d\n"
    "\n"
    "$ echo 0x2 > /d/art/msr/cpumask\n"
    "$ cat /d/art/msr/msr\n"
    "0x3474d91d\n"
    "\n"
    "# SCTLR_EL1 is unchanged on CPU 2\n"
    "$ echo 0x4 > /d/art/msr/cpumask\n"
    "$ cat /d/art/msr/msr\n"
    "0x200000034f4d91d";

REGISTER_ART_PLUGIN(msr, HELP, msr_init, NULL);
