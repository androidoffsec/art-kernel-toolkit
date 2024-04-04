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
#include "mount.h"

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/module.h>

static struct dentry *art_debugfs_dir = NULL;

extern struct art_plugin __art_plugins_start[];
extern struct art_plugin __art_plugins_end[];

static int do_art_plugins_init(void) {
  int err = 0;
  struct art_plugin *p;

  for (p = __art_plugins_start; p != __art_plugins_end; p++) {
    struct dentry *debugfs_dir = debugfs_create_dir(p->name, art_debugfs_dir);

    if (p->help_string != NULL) {
      art_debugfs_create_string("help", 0444, debugfs_dir, p->help_string);
    }
    err = p->init(debugfs_dir);
    if (err) {
      return err;
    }
  }

  return 0;
}

static void do_art_plugins_exit(void) {
  struct art_plugin *p;

  for (p = __art_plugins_start; p != __art_plugins_end; p++) {
    if (p->exit) {
      p->exit();
    }
  }
}

static int __init art_driver_init(void) {
  int ret;

  art_debugfs_dir = debugfs_create_dir("art", NULL);
  if (IS_ERR(art_debugfs_dir)) {
    return PTR_ERR(art_debugfs_dir);
  }

  ret = do_art_plugins_init();
  if (ret) {
    return ret;
  }

  // This must be called after `do_art_plugins_init` as this relies on the
  // kallsyms plugin
  ret = mount_init();
  if (ret) {
    return ret;
  }

  // Note that we intentionally don't return the error code here. Mounting
  // debugfs automatically is non-critical functionality, and in case this
  // function breaks with a kernel upgrade, we still want the rest of the module
  // to be usable as long as the user can manually mount debugfs
  ret = mount("none", "/sys/kernel/debug", "debugfs");
  if (ret < 0) {
    if (ret == -EBUSY) {
      pr_warn("debugfs is already mounted at /sys/kernel/debug\n");
    } else {
      pr_err("failed to automatically mount debugfs: %d\n", ret);
    }
  }
  return 0;
}

static void __exit art_driver_exit(void) {
  do_art_plugins_exit();

  debugfs_remove_recursive(art_debugfs_dir);
}

MODULE_DESCRIPTION("Android Red Team Kernel Toolkit");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
module_init(art_driver_init);
module_exit(art_driver_exit);
