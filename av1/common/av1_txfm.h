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

#ifndef AV1_TXFM_H_
#define AV1_TXFM_H_

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_config.h"

#include "av1/common/enums.h"
#include "av1/common/blockd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(DO_RANGE_CHECK_CLAMP)
#define DO_RANGE_CHECK_CLAMP 0
#endif

extern const int32_t av1_cospi_arr_data[7][64];
extern const int32_t av1_sinpi_arr_data[7][5];

#define MAX_TXFM_STAGE_NUM 12

static const int cos_bit_min = 10;
static const int cos_bit_max = 16;

#define NewSqrt2Bits ((int32_t)12)
// 2^12 * sqrt(2)
static const int32_t NewSqrt2 = 5793;
// 2^12 / sqrt(2)
static const int32_t NewInvSqrt2 = 2896;

static INLINE const int32_t *cospi_arr(int n) {
  return av1_cospi_arr_data[n - cos_bit_min];
}

static INLINE const int32_t *sinpi_arr(int n) {
  return av1_sinpi_arr_data[n - cos_bit_min];
}

static INLINE int32_t range_check_value(int32_t value, int8_t bit) {
#if CONFIG_COEFFICIENT_RANGE_CHECKING
  const int64_t max_value = (1LL << (bit - 1)) - 1;
  const int64_t min_value = -(1LL << (bit - 1));
  if (value < min_value || value > max_value) {
    fprintf(stderr, "coeff out of bit range, value: %d bit %d\n", value, bit);
    assert(0);
  }
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING
#if DO_RANGE_CHECK_CLAMP
  bit = AOMMIN(bit, 31);
  return clamp(value, -(1 << (bit - 1)), (1 << (bit - 1)) - 1);
#endif  // DO_RANGE_CHECK_CLAMP
  (void)bit;
  return value;
}

static INLINE int32_t round_shift(int64_t value, int bit) {
  assert(bit >= 1);
  return (int32_t)((value + (1ll << (bit - 1))) >> bit);
}

static INLINE int32_t half_btf(int32_t w0, int32_t in0, int32_t w1, int32_t in1,
                               int bit) {
  int64_t result_64 = (int64_t)(w0 * in0) + (int64_t)(w1 * in1);
#if CONFIG_COEFFICIENT_RANGE_CHECKING
  assert(result_64 >= INT32_MIN && result_64 <= INT32_MAX);
#endif
  return round_shift(result_64, bit);
}

static INLINE uint16_t highbd_clip_pixel_add(uint16_t dest, tran_high_t trans,
                                             int bd) {
  return clip_pixel_highbd(dest + (int)trans, bd);
}

typedef void (*TxfmFunc)(const int32_t *input, int32_t *output, int8_t cos_bit,
                         const int8_t *stage_range);

typedef void (*FwdTxfm2dFunc)(const int16_t *input, int32_t *output, int stride,
                              TX_TYPE tx_type, int bd);

typedef enum TXFM_TYPE {
  TXFM_TYPE_DCT4,
  TXFM_TYPE_DCT8,
  TXFM_TYPE_DCT16,
  TXFM_TYPE_DCT32,
  TXFM_TYPE_DCT64,
  TXFM_TYPE_ADST4,
  TXFM_TYPE_ADST8,
  TXFM_TYPE_ADST16,
  TXFM_TYPE_IDENTITY4,
  TXFM_TYPE_IDENTITY8,
  TXFM_TYPE_IDENTITY16,
  TXFM_TYPE_IDENTITY32,
#if CONFIG_GFT_LEARNED
  TXFM_TYPE_GFT4,
  TXFM_TYPE_GFT8,
#endif
  TXFM_TYPES,
  TXFM_TYPE_INVALID,
} TXFM_TYPE;

typedef struct TXFM_2D_FLIP_CFG {
  TX_SIZE tx_size;
  int ud_flip;  // flip upside down
  int lr_flip;  // flip left to right
  const int8_t *shift;
  int8_t cos_bit_col;
  int8_t cos_bit_row;
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  TXFM_TYPE txfm_type_col;
  TXFM_TYPE txfm_type_row;
  int stage_num_col;
  int stage_num_row;
} TXFM_2D_FLIP_CFG;

static INLINE void get_flip_cfg(TX_TYPE tx_type, int *ud_flip, int *lr_flip) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      *ud_flip = 0;
      *lr_flip = 0;
      break;
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
#if CONFIG_GFT_LEARNED
    case GFT1_DCT:
    case DCT_GFT1:
    case GFT1_GFT1:
#endif
      *ud_flip = 0;
      *lr_flip = 0;
      break;
    case FLIPADST_DCT:
    case FLIPADST_ADST:
    case V_FLIPADST:
#if CONFIG_GFT_LEARNED
    case GFT2_DCT:
    case GFT2_GFT1:
#endif
      *ud_flip = 1;
      *lr_flip = 0;
      break;
    case DCT_FLIPADST:
    case ADST_FLIPADST:
    case H_FLIPADST:
#if CONFIG_GFT_LEARNED
    case DCT_GFT2:
    case GFT1_GFT2:
#endif
      *ud_flip = 0;
      *lr_flip = 1;
      break;
    case FLIPADST_FLIPADST:
#if CONFIG_GFT_LEARNED
    case GFT2_GFT2:
#endif
      *ud_flip = 1;
      *lr_flip = 1;
      break;
    default:
      *ud_flip = 0;
      *lr_flip = 0;
      assert(0);
  }
}

static INLINE void set_flip_cfg(TX_TYPE tx_type, TXFM_2D_FLIP_CFG *cfg) {
  get_flip_cfg(tx_type, &cfg->ud_flip, &cfg->lr_flip);
}

// Utility function that returns the log of the ratio of the col and row
// sizes.
static INLINE int get_rect_tx_log_ratio(int col, int row) {
  if (col == row) return 0;
  if (col > row) {
    if (col == row * 2) return 1;
    if (col == row * 4) return 2;
    assert(0 && "Unsupported transform size");
  } else {
    if (row == col * 2) return -1;
    if (row == col * 4) return -2;
    assert(0 && "Unsupported transform size");
  }
  return 0;  // Invalid
}

void av1_gen_fwd_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, int bd);

void av1_gen_inv_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, TX_SIZE tx_size,
                             int bd);

void av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg);
void av1_get_inv_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg);
extern const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D];
extern const int8_t av1_txfm_stage_num_list[TXFM_TYPES];
static INLINE int get_txw_idx(TX_SIZE tx_size) {
  return tx_size_wide_log2[tx_size] - tx_size_wide_log2[0];
}
static INLINE int get_txh_idx(TX_SIZE tx_size) {
  return tx_size_high_log2[tx_size] - tx_size_high_log2[0];
}
#define MAX_TXWH_IDX 5

#if CONFIG_GFT_LEARNED
#define GFT_LEARNED_DEBUG 0
#define USE_KLT 0
#define USE_GFT 1
#define USE_LGT 0
#endif

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // AV1_TXFM_H_
