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

#include "mount.h"

#include "kallsyms.h"

#include <linux/mount.h>
#include <linux/namei.h>

typedef int (*path_mount_t)(const char *dev_name, struct path *path,
                            const char *type_page, unsigned long flags,
                            void *data_page);

static path_mount_t __art_path_mount;

static __nocfi unsigned long art_path_mount(const char *dev_name,
                                            struct path *path,
                                            const char *type_page,
                                            unsigned long flags,
                                            void *data_page) {
  BUG_ON(!__art_path_mount);
  return __art_path_mount(dev_name, path, type_page, flags, data_page);
}

int mount(const char *dev_name, const char *pathname, const char *type) {
  int ret;
  struct path path;

  ret = kern_path(pathname, 0, &path);
  if (ret) {
    return ret;
  }

  ret = art_path_mount(dev_name, &path, type, 0, NULL);
  if (ret) {
    goto out;
  }

  pr_info("successfully mounted %s\n", pathname);

out:
  path_put(&path);
  return ret;
}

int mount_init(void) {
  // There doesn't seem to be any exported API for kernel modules to mount
  // filesystems, so we have to find the unexported `path_mount` method to do
  // this
  __art_path_mount = (path_mount_t)art_kallsyms_lookup_name("path_mount");
  if (!__art_path_mount) {
    pr_err("could not resolve path_mount function address\n");
    return -1;
  }

  return 0;
}
