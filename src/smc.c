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

static struct dentry *smc_dentry;

static int smc_init(struct dentry *parent) {
  smc_dentry = parent;
  return smccc_init(SMCCC_SMC, parent);
}

static void smc_exit(void) { smccc_exit(smc_dentry); }

static char *const HELP =
    "# Execute SMCCC_VERSION with some unused arguments in different numeric "
    "formats (supports up to 8 arguments including SMC ID)\n"
    "$ echo 0x80000000 0xdeadbeef 0777 42 > /d/art/smc/cmd\n"
    "\n"
    "# Result is SMC Version 1.2, unused arguments are returned as is (in "
    "hex)\n"
    "$ cat /d/art/smc/result\n"
    "0x10002 0xdeadbeef 0x1ff 0x2a\n";

REGISTER_ART_PLUGIN(smc, HELP, smc_init, smc_exit);
