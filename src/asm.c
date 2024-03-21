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
#include "kallsyms.h"

#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

extern void exec_code(uintptr_t code_addr, struct arm_regs *regs);

typedef int (*set_memory_x_t)(unsigned long addr, int numpages);

static set_memory_x_t __art_set_memory_x;

static __nocfi int art_set_memory_x(unsigned long addr, int numpages) {
  BUG_ON(!__art_set_memory_x);
  return __art_set_memory_x(addr, numpages);
}

// TODO: __nocfi here or not?
int exec_asm(char *buf, size_t len, struct arm_regs *regs) {
  int res;
  uint32_t ret_ins = 0xd65f03c0;
  void (*exec_page)(void);

  size_t alloc_size = PAGE_ALIGN(len);
  size_t num_pages = alloc_size / PAGE_SIZE;

  exec_page = vmalloc(alloc_size);
  if (!exec_page) {
    pr_err("Failed to allocate memory\n");
    return -ENOMEM;
  }
  pr_debug("exec_page: %lx, page_count: %zu", (uintptr_t)exec_page, num_pages);

  memcpy(exec_page, buf, len);
  memcpy(exec_page + len, &ret_ins, sizeof(ret_ins));

  res = art_set_memory_x((unsigned long)exec_page, num_pages);
  if (res) {
    pr_err("Failed to set memory as executable\n");
    goto err;
  }

  pr_info("Jumping to shellcode at %lx", (uintptr_t)exec_page);
  exec_code((uintptr_t)exec_page, regs);

err:
  vfree(exec_page);
  return res;
}

// static int msr_read_op(void *data, uint64_t *value) { return 0; }

// static int msr_write_op(void *data, uint64_t value) { return 0; }

// DEFINE_DEBUGFS_ATTRIBUTE(msr_fops, msr_read_op, msr_write_op, "0x%llx\n");

static int asm_init(struct dentry *parent) {
  __art_set_memory_x = (set_memory_x_t)art_kallsyms_lookup_name("set_memory_x");
  if (!__art_set_memory_x) {
    pr_err("could not resolve set_memory_x function address\n");
    return -1;
  }

  // debugfs_create_file("asm", 0666, parent, NULL, &asm_fops);

  return 0;
}

static char *const HELP = "TODO";

REGISTER_ART_PLUGIN(asm, HELP, asm_init, NULL);
