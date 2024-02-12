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
#include <linux/slab.h>

static ssize_t art_debugfs_string_read(struct file *file, char __user *user_buf,
                                       size_t count, loff_t *ppos) {
  const char *value = file->f_inode->i_private;
  return simple_read_from_buffer(user_buf, count, ppos, value, strlen(value));
}

static const struct file_operations art_debugfs_fops_string = {
    .read = art_debugfs_string_read};

struct dentry *art_debugfs_create_string(const char *name, umode_t mode,
                                         struct dentry *parent,
                                         const char *value) {
  struct dentry *dentry;
  char *buf;

  buf = kstrdup(value, GFP_KERNEL);
  if (!buf) {
    return NULL;
  }

  dentry =
      debugfs_create_file(name, mode, parent, buf, &art_debugfs_fops_string);
  if (!dentry) {
    kfree(buf);
    return NULL;
  }

  return dentry;
}
