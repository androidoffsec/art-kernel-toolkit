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

#include <linux/debugfs.h>

static uint32_t op0 = 2;
static uint32_t op1;
static uint32_t CRn;
static uint32_t CRm;
static uint32_t op2;

static int msr_read_op(void *data, uint64_t *value) {
  struct arm_regs regs = {0};
  int res;

  // MRS x0, <sysreg>
  uint32_t mrs_ins = 0xd5300000;

  uint32_t o0 = op0 - 2;
  // s2_0_c7_c14_6
  // echo 2 > op0; echo 0 > op1; echo 7 > CRn; echo 14 > CRm; echo 6 > op2
  mrs_ins |= o0 << 19;
  mrs_ins |= op1 << 16;
  mrs_ins |= CRn << 12;
  mrs_ins |= CRm << 8;
  mrs_ins |= op2 << 5;

  res = exec_asm((char *)&mrs_ins, sizeof(mrs_ins), &regs);
  if (res) {
    return res;
  }

  *value = regs.x0;

  return res;
}

static int msr_write_op(void *data, uint64_t value) { return 0; }

DEFINE_DEBUGFS_ATTRIBUTE(msr_fops, msr_read_op, msr_write_op, "0x%llx\n");

static int parse_regname(char *regname) {
  uint32_t tmp[5];

  // Replace non-numeric characters with spaces
  char *s = regname;
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

  op0 = tmp[0];
  op1 = tmp[1];
  CRn = tmp[2];
  CRm = tmp[3];
  op2 = tmp[4];

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

  available = snprintf(regname, sizeof(regname), "s%u_%u_c%u_c%u_%u\n", op0,
                       op1, CRn, CRm, op2);

  return simple_read_from_buffer(user_buffer, count, position, regname,
                                 available);
}

struct file_operations regname_fops = {
    .read = regname_read_op,
    .write = regname_write_op,
};

static int msr_init(struct dentry *parent) {
  debugfs_create_file("msr", 0666, parent, NULL, &msr_fops);
  debugfs_create_file("regname", 0666, parent, NULL, &regname_fops);
  debugfs_create_u32("op0", 0666, parent, &op0);
  debugfs_create_u32("op1", 0666, parent, &op1);
  debugfs_create_u32("CRn", 0666, parent, &CRn);
  debugfs_create_u32("CRm", 0666, parent, &CRm);
  debugfs_create_u32("op2", 0666, parent, &op2);

  return 0;
}

// TODO: Note that regname values must be in decimal
static char *const HELP = "TODO";

REGISTER_ART_PLUGIN(msr, HELP, msr_init, NULL);
