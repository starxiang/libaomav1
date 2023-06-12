/*
 * Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/variance.h"

// The bilinear filters look like this:
//
// {{ 128,  0 }, { 112, 16 }, { 96, 32 }, { 80,  48 },
//  {  64, 64 }, {  48, 80 }, { 32, 96 }, { 16, 112 }}
//
// We can factor out the highest common multiple, such that the sum of both
// weights will be 8 instead of 128. The benefits of this are two-fold:
//
// 1) We can infer the filter values from the filter_offset parameter in the
// bilinear filter functions below - we don't have to actually load the values
// from memory:
// f0 = 8 - filter_offset
// f1 = filter_offset
//
// 2) Scaling the pixel values by 8, instead of 128 enables us to operate on
// 16-bit data types at all times, rather than widening out to 32-bit and
// requiring double the number of data processing instructions. (12-bit * 8 =
// 15-bit.)

// Process a block exactly 4 wide and any height.
static void highbd_var_filter_block2d_bil_w4(const uint16_t *src_ptr,
                                             uint16_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  const uint16x4_t f0 = vdup_n_u16(8 - filter_offset);
  const uint16x4_t f1 = vdup_n_u16(filter_offset);

  int i = dst_height;
  do {
    uint16x4_t s0 = load_unaligned_u16_4x1(src_ptr);
    uint16x4_t s1 = load_unaligned_u16_4x1(src_ptr + pixel_step);

    uint16x4_t blend = vmul_u16(s0, f0);
    blend = vmla_u16(blend, s1, f1);
    blend = vrshr_n_u16(blend, 3);

    vst1_u16(dst_ptr, blend);

    src_ptr += src_stride;
    dst_ptr += 4;
  } while (--i != 0);
}

// Process a block which is a multiple of 8 and any height.
static void highbd_var_filter_block2d_bil_large(const uint16_t *src_ptr,
                                                uint16_t *dst_ptr,
                                                int src_stride, int pixel_step,
                                                int dst_width, int dst_height,
                                                int filter_offset) {
  const uint16x8_t f0 = vdupq_n_u16(8 - filter_offset);
  const uint16x8_t f1 = vdupq_n_u16(filter_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);

      uint16x8_t blend = vmulq_u16(s0, f0);
      blend = vmlaq_u16(blend, s1, f1);
      blend = vrshrq_n_u16(blend, 3);

      vst1q_u16(dst_ptr + j, blend);

      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_var_filter_block2d_bil_w8(const uint16_t *src_ptr,
                                             uint16_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      8, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w16(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      16, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w32(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      32, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w64(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      64, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w128(const uint16_t *src_ptr,
                                               uint16_t *dst_ptr,
                                               int src_stride, int pixel_step,
                                               int dst_height,
                                               int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      128, dst_height, filter_offset);
}

#define HBD_SUBPEL_VARIANCE_WXH_NEON(w, h)                                    \
  unsigned int aom_highbd_8_sub_pixel_variance##w##x##h##_neon(               \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, uint32_t *sse) {                    \
    uint16_t tmp0[w * (h + 1)];                                               \
    uint16_t tmp1[w * h];                                                     \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                             \
                                                                              \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1), \
                                       xoffset);                              \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);         \
                                                                              \
    return aom_highbd_8_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp1), w, ref,   \
                                          ref_stride, sse);                   \
  }                                                                           \
                                                                              \
  unsigned int aom_highbd_10_sub_pixel_variance##w##x##h##_neon(              \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, uint32_t *sse) {                    \
    uint16_t tmp0[w * (h + 1)];                                               \
    uint16_t tmp1[w * h];                                                     \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                             \
                                                                              \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1), \
                                       xoffset);                              \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);         \
                                                                              \
    return aom_highbd_10_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp1), w, ref,  \
                                           ref_stride, sse);                  \
  }                                                                           \
                                                                              \
  unsigned int aom_highbd_12_sub_pixel_variance##w##x##h##_neon(              \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, uint32_t *sse) {                    \
    uint16_t tmp0[w * (h + 1)];                                               \
    uint16_t tmp1[w * h];                                                     \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                             \
                                                                              \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1), \
                                       xoffset);                              \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);         \
                                                                              \
    return aom_highbd_12_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp1), w, ref,  \
                                           ref_stride, sse);                  \
  }

HBD_SUBPEL_VARIANCE_WXH_NEON(4, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(4, 8)

HBD_SUBPEL_VARIANCE_WXH_NEON(8, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 8)
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 16)

HBD_SUBPEL_VARIANCE_WXH_NEON(16, 8)
HBD_SUBPEL_VARIANCE_WXH_NEON(16, 16)
HBD_SUBPEL_VARIANCE_WXH_NEON(16, 32)

HBD_SUBPEL_VARIANCE_WXH_NEON(32, 16)
HBD_SUBPEL_VARIANCE_WXH_NEON(32, 32)
HBD_SUBPEL_VARIANCE_WXH_NEON(32, 64)

HBD_SUBPEL_VARIANCE_WXH_NEON(64, 32)
HBD_SUBPEL_VARIANCE_WXH_NEON(64, 64)
HBD_SUBPEL_VARIANCE_WXH_NEON(64, 128)

HBD_SUBPEL_VARIANCE_WXH_NEON(128, 64)
HBD_SUBPEL_VARIANCE_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_VARIANCE_WXH_NEON(4, 16)

HBD_SUBPEL_VARIANCE_WXH_NEON(8, 32)

HBD_SUBPEL_VARIANCE_WXH_NEON(16, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(16, 64)

HBD_SUBPEL_VARIANCE_WXH_NEON(32, 8)

HBD_SUBPEL_VARIANCE_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
