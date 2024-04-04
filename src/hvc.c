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
#include "smccc.h"

static struct dentry *hvc_dentry;

static int hvc_init(struct dentry *parent) {
  hvc_dentry = parent;
  return smccc_init(SMCCC_HVC, parent);
}

static void hvc_exit(void) { smccc_exit(hvc_dentry); }

REGISTER_ART_PLUGIN(hvc, NULL, hvc_init, hvc_exit);
