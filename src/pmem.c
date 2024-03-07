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
#include <linux/highmem.h>
#include <linux/version.h>

static uint64_t addr;
static uint64_t bytes_read_size = 8;

static bool is_ram(uint64_t addr) {
#ifdef CONFIG_ARM64

#if KERNEL_VERSION(5, 14, 0) <= LINUX_VERSION_CODE
  return pfn_is_map_memory(__phys_to_pfn(addr));
#else
  return pfn_valid(__phys_to_pfn(addr));
#endif /* KERNEL_VERSION(5, 14, 0) */

#else
  return false;
#endif /* CONFIG_ARM64 */
}

static void *pmem_kmap(struct page *page) {
#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE
  return kmap_local_page(page);
#else
  return kmap(page);
#endif /* KERNEL_VERSION(5, 11, 0) */
}

static void pmem_kunmap(void *phys_addr) {
#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE
  kunmap_local(phys_addr);
#else
  kunmap(phys_addr);
#endif /* KERNEL_VERSION(5, 11, 0) */
}

static int copy_phys_ram(char *buffer, uint64_t phys_addr, size_t len,
                         bool to_buffer) {
  ssize_t ret = -EINVAL;

  if (phys_addr == 0 || len == 0) {
    return ret;
  }

  ret = len;
  while (len) {
    off_t page_offset;
    size_t bytes_to_copy;
    char *kaddr;
    void *mapped_page;

    mapped_page = pmem_kmap(phys_to_page(phys_addr));
    if (mapped_page == NULL) {
      ret = -EFAULT;
      break;
    }

    page_offset = phys_addr & ~PAGE_MASK;
    kaddr = mapped_page + page_offset;
    bytes_to_copy = min(len, (size_t)PAGE_SIZE - page_offset);

    if (to_buffer) {
      memcpy(buffer, kaddr, bytes_to_copy);
    } else {
      memcpy(kaddr, buffer, bytes_to_copy);
    }

    pmem_kunmap(mapped_page);

    phys_addr += bytes_to_copy;
    buffer += bytes_to_copy;
    len -= bytes_to_copy;
  }

  return ret;
}

static int copy_from_phys_ioremap(char *dst_buffer, uint64_t phys_addr,
                                  size_t len) {
  int ret = -EINVAL;
  void *io = ioremap(phys_addr, len);
  if (io) {
    int i;
    for (i = 0; i < len; i++) {
      dst_buffer[i] = ioread8(io + i);
    }
    if (i == len) {
      ret = len;
    }
    iounmap(io);
  }

  return ret;
}

static int copy_to_phys_ioremap(const char *src_buffer, uint64_t phys_addr,
                                size_t len) {
  int ret = -EINVAL;
  void *io = ioremap(phys_addr, len);
  if (io) {
    int i;
    for (i = 0; i < len; i++) {
      iowrite8(src_buffer[i], io + i);
    }
    if (i == len) {
      ret = len;
    }
    iounmap(io);
  }
  return ret;
}

static int read_phys(char *buffer, uint64_t addr, size_t len) {
  int ret = -EINVAL;

  if (is_ram(addr)) {
    ret = copy_phys_ram(buffer, addr, len, true);
  } else {
    ret = copy_from_phys_ioremap(buffer, addr, len);
  }

  return ret;
}

static int write_phys(const char *buffer, uint64_t addr, size_t len) {
  int ret = -EINVAL;

  if (is_ram(addr)) {
    ret = copy_phys_ram((char *)buffer, addr, len, false);
  } else {
    ret = copy_to_phys_ioremap(buffer, addr, len);
  }

  return ret;
}

static int copy_from_phys_to_user(char __user *buffer, loff_t offset,
                                  size_t len) {
  int ret = -EINVAL;
  uint64_t target_addr = addr + offset;
  char *tmp_buf = kzalloc(len, GFP_KERNEL);

  ret = read_phys(tmp_buf, target_addr, len);

  if (ret < 0) {
    goto err;
  }
  if (copy_to_user(buffer, tmp_buf, ret)) {
    ret = -EFAULT;
    goto err;
  }

err:
  kfree(tmp_buf);

  return ret;
}

static int copy_from_user_to_phys(const char __user *buffer, loff_t offset,
                                  size_t len) {
  int ret = -EINVAL;
  uint64_t target_addr = addr + offset;
  char *tmp_buf = kzalloc(len, GFP_KERNEL);

  if (copy_from_user(tmp_buf, buffer, len)) {
    ret = -EFAULT;
    goto err;
  }

  ret = write_phys(tmp_buf, target_addr, len);

err:
  kfree(tmp_buf);

  return ret;
}

static int pmem_val_write_op(void *data, uint64_t value) {
  int ret = write_phys((char *)&value, addr, sizeof(value));
  if (ret < 0) {
    return ret;
  }

  return 0;
}

static int pmem_val_read_op(void *data, uint64_t *value) {
  int ret = read_phys((char *)value, addr, sizeof(*value));
  if (ret < 0) {
    return ret;
  }

  return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(pmem_val_fops, pmem_val_read_op, pmem_val_write_op,
                         "0x%llx\n");

static ssize_t pmem_bytes_read(struct file *filp, char __user *buffer,
                               size_t len, loff_t *offset) {
  size_t remaining, read_size;
  ssize_t ret;

  if (*offset >= bytes_read_size) {
    // No more data to read
    return 0;
  }

  remaining = bytes_read_size - *offset;
  read_size = min(len, remaining);

  ret = copy_from_phys_to_user(buffer, *offset, read_size);
  if (ret >= 0) {
    *offset += ret;
  }

  return ret;
}

static ssize_t pmem_bytes_write(struct file *filp, const char __user *buffer,
                                size_t len, loff_t *offset) {
  return copy_from_user_to_phys(buffer, *offset, len);
}

static const struct file_operations pmem_bytes_fops = {
    .read = pmem_bytes_read,
    .write = pmem_bytes_write,
};

static char *const HELP =
    "# Write 32-bit value in base 10 to addr:\n"
    "$ echo 0xB62CE0DC > /d/art/pmem/addr\n"
    "$ echo 12345678 > /d/art/pmem/val\n"
    "\n"
    "# Write string to addr:\n"
    "$ echo 0xB62CE0DC > /d/art/pmem/addr\n"
    "$ echo -n 'helloworld' > /d/art/pmem/bytes\n"
    "\n"
    "# Write hex value to addr:\n"
    "$ echo 0xB62CE0DC > /d/art/pmem/addr\n"
    "$ echo -n '56 67 89 ab cd ef' | xxd -r -p | dd of=/d/art/pmem/bytes\n"
    "\n"
    "# Read 5 hex values from addr:\n"
    "$ echo 0x5 > /d/art/pmem/bytes-read-size\n"
    "$ xxd -p /d/art/pmem/bytes\n";

static int pmem_init(struct dentry *parent) {
  debugfs_create_x64("addr", 0666, parent, &addr);
  debugfs_create_x64("bytes-read-size", 0666, parent, &bytes_read_size);
  debugfs_create_file("val", 0666, parent, NULL, &pmem_val_fops);
  debugfs_create_file("bytes", 0666, parent, NULL, &pmem_bytes_fops);
  return 0;
}

REGISTER_ART_PLUGIN(pmem, HELP, pmem_init, NULL);
