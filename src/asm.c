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

#include "asm.h"

#include "art.h"
#include "kallsyms.h"

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

extern void exec_code(uintptr_t code_addr, struct arm64_regs *regs);

static struct arm64_regs regs;
static cpumask_t cpu_mask = CPU_MASK_CPU0;

typedef int (*set_memory_x_t)(unsigned long addr, int numpages);
static set_memory_x_t __art_set_memory_x;

static __nocfi int art_set_memory_x(unsigned long addr, int numpages) {
  BUG_ON(!__art_set_memory_x);
  return __art_set_memory_x(addr, numpages);
}

struct exec_code_info {
  uintptr_t code_addr;
  struct arm64_regs *regs;
};

static void exec_code_smp_call_func(void *info) {
  struct exec_code_info *exec_info = (struct exec_code_info *)info;
  uintptr_t code_addr = exec_info->code_addr;
  struct arm64_regs *regs = exec_info->regs;

  pr_info("Jumping to shellcode at %lx", code_addr);
  exec_code(code_addr, regs);
}

int exec_asm(uint8_t *buf, size_t len, struct arm64_regs *regs,
             const cpumask_t *cpu_mask) {
  int res;
  uint32_t ret_ins = 0xd65f03c0;
  struct exec_code_info info;
  void (*exec_page)(void);

  size_t alloc_size = PAGE_ALIGN(len);
  size_t num_pages = alloc_size / PAGE_SIZE;

  // We can't use kmalloc with `set_memory_x`
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

  info.code_addr = (uintptr_t)exec_page;
  info.regs = regs;
  on_each_cpu_mask(cpu_mask, exec_code_smp_call_func, &info, true);

err:
  vfree(exec_page);
  return res;
}

static ssize_t asm_write_op(struct file *fp, const char __user *user_buffer,
                            size_t count, loff_t *position) {
  int res;
  int num_bytes;
  uint8_t *asm_code = kzalloc(count, GFP_KERNEL);

  if (cpumask_weight(&cpu_mask) != 1) {
    pr_err("Exactly one CPU must be selected\n");
    return -EINVAL;
  }

  num_bytes =
      simple_write_to_buffer(asm_code, count, position, user_buffer, count);
  if (num_bytes < 0) {
    return num_bytes;
  }

  res = exec_asm(asm_code, count, &regs, &cpu_mask);
  if (res < 0) {
    return res;
  }

  return num_bytes;
}

struct file_operations asm_fops = {.write = asm_write_op};

static int asm_init(struct dentry *parent) {
  int i;

  __art_set_memory_x = (set_memory_x_t)art_kallsyms_lookup_name("set_memory_x");
  if (!__art_set_memory_x) {
    pr_err("could not resolve set_memory_x function address\n");
    return -1;
  }

  debugfs_create_file("asm", 0222, parent, NULL, &asm_fops);
  debugfs_create_ulong("cpumask", 0666, parent, &cpumask_bits(&cpu_mask)[0]);

  // Create a read-only debugfs file for each register in the `regs` struct
  for (i = 0; i <= 28; i++) {
    char name[4];
    snprintf(name, sizeof(name), "x%d", i);
    debugfs_create_x64(name, 0444, parent, (uint64_t *)&regs + i);
  }

  return 0;
}

static char *const HELP =
    "# mov x0, 042; mov x9, 42; mov x28, 0x42\n"
    "$ echo 400480d2490580d25c0880d2 | xxd -r -p > /d/art/asm/asm\n"
    "$ cat /d/art/asm/x0\n"
    "0x0000000000000022\n"
    "$ cat /d/art/asm/x9\n"
    "0x000000000000002a\n"
    "$ cat /d/art/asm/x28\n"
    "0x0000000000000042\n";

REGISTER_ART_PLUGIN(asm, HELP, asm_init, NULL);
