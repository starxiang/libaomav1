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

#include "av1/common/av1_common_int.h"
#include "av1/common/cfl.h"
#include "av1/common/common_data.h"

#include "config/av1_rtcd.h"

void cfl_init(CFL_CTX *cfl, const SequenceHeader *seq_params) {
  assert(block_size_wide[CFL_MAX_BLOCK_SIZE] == CFL_BUF_LINE);
  assert(block_size_high[CFL_MAX_BLOCK_SIZE] == CFL_BUF_LINE);

  memset(&cfl->recon_buf_q3, 0, sizeof(cfl->recon_buf_q3));
  memset(&cfl->ac_buf_q3, 0, sizeof(cfl->ac_buf_q3));
  cfl->subsampling_x = seq_params->subsampling_x;
  cfl->subsampling_y = seq_params->subsampling_y;
  cfl->are_parameters_computed = 0;
  cfl->store_y = 0;
  // The DC_PRED cache is disabled by default and is only enabled in
  // cfl_rd_pick_alpha
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
  cfl->alpha_is_cached[CFL_PRED_U] = 0;
  cfl->alpha_is_cached[CFL_PRED_V] = 0;
}

void cfl_store_dc_pred(MACROBLOCKD *const xd, const uint8_t *input,
                       CFL_PRED_TYPE pred_plane, int width) {
  assert(pred_plane < CFL_PRED_PLANES);
  assert(width <= CFL_BUF_LINE);

  if (is_cur_buf_hbd(xd)) {
    uint16_t *const input_16 = CONVERT_TO_SHORTPTR(input);
    memcpy(xd->cfl.dc_pred_cache[pred_plane], input_16, width << 1);
    return;
  }

  memcpy(xd->cfl.dc_pred_cache[pred_plane], input, width);
}

static void cfl_load_dc_pred_lbd(const int16_t *dc_pred_cache, uint8_t *dst,
                                 int dst_stride, int width, int height) {
  for (int j = 0; j < height; j++) {
    memcpy(dst, dc_pred_cache, width);
    dst += dst_stride;
  }
}

static void cfl_load_dc_pred_hbd(const int16_t *dc_pred_cache, uint16_t *dst,
                                 int dst_stride, int width, int height) {
  const size_t num_bytes = width << 1;
  for (int j = 0; j < height; j++) {
    memcpy(dst, dc_pred_cache, num_bytes);
    dst += dst_stride;
  }
}
void cfl_load_dc_pred(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                      TX_SIZE tx_size, CFL_PRED_TYPE pred_plane) {
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  assert(pred_plane < CFL_PRED_PLANES);
  assert(width <= CFL_BUF_LINE);
  assert(height <= CFL_BUF_LINE);
  if (is_cur_buf_hbd(xd)) {
    uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst);
    cfl_load_dc_pred_hbd(xd->cfl.dc_pred_cache[pred_plane], dst_16, dst_stride,
                         width, height);
    return;
  }
  cfl_load_dc_pred_lbd(xd->cfl.dc_pred_cache[pred_plane], dst, dst_stride,
                       width, height);
}

// Due to frame boundary issues, it is possible that the total area covered by
// chroma exceeds that of luma. When this happens, we fill the missing pixels by
// repeating the last columns and/or rows.
static INLINE void cfl_pad(CFL_CTX *cfl, int width, int height) {
  const int diff_width = width - cfl->buf_width;
  const int diff_height = height - cfl->buf_height;

  if (diff_width > 0) {
    const int min_height = height - diff_height;
    uint16_t *recon_buf_q3 = cfl->recon_buf_q3 + (width - diff_width);
    for (int j = 0; j < min_height; j++) {
      const uint16_t last_pixel = recon_buf_q3[-1];
      assert(recon_buf_q3 + diff_width <= cfl->recon_buf_q3 + CFL_BUF_SQUARE);
      for (int i = 0; i < diff_width; i++) {
        recon_buf_q3[i] = last_pixel;
      }
      recon_buf_q3 += CFL_BUF_LINE;
    }
    cfl->buf_width = width;
  }
  if (diff_height > 0) {
    uint16_t *recon_buf_q3 =
        cfl->recon_buf_q3 + ((height - diff_height) * CFL_BUF_LINE);
    for (int j = 0; j < diff_height; j++) {
      const uint16_t *last_row_q3 = recon_buf_q3 - CFL_BUF_LINE;
      assert(recon_buf_q3 + width <= cfl->recon_buf_q3 + CFL_BUF_SQUARE);
      for (int i = 0; i < width; i++) {
        recon_buf_q3[i] = last_row_q3[i];
      }
      recon_buf_q3 += CFL_BUF_LINE;
    }
    cfl->buf_height = height;
  }
}

static void subtract_average_c(const uint16_t *src, int16_t *dst, int width,
                               int height, int round_offset, int num_pel_log2) {
  int sum = round_offset;
  const uint16_t *recon = src;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      sum += recon[i];
    }
    recon += CFL_BUF_LINE;
  }
  const int avg = sum >> num_pel_log2;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = src[i] - avg;
    }
    src += CFL_BUF_LINE;
    dst += CFL_BUF_LINE;
  }
}

CFL_SUB_AVG_FN(c)

static INLINE int cfl_idx_to_alpha(uint8_t alpha_idx, int8_t joint_sign,
                                   CFL_PRED_TYPE pred_type) {
  const int alpha_sign = (pred_type == CFL_PRED_U) ? CFL_SIGN_U(joint_sign)
                                                   : CFL_SIGN_V(joint_sign);
  if (alpha_sign == CFL_SIGN_ZERO) return 0;
  const int abs_alpha_q3 =
      (pred_type == CFL_PRED_U) ? CFL_IDX_U(alpha_idx) : CFL_IDX_V(alpha_idx);
  return (alpha_sign == CFL_SIGN_POS) ? abs_alpha_q3 + 1 : -abs_alpha_q3 - 1;
}

static INLINE void cfl_predict_lbd_c(const int16_t *ac_buf_q3, uint8_t *dst,
                                     int dst_stride, int alpha_q3, int width,
                                     int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = clip_pixel(get_scaled_luma_q0(alpha_q3, ac_buf_q3[i]) + dst[i]);
    }
    dst += dst_stride;
    ac_buf_q3 += CFL_BUF_LINE;
  }
}

CFL_PREDICT_FN(c, lbd)

void cfl_predict_hbd_c(const int16_t *ac_buf_q3, uint16_t *dst, int dst_stride,
                       int alpha_q3, int bit_depth, int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = clip_pixel_highbd(
          get_scaled_luma_q0(alpha_q3, ac_buf_q3[i]) + dst[i], bit_depth);
    }
    dst += dst_stride;
    ac_buf_q3 += CFL_BUF_LINE;
  }
}

CFL_PREDICT_FN(c, hbd)

static void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size) {
  CFL_CTX *const cfl = &xd->cfl;
  // Do not call cfl_compute_parameters multiple time on the same values.
  assert(cfl->are_parameters_computed == 0);

  cfl_pad(cfl, tx_size_wide[tx_size], tx_size_high[tx_size]);
  cfl_get_subtract_average_fn(tx_size)(cfl->recon_buf_q3, cfl->ac_buf_q3);
  cfl->are_parameters_computed = 1;
}

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       TX_SIZE tx_size, int plane) {
  CFL_CTX *const cfl = &xd->cfl;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(is_cfl_allowed(xd));

  if (!cfl->are_parameters_computed) cfl_compute_parameters(xd, tx_size);

  const int alpha_q3 =
      cfl_idx_to_alpha(mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, plane - 1);
  assert((tx_size_high[tx_size] - 1) * CFL_BUF_LINE + tx_size_wide[tx_size] <=
         CFL_BUF_SQUARE);
  if (is_cur_buf_hbd(xd)) {
    uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst);
    cfl_get_predict_hbd_fn(tx_size)(cfl->ac_buf_q3, dst_16, dst_stride,
                                    alpha_q3, xd->bd);
    return;
  }
  cfl_get_predict_lbd_fn(tx_size)(cfl->ac_buf_q3, dst, dst_stride, alpha_q3);
}

static void cfl_luma_subsampling_420_lbd_c(const uint8_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  for (int j = 0; j < height; j += 2) {
    for (int i = 0; i < width; i += 2) {
      const int bot = i + input_stride;
      output_q3[i >> 1] =
          (input[i] + input[i + 1] + input[bot] + input[bot + 1]) << 1;
    }
    input += input_stride << 1;
    output_q3 += CFL_BUF_LINE;
  }
}

static void cfl_luma_subsampling_422_lbd_c(const uint8_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i += 2) {
      output_q3[i >> 1] = (input[i] + input[i + 1]) << 2;
    }
    input += input_stride;
    output_q3 += CFL_BUF_LINE;
  }
}

static void cfl_luma_subsampling_444_lbd_c(const uint8_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += CFL_BUF_LINE;
  }
}

static void cfl_luma_subsampling_420_hbd_c(const uint16_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  for (int j = 0; j < height; j += 2) {
    for (int i = 0; i < width; i += 2) {
      const int bot = i + input_stride;
      output_q3[i >> 1] =
          (input[i] + input[i + 1] + input[bot] + input[bot + 1]) << 1;
    }
    input += input_stride << 1;
    output_q3 += CFL_BUF_LINE;
  }
}

static void cfl_luma_subsampling_422_hbd_c(const uint16_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i += 2) {
      output_q3[i >> 1] = (input[i] + input[i + 1]) << 2;
    }
    input += input_stride;
    output_q3 += CFL_BUF_LINE;
  }
}

static void cfl_luma_subsampling_444_hbd_c(const uint16_t *input,
                                           int input_stride,
                                           uint16_t *output_q3, int width,
                                           int height) {
  assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += CFL_BUF_LINE;
  }
}

CFL_GET_SUBSAMPLE_FUNCTION(c)

static INLINE cfl_subsample_hbd_fn cfl_subsampling_hbd(TX_SIZE tx_size,
                                                       int sub_x, int sub_y) {
  if (sub_x == 1) {
    if (sub_y == 1) {
      return cfl_get_luma_subsampling_420_hbd(tx_size);
    }
    return cfl_get_luma_subsampling_422_hbd(tx_size);
  }
  return cfl_get_luma_subsampling_444_hbd(tx_size);
}

static INLINE cfl_subsample_lbd_fn cfl_subsampling_lbd(TX_SIZE tx_size,
                                                       int sub_x, int sub_y) {
  if (sub_x == 1) {
    if (sub_y == 1) {
      return cfl_get_luma_subsampling_420_lbd(tx_size);
    }
    return cfl_get_luma_subsampling_422_lbd(tx_size);
  }
  return cfl_get_luma_subsampling_444_lbd(tx_size);
}

static void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride,
                      int row, int col, TX_SIZE tx_size, int use_hbd) {
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const int tx_off_log2 = MI_SIZE_LOG2;
  const int sub_x = cfl->subsampling_x;
  const int sub_y = cfl->subsampling_y;
  const int store_row = row << (tx_off_log2 - sub_y);
  const int store_col = col << (tx_off_log2 - sub_x);
  const int store_height = height >> sub_y;
  const int store_width = width >> sub_x;

  // Invalidate current parameters
  cfl->are_parameters_computed = 0;

  // Store the surface of the pixel buffer that was written to, this way we
  // can manage chroma overrun (e.g. when the chroma surfaces goes beyond the
  // frame boundary)
  if (col == 0 && row == 0) {
    cfl->buf_width = store_width;
    cfl->buf_height = store_height;
  } else {
    cfl->buf_width = OD_MAXI(store_col + store_width, cfl->buf_width);
    cfl->buf_height = OD_MAXI(store_row + store_height, cfl->buf_height);
  }

  // Check that we will remain inside the pixel buffer.
  assert(store_row + store_height <= CFL_BUF_LINE);
  assert(store_col + store_width <= CFL_BUF_LINE);

  // Store the input into the CfL pixel buffer
  uint16_t *recon_buf_q3 =
      cfl->recon_buf_q3 + (store_row * CFL_BUF_LINE + store_col);
  if (use_hbd) {
    cfl_subsampling_hbd(tx_size, sub_x, sub_y)(CONVERT_TO_SHORTPTR(input),
                                               input_stride, recon_buf_q3);
  } else {
    cfl_subsampling_lbd(tx_size, sub_x, sub_y)(input, input_stride,
                                               recon_buf_q3);
  }
}

// Adjust the row and column of blocks smaller than 8X8, as chroma-referenced
// and non-chroma-referenced blocks are stored together in the CfL buffer.
static INLINE void sub8x8_adjust_offset(const CFL_CTX *cfl, int mi_row,
                                        int mi_col, int *row_out,
                                        int *col_out) {
  // Increment row index for bottom: 8x4, 16x4 or both bottom 4x4s.
  if ((mi_row & 0x01) && cfl->subsampling_y) {
    assert(*row_out == 0);
    (*row_out)++;
  }

  // Increment col index for right: 4x8, 4x16 or both right 4x4s.
  if ((mi_col & 0x01) && cfl->subsampling_x) {
    assert(*col_out == 0);
    (*col_out)++;
  }
}

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize) {
  CFL_CTX *const cfl = &xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  uint8_t *dst = &pd->dst.buf[(row * pd->dst.stride + col) << MI_SIZE_LOG2];

  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    // Only dimensions of size 4 can have an odd offset.
    assert(!((col & 1) && tx_size_wide[tx_size] != 4));
    assert(!((row & 1) && tx_size_high[tx_size] != 4));
    sub8x8_adjust_offset(cfl, xd->mi_row, xd->mi_col, &row, &col);
  }
  cfl_store(cfl, dst, pd->dst.stride, row, col, tx_size, is_cur_buf_hbd(xd));
}

static INLINE int max_intra_block_width(const MACROBLOCKD *xd,
                                        BLOCK_SIZE plane_bsize, int plane,
                                        TX_SIZE tx_size) {
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane)
                              << MI_SIZE_LOG2;
  return ALIGN_POWER_OF_TWO(max_blocks_wide, tx_size_wide_log2[tx_size]);
}

static INLINE int max_intra_block_height(const MACROBLOCKD *xd,
                                         BLOCK_SIZE plane_bsize, int plane,
                                         TX_SIZE tx_size) {
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane)
                              << MI_SIZE_LOG2;
  return ALIGN_POWER_OF_TWO(max_blocks_high, tx_size_high_log2[tx_size]);
}

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size) {
  CFL_CTX *const cfl = &xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  int row = 0;
  int col = 0;

  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    sub8x8_adjust_offset(cfl, xd->mi_row, xd->mi_col, &row, &col);
  }
  const int width = max_intra_block_width(xd, bsize, AOM_PLANE_Y, tx_size);
  const int height = max_intra_block_height(xd, bsize, AOM_PLANE_Y, tx_size);
  tx_size = get_tx_size(width, height);
  cfl_store(cfl, pd->dst.buf, pd->dst.stride, row, col, tx_size,
            is_cur_buf_hbd(xd));
}

static INLINE void cfl_ns_predict_lbd_c(const int16_t *ac_buf_q3, uint8_t *dst,
                                     int dst_stride, int alpha_q12, int width,
                                     int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int scaled_luma_q15 = alpha_q12 * ac_buf_q3[i];
      int scaled_luma_q0 = ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q15, 15);
      dst[i] = clip_pixel(scaled_luma_q0 + dst[i]);
    }
    dst += dst_stride;
    ac_buf_q3 += CFL_BUF_LINE;
  }
}

static INLINE void cfl_ns_predict_hbd_c(const int16_t *ac_buf_q3, uint16_t *dst,
                                     int dst_stride, int alpha_q12, int width,
                                     int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int scaled_luma_q15 = alpha_q12 * ac_buf_q3[i];
      int scaled_luma_q0 = ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q15, 15);
      dst[i] = clip_pixel(scaled_luma_q0 + dst[i]);
    }
    dst += dst_stride;
    ac_buf_q3 += CFL_BUF_LINE;
  }
}

void cfl_ns_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       TX_SIZE tx_size, int plane) {
  CFL_CTX *const cfl = &xd->cfl;
  assert(is_cfl_allowed(xd));

  if (!cfl->are_parameters_computed) cfl_compute_parameters(xd, tx_size);

  const CFL_PRED_TYPE pred_plane = get_cfl_pred_type(plane);
  const int alpha_q12 = cfl->alpha_q12[pred_plane];
  assert((tx_size_high[tx_size] - 1) * CFL_BUF_LINE + tx_size_wide[tx_size] <=
         CFL_BUF_SQUARE);
  if (is_cur_buf_hbd(xd)) {
    uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst);
    cfl_ns_predict_hbd_c(cfl->ac_buf_q3, dst_16, dst_stride, alpha_q12,
                       tx_size_wide[tx_size], tx_size_high[tx_size]);
    return;
  }
  cfl_ns_predict_lbd_c(cfl->ac_buf_q3, dst, dst_stride, alpha_q12,
                       tx_size_wide[tx_size], tx_size_high[tx_size]);
}

static inline int32_t ClampToSigned32(int32_t v, uint32_t num_bits) {
  assert(num_bits > 0 && num_bits < sizeof(int32_t) * 8);
  const int32_t limit = (int32_t)1 << (num_bits - 1);
  return clamp(v, -limit, limit - 1);
}

static void linear_regression_high(int width, int height, const uint16_t* const y_above,
                                    const uint16_t* const y_left,
                                    const uint16_t* const uv_above,
                                    const uint16_t* const uv_left,
                                    int32_t* const a, int32_t* const b) {
  // Max precision for YUV values, excluding the sign.
  const uint32_t kYuvMaxPrec = 9u;
  const uint32_t kCflFracBits = 12;
  const uint32_t kCflABits = kCflFracBits + 2 + 1;
  const uint32_t kCflBBits = kCflFracBits + kYuvMaxPrec + 2;

  // TODO make sure there are no overflows
  int64_t num_values = width + height;
  int64_t l_sum = 0;
  int64_t uv_sum = 0;
  int64_t l_uv_sum = 0;
  int64_t l_l_sum = 0;
  for (int i = 0; i < width; ++i) {
    const uint16_t l = y_above[i], uv = uv_above[i];
    l_sum += l;
    uv_sum += uv;
    l_uv_sum += l * uv;
    l_l_sum += l * l;
  }
  for (int i = 0; i < height; ++i) {
    const uint16_t l = y_left[i], uv = uv_left[i];
    l_sum += l;
    uv_sum += uv;
    l_uv_sum += l * uv;
    l_l_sum += l * l;
  }
  const int64_t num = l_uv_sum * num_values - l_sum * uv_sum;
  const int64_t den = l_l_sum * num_values - l_sum * l_sum;
  int32_t A = 0;
  int32_t B = DIVIDE_AND_ROUND(uv_sum << kCflFracBits, num_values);
  if (num != 0 && den != 0) {
    A = DIVIDE_AND_ROUND(num << kCflFracBits, den);  // fits in 64b precision
  }
  B += (1 << kCflFracBits >> 1);   // include rounding constant

  // Tighten the scaling/offset value to avoid obvious overflows.
  // Warning! alpha-from-chroma heavily relies on saturating to [0..255],
  // and generate large 'B' constants. We must not clip 'B' with min/max_value_
  // but with the max codable range (kYuvMaxPrec = 9b) to retain this property.
  const int32_t max_range = 2 << (kCflFracBits + kYuvMaxPrec);
  if (abs(B) > max_range) {
    A = 0;
  }
  *a = ClampToSigned32(A, kCflABits);
  *b = ClampToSigned32(B, kCflBBits);
}

void cfl_ns_calc_alpha_high(CFL_CTX *const cfl,
                    CFL_PRED_TYPE pred_plane,
                    TX_SIZE tx_size,
                    int x, int y,
                    const uint16_t *above,
                    const uint16_t *left) {
  uint16_t *const y_above = cfl->above_ref + x + (y >> 2) * CFL_BUF_LINE;
  uint16_t *const y_left = cfl->left_ref + y + (x >> 2) * CFL_BUF_LINE;
  int32_t a;
  int32_t b; // TODO should be passed in?
  linear_regression_high(tx_size_wide[tx_size], tx_size_high[tx_size], y_above,
                    y_left, above, left, &a, &b);
  cfl->alpha_q12[pred_plane] = a;
}

