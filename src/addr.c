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

#include <asm/io.h>
#include <linux/debugfs.h>

uint64_t vaddr;
uint64_t paddr;

static int addr_va_read_op(void *data, uint64_t *value) {
  *value = vaddr;
  return 0;
}
static int addr_va_write_op(void *data, uint64_t value) {
  vaddr = value;
  paddr = virt_to_phys((void *)vaddr);
  return 0;
}

static int addr_pa_read_op(void *data, uint64_t *value) {
  *value = paddr;
  return 0;
}

static int addr_pa_write_op(void *data, uint64_t value) {
  paddr = value;
  vaddr = (uint64_t)phys_to_virt(paddr);
  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(addr_va_fops, addr_va_read_op, addr_va_write_op,
                         "0x%llx\n");

DEFINE_DEBUGFS_ATTRIBUTE(addr_pa_fops, addr_pa_read_op, addr_pa_write_op,
                         "0x%llx\n");

static int addr_init(struct dentry *parent) {
  debugfs_create_file("va", 0666, parent, NULL, &addr_va_fops);
  debugfs_create_file("pa", 0666, parent, NULL, &addr_pa_fops);

  return 0;
}

static char *const HELP =
    "$ echo 0xffffff800468b400 > /d/art/addr/va\n"
    "$ cat /d/art/addr/pa\n"
    "0x4468b400\n"
    "\n"
    "$ echo 0x4468b400 > /d/art/addr/pa\n"
    "$ cat /d/art/addr/va\n"
    "0xffffff800468b400\n";

REGISTER_ART_PLUGIN(addr, HELP, addr_init, NULL);
