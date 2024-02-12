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

#include <linux/debugfs.h>
#include <linux/slab.h>

static void *page;
static uint64_t alloc_size;

static int kmalloc_alloc_write_op(void *data, uint64_t value) {
  alloc_size = value;
  page = kmalloc(alloc_size, GFP_KERNEL);

  if (page == NULL) {
    pr_err("allocation failed");
    return -1;
  }

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_alloc_fops, NULL, kmalloc_alloc_write_op,
                         "0x%llx\n");

static int kmalloc_free_write_op(void *data, uint64_t value) {
  kfree((void *)value);
  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_free_fops, NULL, kmalloc_free_write_op,
                         "0x%llx\n");

static int kmalloc_va_read_op(void *data, uint64_t *value) {
  if (page == NULL) {
    pr_err("no page has been allocated");
    return -1;
  }

  *value = (uint64_t)page;

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_va_fops, kmalloc_va_read_op, NULL, "0x%llx\n");

static int kmalloc_pa_read_op(void *data, uint64_t *value) {
  if (page == NULL) {
    pr_err("no page has been allocated");
    return -1;
  }

  *value = (uint64_t)__pa(page);

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_pa_fops, kmalloc_pa_read_op, NULL, "0x%llx\n");

static int kmalloc_pfn_read_op(void *data, uint64_t *value) {
  if (page == NULL) {
    pr_err("no page has been allocated");
    return -1;
  }

  *value = (uint64_t)__pa(page) >> 12;

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_pfn_fops, kmalloc_pfn_read_op, NULL,
                         "0x%llx\n");

static int kmalloc_size_read_op(void *data, uint64_t *value) {
  if (page == NULL) {
    pr_err("no page has been allocated");
    return -1;
  }

  *value = (uint64_t)alloc_size;

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kmalloc_size_fops, kmalloc_size_read_op, NULL,
                         "0x%llx\n");

static int kmalloc_init(struct dentry *parent) {
  debugfs_create_file("alloc", 0222, parent, NULL, &kmalloc_alloc_fops);
  debugfs_create_file("free", 0222, parent, NULL, &kmalloc_free_fops);

  debugfs_create_file("va", 0444, parent, NULL, &kmalloc_va_fops);
  debugfs_create_file("pa", 0444, parent, NULL, &kmalloc_pa_fops);
  debugfs_create_file("pfn", 0444, parent, NULL, &kmalloc_pfn_fops);
  debugfs_create_file("size", 0444, parent, NULL, &kmalloc_size_fops);

  return 0;
}

static char *const HELP =
    "# Allocate 1024 bytes\n"
    "$ echo 0x400 > /d/art/kmalloc/alloc\n"
    "\n"
    "$ cat /d/art/kmalloc/size\n"
    "0x400\n"
    "\n"
    "$ cat /d/art/kmalloc/va\n"
    "0xffffff8004048000\n"
    "\n"
    "$ cat /d/art/kmalloc/pa\n"
    "0x44048000\n"
    "\n"
    "$ cat /d/art/kmalloc/pfn\n"
    "0x44048\n"
    "\n"
    "# Free allocated memory\n"
    "$ echo $(cat /d/art/kmalloc/va) > /d/art/kmalloc/va\n";

REGISTER_ART_PLUGIN(kmalloc, HELP, kmalloc_init, NULL);
