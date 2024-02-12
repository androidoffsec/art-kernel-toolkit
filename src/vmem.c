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

#include "art_debugfs.h"

#include <linux/debugfs.h>

static uint64_t addr;

static int vmem_val_write_op(void *data, uint64_t value) {
  *(uint64_t *)addr = value;
  return 0;
}

static int vmem_val_read_op(void *data, uint64_t *value) {
  *value = *(uint64_t *)addr;
  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(vmem_val_fops, vmem_val_read_op, vmem_val_write_op,
                         "0x%llx\n");

static int vmem_init(struct dentry *parent) {
  debugfs_create_x64("addr", 0666, parent, &addr);
  debugfs_create_file("val", 0666, parent, NULL, &vmem_val_fops);

  return 0;
}

static char *const HELP =
    "# Write address to write to or read from to `addr`\n"
    "$ echo ffffffc009fa2378 > /d/art/vmem/addr\n"
    "\n"
    "# Read from `val` to read 64-bit hex value at address\n"
    "$ cat /d/art/vmem/val\n"
    "0xffffff80038db270\n"
    "\n"
    "# Write 64-bit hex value to `val` to write to address\n"
    "$ echo 0xdeadbeef > /d/art/vmem/val\n"
    "\n"
    "# Confirm write succeeded"
    "$ cat /d/art/vmem/val\n"
    "0xdeadbeef\n";

REGISTER_ART_PLUGIN(vmem, HELP, vmem_init, NULL);
