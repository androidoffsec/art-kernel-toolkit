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

#ifndef ART_H
#define ART_H

#include <linux/fs.h>

typedef int (*art_plugin_init_func)(struct dentry *);
typedef void (*art_plugin_exit_func)(void);

struct art_plugin {
  char *name;
  char *help_string;
  art_plugin_init_func init;
  art_plugin_exit_func exit;
};

#define REGISTER_ART_PLUGIN(_name, _help, _init, _exit)                \
  struct art_plugin __art_plugin_##_name __section(".art.plugins") = { \
      .name = #_name, .help_string = _help, .init = _init, .exit = _exit}

#endif /* ART_H */
