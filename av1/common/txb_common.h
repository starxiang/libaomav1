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

#ifndef AV1_COMMON_TXB_COMMON_H_
#define AV1_COMMON_TXB_COMMON_H_

extern const int16_t k_eob_group_start[12];
extern const int16_t k_eob_offset_bits[12];
int16_t get_eob_pos_token(int eob, int16_t *extra);
int av1_get_eob_pos_ctx(TX_TYPE tx_type, int eob_token);

extern const int16_t av1_coeff_band_4x4[16];

extern const int16_t av1_coeff_band_8x8[64];

extern const int16_t av1_coeff_band_16x16[256];

extern const int16_t av1_coeff_band_32x32[1024];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

static INLINE TX_SIZE get_txsize_context(TX_SIZE tx_size) {
  return txsize_sqr_up_map[tx_size];
}

// Note: MAX_TX_PAD_2D is dependent to this offset table.
static const int base_ref_offset[BASE_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -2, 0 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { 0, -1 }, { 0, 1 },
  { 0, 2 },  { 1, -1 },  { 1, 0 },  { 1, 1 },  { 2, 0 }
  /* clang-format on*/
};

static INLINE uint8_t *set_levels(uint8_t *const levels_buf, const int width) {
  return levels_buf + MAX_TX_PAD_TOP * (width + MAX_TX_PAD_HOR);
}

static INLINE int get_paded_idx(const int idx, const int bwl) {
  return idx + MAX_TX_PAD_HOR * (idx >> bwl);
}

static INLINE int get_level_count_mag(int *const mag,
                                      const uint8_t *const levels,
                                      const int stride, const int row,
                                      const int col, const int level,
                                      const int (*nb_offset)[2],
                                      const int nb_num) {
  int count = 0;
  *mag = 0;
  for (int idx = 0; idx < nb_num; ++idx) {
    const int ref_row = row + nb_offset[idx][0];
    const int ref_col = col + nb_offset[idx][1];
    const int pos = ref_row * stride + ref_col;
    count += levels[pos] > level;
    if (nb_offset[idx][0] >= 0 && nb_offset[idx][1] >= 0)
      *mag = AOMMAX(*mag, levels[pos]);
  }
  return count;
}

static INLINE int get_base_ctx_from_count_mag(int row, int col, int count,
                                              int sig_mag) {
  const int ctx = (count + 1) >> 1;
  int ctx_idx = -1;
  if (row == 0 && col == 0) {
    ctx_idx = (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 8);
  } else if (row == 0) {
    ctx_idx = 8 + (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 18);
  } else if (col == 0) {
    ctx_idx = 8 + 10 + (ctx << 1) + sig_mag;
    // TODO(angiebird): turn this on once the optimization is finalized
    // assert(ctx_idx < 28);
  } else {
    ctx_idx = 8 + 10 + 10 + (ctx << 1) + sig_mag;
    assert(ctx_idx < COEFF_BASE_CONTEXTS);
  }
  return ctx_idx;
}

static INLINE int get_base_ctx(const uint8_t *const levels,
                               const int c,  // raster order
                               const int bwl, const int level) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + MAX_TX_PAD_HOR;
  const int level_minus_1 = level - 1;
  int mag;
  const int count =
      get_level_count_mag(&mag, levels, stride, row, col, level_minus_1,
                          base_ref_offset, BASE_CONTEXT_POSITION_NUM);
  const int ctx_idx = get_base_ctx_from_count_mag(row, col, count, mag > level);
  return ctx_idx;
}

#define BR_CONTEXT_POSITION_NUM 8  // Base range coefficient context
// Note: MAX_TX_PAD_2D is dependent to this offset table.
static const int br_ref_offset[BR_CONTEXT_POSITION_NUM][2] = {
  /* clang-format off*/
  { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
  { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 },
  /* clang-format on*/
};

static const int br_level_map[9] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3,
};

static const int coeff_to_br_index[COEFF_BASE_RANGE] = {
  0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
};

static const int br_index_to_coeff[BASE_RANGE_SETS] = {
  0, 2, 6,
};

static const int br_extra_bits[BASE_RANGE_SETS] = {
  1, 2, 3,
};

#define BR_MAG_OFFSET 1

static INLINE int get_br_ctx_from_count_mag(int row, int col, int count,
                                            int mag) {
  int offset = 0;
  if (mag <= BR_MAG_OFFSET)
    offset = 0;
  else if (mag <= 3)
    offset = 1;
  else if (mag <= 5)
    offset = 2;
  else
    offset = 3;

  int ctx = br_level_map[count];
  ctx += offset * BR_TMP_OFFSET;

  // DC: 0 - 1
  if (row == 0 && col == 0) return ctx;

  // Top row: 2 - 4
  if (row == 0) return 2 + ctx;

  // Left column: 5 - 7
  if (col == 0) return 5 + ctx;

  // others: 8 - 11
  return 8 + ctx;
}

static INLINE int get_br_ctx(const uint8_t *const levels,
                             const int c,  // raster order
                             const int bwl) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = (1 << bwl) + MAX_TX_PAD_HOR;
  const int level_minus_1 = NUM_BASE_LEVELS;
  int mag;
  const int count =
      get_level_count_mag(&mag, levels, stride, row, col, level_minus_1,
                          br_ref_offset, BR_CONTEXT_POSITION_NUM);
  const int ctx = get_br_ctx_from_count_mag(row, col, count, mag);
  return ctx;
}

#define SIG_REF_OFFSET_NUM 7

// Note: MAX_TX_PAD_2D is dependent to these offset tables.
static const int sig_ref_offset[SIG_REF_OFFSET_NUM][2] = {
  { 2, 1 }, { 2, 0 }, { 1, 2 }, { 1, 1 }, { 1, 0 }, { 0, 2 }, { 0, 1 },
};

static const int sig_ref_offset_vert[SIG_REF_OFFSET_NUM][2] = {
  { 2, 1 }, { 2, 0 }, { 3, 0 }, { 1, 1 }, { 1, 0 }, { 4, 0 }, { 0, 1 },
};

static const int sig_ref_offset_horiz[SIG_REF_OFFSET_NUM][2] = {
  { 0, 3 }, { 0, 4 }, { 1, 2 }, { 1, 1 }, { 1, 0 }, { 0, 2 }, { 0, 1 },
};

static INLINE int get_nz_count(const uint8_t *const levels, const int bwl,
                               const int row, const int col,
                               const TX_CLASS tx_class) {
  const int stride = (1 << bwl) + MAX_TX_PAD_HOR;
  int count = 0;
  for (int idx = 0; idx < SIG_REF_OFFSET_NUM; ++idx) {
    const int ref_row = row + ((tx_class == TX_CLASS_2D)
                                   ? sig_ref_offset[idx][0]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_offset_vert[idx][0]
                                          : sig_ref_offset_horiz[idx][0]));
    const int ref_col = col + ((tx_class == TX_CLASS_2D)
                                   ? sig_ref_offset[idx][1]
                                   : ((tx_class == TX_CLASS_VERT)
                                          ? sig_ref_offset_vert[idx][1]
                                          : sig_ref_offset_horiz[idx][1]));
    const int nb_pos = ref_row * stride + ref_col;
    count += (levels[nb_pos] != 0);
  }
  return count;
}

static INLINE TX_CLASS get_tx_class(TX_TYPE tx_type) {
  switch (tx_type) {
#if CONFIG_EXT_TX
    case V_DCT:
    case V_ADST:
    case V_FLIPADST: return TX_CLASS_VERT;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST: return TX_CLASS_HORIZ;
#endif
    default: return TX_CLASS_2D;
  }
}

// TODO(angiebird): optimize this function by generate a table that maps from
// count to ctx
static INLINE int get_nz_map_ctx_from_count(int count,
                                            int coeff_idx,  // raster order
                                            int bwl, int height,
                                            TX_TYPE tx_type) {
  (void)tx_type;
  const int row = coeff_idx >> bwl;
  const int col = coeff_idx - (row << bwl);
  const int width = 1 << bwl;

  int ctx = 0;
#if CONFIG_EXT_TX
  int tx_class = get_tx_class(tx_type);
  int offset;
  if (tx_class == TX_CLASS_2D)
    offset = 0;
  else if (tx_class == TX_CLASS_VERT)
    offset = SIG_COEF_CONTEXTS_2D;
  else
    offset = SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D;
#else
  int offset = 0;
#endif

  ctx = (count + 1) >> 1;

  if (tx_class == TX_CLASS_2D) {
    {
      if (row == 0 && col == 0) return offset + 0;

      if (width < height)
        if (row < 2) return offset + 11 + ctx;

      if (width > height)
        if (col < 2) return offset + 16 + ctx;

      if (row + col < 2) return offset + ctx + 1;
      if (row + col < 4) return offset + 5 + ctx + 1;

      return offset + 21 + AOMMIN(ctx, 4);
    }
  } else {
    if (tx_class == TX_CLASS_VERT) {
      if (row == 0) return offset + ctx;
      if (row < 2) return offset + 5 + ctx;
      return offset + 10 + ctx;
    } else {
      if (col == 0) return offset + ctx;
      if (col < 2) return offset + 5 + ctx;
      return offset + 10 + ctx;
    }
  }
}

static INLINE int get_nz_map_ctx(const uint8_t *const levels,
                                 const int scan_idx, const int16_t *const scan,
                                 const int bwl, const int height,
                                 const TX_TYPE tx_type) {
  const int coeff_idx = scan[scan_idx];
  const int row = coeff_idx >> bwl;
  const int col = coeff_idx - (row << bwl);

  int tx_class = get_tx_class(tx_type);
  int count = get_nz_count(levels, bwl, row, col, tx_class);
  return get_nz_map_ctx_from_count(count, coeff_idx, bwl, height, tx_type);
}

static INLINE int get_eob_ctx(const int coeff_idx,  // raster order
                              const TX_SIZE txs_ctx, const TX_TYPE tx_type) {
  int offset = 0;
#if CONFIG_CTX1D
  const TX_CLASS tx_class = get_tx_class(tx_type);
  if (tx_class == TX_CLASS_VERT)
    offset = EOB_COEF_CONTEXTS_2D;
  else if (tx_class == TX_CLASS_HORIZ)
    offset = EOB_COEF_CONTEXTS_2D + EOB_COEF_CONTEXTS_1D;
#else
  (void)tx_type;
#endif

  if (txs_ctx == TX_4X4) return offset + av1_coeff_band_4x4[coeff_idx];
  if (txs_ctx == TX_8X8) return offset + av1_coeff_band_8x8[coeff_idx];
  if (txs_ctx == TX_16X16) return offset + av1_coeff_band_16x16[coeff_idx];
  if (txs_ctx == TX_32X32) return offset + av1_coeff_band_32x32[coeff_idx];

  assert(0);
  return 0;
}

static INLINE void set_dc_sign(int *cul_level, tran_low_t v) {
  if (v < 0)
    *cul_level |= 1 << COEFF_CONTEXT_BITS;
  else if (v > 0)
    *cul_level += 2 << COEFF_CONTEXT_BITS;
}

static INLINE int get_dc_sign_ctx(int dc_sign) {
  int dc_sign_ctx = 0;
  if (dc_sign < 0)
    dc_sign_ctx = 1;
  else if (dc_sign > 0)
    dc_sign_ctx = 2;

  return dc_sign_ctx;
}

static INLINE void get_txb_ctx(BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               int plane, const ENTROPY_CONTEXT *a,
                               const ENTROPY_CONTEXT *l, TXB_CTX *txb_ctx) {
  const int txb_w_unit = tx_size_wide_unit[tx_size];
  const int txb_h_unit = tx_size_high_unit[tx_size];
  int ctx_offset = (plane == 0) ? 0 : 7;

  if (plane_bsize > txsize_to_bsize[tx_size]) ctx_offset += 3;

  int dc_sign = 0;
  for (int k = 0; k < txb_w_unit; ++k) {
    int sign = ((uint8_t)a[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  for (int k = 0; k < txb_h_unit; ++k) {
    int sign = ((uint8_t)l[k]) >> COEFF_CONTEXT_BITS;
    if (sign == 1)
      --dc_sign;
    else if (sign == 2)
      ++dc_sign;
    else if (sign != 0)
      assert(0);
  }

  txb_ctx->dc_sign_ctx = get_dc_sign_ctx(dc_sign);

  if (plane == 0) {
    int top = 0;
    int left = 0;

    for (int k = 0; k < txb_w_unit; ++k) {
      top = AOMMAX(top, ((uint8_t)a[k] & COEFF_CONTEXT_MASK));
    }

    for (int k = 0; k < txb_h_unit; ++k) {
      left = AOMMAX(left, ((uint8_t)l[k] & COEFF_CONTEXT_MASK));
    }

    top = AOMMIN(top, 255);
    left = AOMMIN(left, 255);

    if (plane_bsize == txsize_to_bsize[tx_size])
      txb_ctx->txb_skip_ctx = 0;
    else if (top == 0 && left == 0)
      txb_ctx->txb_skip_ctx = 1;
    else if (top == 0 || left == 0)
      txb_ctx->txb_skip_ctx = 2 + (AOMMAX(top, left) > 3);
    else if (AOMMAX(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 4;
    else if (AOMMIN(top, left) <= 3)
      txb_ctx->txb_skip_ctx = 5;
    else
      txb_ctx->txb_skip_ctx = 6;
  } else {
    int ctx_base = get_entropy_context(tx_size, a, l);
    txb_ctx->txb_skip_ctx = ctx_offset + ctx_base;
  }
}

void av1_init_txb_probs(FRAME_CONTEXT *fc);

void av1_init_lv_map(AV1_COMMON *cm);

#endif  // AV1_COMMON_TXB_COMMON_H_
