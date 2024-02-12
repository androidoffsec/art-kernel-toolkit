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

#include <asm-generic/errno-base.h>
#include <linux/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/string.h>

#define SMCCC_BUF_SIZE 128
#define MAX_SMCCC_REGS 8

struct smccc_private_data {
  spinlock_t lock;
  enum smccc_type type;
  char cmd_buf[SMCCC_BUF_SIZE];
  char result_buf[SMCCC_BUF_SIZE];
};

#define STRSCPY_TO_SMCCC(smccc_data_obj, smccc_buf, src_buf, count) \
  strscpy_with_lock(smccc_data_obj->smccc_buf, src_buf, count,      \
                    &smccc_data_obj->lock)
#define STRSCPY_FROM_SMCCC(smccc_data_obj, smccc_buf, dst_buf, count) \
  strscpy_with_lock(dst_buf, smccc_data_obj->smccc_buf, count,        \
                    &smccc_data_obj->lock)

#define STRSCPY_TO_SMCCC_CMD(smccc_data_obj, src_buf, count) \
  STRSCPY_TO_SMCCC(smccc_data_obj, cmd_buf, src_buf, count)
#define STRSCPY_TO_SMCCC_RESULT(smccc_data_obj, src_buf, count) \
  STRSCPY_TO_SMCCC(smccc_data_obj, result_buf, src_buf, count)
#define STRSCPY_FROM_SMCCC_CMD(smccc_data_obj, dst_buf, count) \
  STRSCPY_FROM_SMCCC(smccc_data_obj, cmd_buf, dst_buf, count)
#define STRSCPY_FROM_SMCCC_RESULT(smccc_data_obj, dst_buf, count) \
  STRSCPY_FROM_SMCCC(smccc_data_obj, result_buf, dst_buf, count)

static ssize_t strscpy_with_lock(char *dst, char *src, size_t count,
                                 spinlock_t *lock) {
  unsigned long flags;
  ssize_t res;

  spin_lock_irqsave(lock, flags);
  res = strscpy(dst, src, count);
  spin_unlock_irqrestore(lock, flags);

  return res;
}

static int parse_smccc_args(char *cmd_buf,
                            uint64_t smccc_regs[MAX_SMCCC_REGS]) {
  char *arg;
  size_t num_args = 0;
  int ret = -EINVAL;
  char *buf = kstrdup(cmd_buf, GFP_KERNEL);

  while ((arg = strsep(&buf, " ")) != NULL) {
    if (num_args >= MAX_SMCCC_REGS) {
      pr_err("number of SMCCC arguments exceeds maximum of %d\n",
             MAX_SMCCC_REGS);
      ret = -EINVAL;
      goto out;
    }

    ret = kstrtoull(arg, 0, &smccc_regs[num_args]);
    if (ret) {
      goto out;
    }

    num_args++;
  }

  ret = num_args;

out:
  kfree(buf);
  return ret;
}

static int smccc_cmd_execute(struct smccc_private_data *smccc_data) {
  int res;
  uint64_t smccc_args[MAX_SMCCC_REGS] = {0};
  char cmd_buf[SMCCC_BUF_SIZE];
  char tmp_buf[SMCCC_BUF_SIZE];
  struct arm_smccc_res smccc_res = {0, 0, 0, 0};

  res = STRSCPY_FROM_SMCCC_CMD(smccc_data, cmd_buf, sizeof(cmd_buf));
  if (res < 0) {
    return res;
  }

  res = parse_smccc_args(cmd_buf, smccc_args);
  if (res < 0) {
    pr_err("failed to parse SMCCC command buffer\n");
    return res;
  }

  switch (smccc_data->type) {
    case SMCCC_HVC:
      arm_smccc_1_1_hvc(smccc_args[0], smccc_args[1], smccc_args[2],
                        smccc_args[3], smccc_args[4], smccc_args[5],
                        smccc_args[6], smccc_args[7], &smccc_res);
      break;
    case SMCCC_SMC:
      arm_smccc_1_1_smc(smccc_args[0], smccc_args[1], smccc_args[2],
                        smccc_args[3], smccc_args[4], smccc_args[5],
                        smccc_args[6], smccc_args[7], &smccc_res);
      break;
    default:
      pr_err("invalid SMCCC type: %d\n", smccc_data->type);
      BUG();
  }

  snprintf(tmp_buf, SMCCC_BUF_SIZE - 1, "0x%lx 0x%lx 0x%lx 0x%lx\n",
           smccc_res.a0, smccc_res.a1, smccc_res.a2, smccc_res.a3);

  res = STRSCPY_TO_SMCCC_RESULT(smccc_data, tmp_buf, sizeof(tmp_buf));
  if (res < 0) {
    return res;
  }

  pr_notice("result: %s\n", tmp_buf);
  return 0;
}

static ssize_t smccc_cmd_write(struct file *fp, const char __user *user_buffer,
                               size_t count, loff_t *position) {
  int res;
  int num_written;
  char tmp_buf[SMCCC_BUF_SIZE];

  struct smccc_private_data *smccc_data =
      (struct smccc_private_data *)file_inode(fp)->i_private;

  num_written = simple_write_to_buffer(tmp_buf, SMCCC_BUF_SIZE - 1, position,
                                       user_buffer, count);

  if (num_written < 0) {
    return num_written;
  }

  tmp_buf[num_written] = '\0';
  STRSCPY_TO_SMCCC_CMD(smccc_data, tmp_buf, sizeof(tmp_buf));
  res = smccc_cmd_execute(smccc_data);
  if (res < 0) {
    return res;
  }

  return num_written;
}

static ssize_t smccc_cmd_read(struct file *fp, char __user *user_buffer,
                              size_t count, loff_t *position) {
  char tmp_buf[SMCCC_BUF_SIZE];
  size_t buf_len;

  struct smccc_private_data *smccc_data =
      (struct smccc_private_data *)file_inode(fp)->i_private;

  buf_len = STRSCPY_FROM_SMCCC_CMD(smccc_data, tmp_buf, sizeof(tmp_buf));
  if (buf_len < 0) {
    return buf_len;
  }

  return simple_read_from_buffer(user_buffer, count, position, tmp_buf,
                                 buf_len);
}

static const struct file_operations smccc_cmd_fops = {
    .read = smccc_cmd_read,
    .write = smccc_cmd_write,
};

static ssize_t smccc_result_read(struct file *fp, char __user *user_buffer,
                                 size_t count, loff_t *position) {
  char tmp_buf[SMCCC_BUF_SIZE];
  size_t buf_len;

  struct smccc_private_data *smccc_data =
      (struct smccc_private_data *)file_inode(fp)->i_private;

  buf_len = STRSCPY_FROM_SMCCC_RESULT(smccc_data, tmp_buf, sizeof(tmp_buf));
  if (buf_len < 0) {
    return buf_len;
  }

  return simple_read_from_buffer(user_buffer, count, position, tmp_buf,
                                 buf_len);
}

static const struct file_operations smccc_result_fops = {
    .read = smccc_result_read,
};

int smccc_init(enum smccc_type type, struct dentry *parent) {
  struct smccc_private_data *smccc_data =
      kzalloc(sizeof(struct smccc_private_data), GFP_KERNEL);

  if (smccc_data == NULL) {
    return -ENOMEM;
  }

  spin_lock_init(&smccc_data->lock);
  smccc_data->type = type;

  debugfs_create_file("cmd", 0666, parent, (void *)smccc_data, &smccc_cmd_fops);
  debugfs_create_file("result", 0444, parent, (void *)smccc_data,
                      &smccc_result_fops);

  parent->d_inode->i_private = smccc_data;

  return 0;
}

void smccc_exit(struct dentry *parent) {
  struct smccc_private_data *smccc_data = parent->d_inode->i_private;
  kfree(smccc_data);
}
