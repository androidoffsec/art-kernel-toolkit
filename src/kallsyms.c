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

#include "kallsyms.h"

#include "art.h"
#include "art_debugfs.h"

#include <asm-generic/errno-base.h>
#include <asm/insn.h>
#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/printk.h>
#include <linux/slab.h>

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t __art_kallsyms_lookup_name;

static char lookup_name[NAME_MAX + 1];
static uint64_t lookup_addr;

__nocfi unsigned long art_kallsyms_lookup_name(const char *name) {
  BUG_ON(!__art_kallsyms_lookup_name);
  return __art_kallsyms_lookup_name(name);
}
EXPORT_SYMBOL(art_kallsyms_lookup_name);

#ifdef CONFIG_KPROBES
static int find_kallsyms_lookup_name(void) {
  int ret = 0;
  struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};

  ret = register_kprobe(&kp);
  if (ret) {
    return ret;
  }

  __art_kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
  pr_info("found kallsyms_lookup_name at 0x%lx\n",
          (unsigned long)__art_kallsyms_lookup_name);

  unregister_kprobe(&kp);
  return ret;
}
#else

#include <asm/memory.h>

static int find_kallsyms_lookup_name(void) {
  unsigned long addr = kimage_vaddr;
  unsigned long end_addr = kimage_vaddr + SZ_32M;
  const uint8_t insn_size = AARCH64_INSN_SIZE;

  char *target_sym = "kallsyms_lookup_name";
  char *lookup_sym = kzalloc(NAME_MAX, GFP_KERNEL);

  pr_info("CONFIG_KPROBES is not set, falling back to manual search for %s",
          target_sym);

  if (!lookup_sym) {
    return -ENOMEM;
  }

  pr_info("starting search for %s at 0x%lx\n", target_sym, addr);
  while (addr < end_addr) {
    sprint_symbol(lookup_sym, addr);

    // sprint_symbol output will look like 'kallsyms_lookup_name+0x0/0x124', so
    // we must use strncmp here instead of strcmp to only compare the prefix
    if (strncmp(lookup_sym, target_sym, strlen(target_sym)) == 0) {
      __art_kallsyms_lookup_name = (kallsyms_lookup_name_t)addr;
      pr_info("found %s at 0x%lx\n", target_sym, addr);
      break;
    }

    addr += insn_size;
  }

  kfree(lookup_sym);

  if (__art_kallsyms_lookup_name == NULL) {
    return -EINVAL;
  }

  return 0;
}

#endif /* CONFIG_KPROBES */

static ssize_t kallsyms_lookup_name_read(struct file *fp,
                                         char __user *user_buffer, size_t count,
                                         loff_t *position) {
  ssize_t res;

  res = simple_read_from_buffer(user_buffer, count, position, lookup_name,
                                strlen(lookup_name));

  return res;
}

static ssize_t kallsyms_lookup_name_write(struct file *fp,
                                          const char __user *user_buffer,
                                          size_t count, loff_t *position) {
  int res;
  res = simple_write_to_buffer(lookup_name, NAME_MAX, position, user_buffer,
                               count);
  if (res <= 0) {
    lookup_addr = 0;
    return res;
  }

  // Set the newline to a null byte
  lookup_name[res - 1] = '\0';

  lookup_addr = art_kallsyms_lookup_name(lookup_name);
  return res;
}

static const struct file_operations lookup_name_fops = {
    .read = kallsyms_lookup_name_read, .write = kallsyms_lookup_name_write};

static int kallsyms_init(struct dentry *parent) {
  int res = find_kallsyms_lookup_name();
  if (res) {
    pr_warn("failed to find kallsyms_lookup_name\n");
    return res;
  }

  debugfs_create_file("lookup_name", 0666, parent, NULL, &lookup_name_fops);
  debugfs_create_x64("addr", 0444, parent, &lookup_addr);

  return res;
}

static char *const HELP =
    "$ echo __sys_setuid > /d/art/kallsyms/lookup_name\n"
    "$ cat /d/art/kallsyms/addr\n"
    "0xffffffedb417da48\n";

REGISTER_ART_PLUGIN(kallsyms, HELP, kallsyms_init, NULL);
