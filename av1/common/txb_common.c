/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "aom/aom_integer.h"
#include "av1/common/onyxc_int.h"
#if CONFIG_LV_MAP
#include "av1/common/txb_common.h"
#endif

const int16_t av1_coeff_band_4x4[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                         8, 9, 10, 11, 12, 13, 14, 15 };

const int16_t av1_coeff_band_8x8[64] = {
  0,  1,  2,  2,  3,  3,  4,  4,  5,  6,  2,  2,  3,  3,  4,  4,
  7,  7,  8,  8,  9,  9,  10, 10, 7,  7,  8,  8,  9,  9,  10, 10,
  11, 11, 12, 12, 13, 13, 14, 14, 11, 11, 12, 12, 13, 13, 14, 14,
  15, 15, 16, 16, 17, 17, 18, 18, 15, 15, 16, 16, 17, 17, 18, 18,
};

const int16_t av1_coeff_band_16x16[256] = {
  0,  1,  4,  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  2,  3,  4,
  4,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,
  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  5,  5,  6,  6,  7,  7,  7,  7,  8,
  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12,
  13, 13, 13, 13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13,
  13, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 10, 10,
  10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15,
  15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15,
  16, 16, 16, 16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16,
  16, 17, 17, 17, 17, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17,
  17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18,
  18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18,
  19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 18, 18, 18, 18, 19, 19, 19,
  19, 20, 20, 20, 20, 21, 21, 21, 21,
};

const int16_t av1_coeff_band_32x32[1024] = {
  0,  1,  4,  4,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
  11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 2,  3,  4,  4,  7,  7,
  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12,
  12, 12, 12, 12, 12, 12, 12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10,
  10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12,
  12, 5,  5,  6,  6,  7,  7,  7,  7,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11,
  11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,
  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
  12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10,
  10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
  12, 12, 8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11,
  11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 8,  8,  8,  8,
  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11,
  11, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14,
  14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16,
  16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14,
  15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13,
  13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15,
  15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14,
  14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16,
  16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14,
  14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13,
  13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
  15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13,
  14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16,
  16, 16, 16, 16, 16, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
  14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17,
  17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
  19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17,
  17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20,
  20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18,
  18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20,
  17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19,
  19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17,
  17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20,
  20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18,
  18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20,
  20, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19,
  19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 17,
  17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19,
  20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22,
  22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24,
  24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23,
  23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21,
  21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23,
  23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22,
  22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24,
  24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
  23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21,
  21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23,
  23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22,
  22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24,
  24, 24, 24, 24, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22,
  22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24,
};

void av1_adapt_txb_probs(AV1_COMMON *cm, unsigned int count_sat,
                         unsigned int update_factor) {
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = cm->pre_fc;
  const FRAME_COUNTS *counts = &cm->counts;
  TX_SIZE tx_size;
  int plane, ctx, level;

  // Update probability models for transform block skip flag
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size)
    for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx)
      fc->txb_skip[tx_size][ctx] = mode_mv_merge_probs(
          pre_fc->txb_skip[tx_size][ctx], counts->txb_skip[tx_size][ctx]);

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      fc->dc_sign[plane][ctx] = mode_mv_merge_probs(
          pre_fc->dc_sign[plane][ctx], counts->dc_sign[plane][ctx]);

  // Update probability models for non-zero coefficient map and eob flag.
  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size)
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (level = 0; level < NUM_BASE_LEVELS; ++level)
        for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx)
          fc->coeff_base[tx_size][plane][level][ctx] =
              merge_probs(pre_fc->coeff_base[tx_size][plane][level][ctx],
                          counts->coeff_base[tx_size][plane][level][ctx],
                          count_sat, update_factor);

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
        fc->nz_map[tx_size][plane][ctx] = merge_probs(
            pre_fc->nz_map[tx_size][plane][ctx],
            counts->nz_map[tx_size][plane][ctx], count_sat, update_factor);
      }

      for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
        fc->eob_flag[tx_size][plane][ctx] = merge_probs(
            pre_fc->eob_flag[tx_size][plane][ctx],
            counts->eob_flag[tx_size][plane][ctx], count_sat, update_factor);
      }
    }
  }

  for (tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
    for (plane = 0; plane < PLANE_TYPES; ++plane)
      for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx)
        fc->coeff_lps[tx_size][plane][ctx] = merge_probs(
            pre_fc->coeff_lps[tx_size][plane][ctx],
            counts->coeff_lps[tx_size][plane][ctx], count_sat, update_factor);
  }
}

void av1_init_lv_map(AV1_COMMON *cm) {
  LV_MAP_CTX_TABLE *coeff_ctx_table = &cm->coeff_ctx_table;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      for (int sig_mag = 0; sig_mag < 2; ++sig_mag) {
        for (int count = 0; count < BASE_CONTEXT_POSITION_NUM + 1; ++count) {
          coeff_ctx_table->base_ctx_table[row][col][sig_mag][count] =
              get_base_ctx_from_count_mag(row, col, count, sig_mag);
        }
      }
    }
  }
}
