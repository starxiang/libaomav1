/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_CONTEXT_TREE_H_
#define AOM_AV1_ENCODER_CONTEXT_TREE_H_

#include "av1/common/blockd.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;
struct AV1Common;
struct ThreadData;

typedef struct {
  tran_low_t *coeff_buf[MAX_MB_PLANE];
  tran_low_t *qcoeff_buf[MAX_MB_PLANE];
  tran_low_t *dqcoeff_buf[MAX_MB_PLANE];
} PC_TREE_SHARED_BUFFERS;

// Structure to hold snapshot of coding context during the mode picking process
typedef struct {
  MB_MODE_INFO mic;
  MB_MODE_INFO_EXT mbmi_ext;
  uint8_t *color_index_map[2];
  uint8_t *blk_skip;

  tran_low_t *coeff[MAX_MB_PLANE];
  tran_low_t *qcoeff[MAX_MB_PLANE];
  tran_low_t *dqcoeff[MAX_MB_PLANE];
  uint16_t *eobs[MAX_MB_PLANE];
  uint8_t *txb_entropy_ctx[MAX_MB_PLANE];

  int num_4x4_blk;
  // For current partition, only if all Y, U, and V transform blocks'
  // coefficients are quantized to 0, skippable is set to 1.
  int skippable;
  int best_mode_index;
  int hybrid_pred_diff;
  int comp_pred_diff;
  int single_pred_diff;

  RD_STATS rd_stats;

  int rd_mode_is_ready;  // Flag to indicate whether rd pick mode decision has
                         // been made.

  // motion vector cache for adaptive motion search control in partition
  // search loop
  MV pred_mv[REF_FRAMES];
} PICK_MODE_CONTEXT;

typedef struct PC_TREE {
  PARTITION_TYPE partitioning;
  BLOCK_SIZE block_size;
  PICK_MODE_CONTEXT *none;
  PICK_MODE_CONTEXT *horizontal[2];
  PICK_MODE_CONTEXT *vertical[2];
#if CONFIG_RECURSIVE_ABPART
  struct PC_TREE *horza_split[2];
  struct PC_TREE *horzb_split[2];
  struct PC_TREE *verta_split[2];
  struct PC_TREE *vertb_split[2];
  PICK_MODE_CONTEXT *horza_rec;
  PICK_MODE_CONTEXT *horzb_rec;
  PICK_MODE_CONTEXT *verta_rec;
  PICK_MODE_CONTEXT *vertb_rec;
#else
  PICK_MODE_CONTEXT *horizontala[3];
  PICK_MODE_CONTEXT *horizontalb[3];
  PICK_MODE_CONTEXT *verticala[3];
  PICK_MODE_CONTEXT *verticalb[3];
#endif  // CONFIG_RECURSIVE_ABPART
#if CONFIG_3WAY_PARTITIONS
  PICK_MODE_CONTEXT *horizontal3[3];
  PICK_MODE_CONTEXT *vertical3[3];
#else
  PICK_MODE_CONTEXT *horizontal4[4];
  PICK_MODE_CONTEXT *vertical4[4];
#endif  // CONFIG_3WAY_PARTITIONS
  struct PC_TREE *split[4];
  int is_last_subblock;
} PC_TREE;

typedef struct SIMPLE_MOTION_DATA_TREE {
  BLOCK_SIZE block_size;
  PARTITION_TYPE partitioning;
  struct SIMPLE_MOTION_DATA_TREE *split[4];

  // Simple motion search_features
  MV mv_ref_fulls[REF_FRAMES];
  unsigned int sms_none_feat[2];
  unsigned int sms_rect_feat[8];
  int sms_none_valid;
  int sms_rect_valid;
} SIMPLE_MOTION_DATA_TREE;

void av1_setup_shared_coeff_buffer(AV1_COMMON *cm,
                                   PC_TREE_SHARED_BUFFERS *shared_bufs);
void av1_free_shared_coeff_buffer(PC_TREE_SHARED_BUFFERS *shared_bufs);

PC_TREE *av1_alloc_pc_tree_node(BLOCK_SIZE bsize, int is_last);
void av1_free_pc_tree_recursive(PC_TREE *tree, int num_planes, int keep_best,
                                int keep_none);

PICK_MODE_CONTEXT *av1_alloc_pmc(const AV1_COMMON *cm, BLOCK_SIZE bsize,
                                 PC_TREE_SHARED_BUFFERS *shared_bufs);
void av1_free_pmc(PICK_MODE_CONTEXT *ctx, int num_planes);

void av1_copy_tree_context(PICK_MODE_CONTEXT *dst_ctx,
                           PICK_MODE_CONTEXT *src_ctx);

void av1_setup_sms_tree(struct AV1Common *cm, struct ThreadData *td);
void av1_free_sms_tree(struct ThreadData *td);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_CONTEXT_TREE_H_
