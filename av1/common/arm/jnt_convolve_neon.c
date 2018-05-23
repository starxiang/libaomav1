/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "av1/common/common.h"
#include "av1/common/arm/convolve_neon.h"
#include "av1/common/arm/mem_neon.h"
#include "av1/common/arm/transpose_neon.h"

static INLINE void compute_avg_4x4(
    uint16x4_t res0, uint16x4_t res1, uint16x4_t res2, uint16x4_t res3,
    uint16x4_t d0, uint16x4_t d1, uint16x4_t d2, uint16x4_t d3,
    const uint16_t fwd_offset, const uint16_t bck_offset,
    const int16x4_t sub_const_vec, const int16_t round_bits,
    const int32_t use_jnt_comp_avg, uint8x8_t *t0, uint8x8_t *t1) {
  int16x4_t tmp0, tmp1, tmp2, tmp3;
  uint16x4_t tmp_u0, tmp_u1, tmp_u2, tmp_u3;
  uint32x4_t sum0, sum1, sum2, sum3;

  int32x4_t dst0, dst1, dst2, dst3;
  int16x8_t tmp4, tmp5;
  const int16x8_t zero = vdupq_n_s16(0);

  if (use_jnt_comp_avg) {
    const int32x4_t round_bits_vec = vdupq_n_s32((int32_t)(-round_bits));
    const int32x4_t const_vec = vmovl_s16(sub_const_vec);

    sum0 = vmull_n_u16(res0, fwd_offset);
    sum0 = vmlal_n_u16(sum0, d0, bck_offset);
    sum1 = vmull_n_u16(res1, fwd_offset);
    sum1 = vmlal_n_u16(sum1, d1, bck_offset);
    sum2 = vmull_n_u16(res2, fwd_offset);
    sum2 = vmlal_n_u16(sum2, d2, bck_offset);
    sum3 = vmull_n_u16(res3, fwd_offset);
    sum3 = vmlal_n_u16(sum3, d3, bck_offset);

    sum0 = vshrq_n_u32(sum0, DIST_PRECISION_BITS);
    sum1 = vshrq_n_u32(sum1, DIST_PRECISION_BITS);
    sum2 = vshrq_n_u32(sum2, DIST_PRECISION_BITS);
    sum3 = vshrq_n_u32(sum3, DIST_PRECISION_BITS);

    dst0 = vsubq_s32(vreinterpretq_s32_u32(sum0), const_vec);
    dst1 = vsubq_s32(vreinterpretq_s32_u32(sum1), const_vec);
    dst2 = vsubq_s32(vreinterpretq_s32_u32(sum2), const_vec);
    dst3 = vsubq_s32(vreinterpretq_s32_u32(sum3), const_vec);

    dst0 = vqrshlq_s32(dst0, round_bits_vec);
    dst1 = vqrshlq_s32(dst1, round_bits_vec);
    dst2 = vqrshlq_s32(dst2, round_bits_vec);
    dst3 = vqrshlq_s32(dst3, round_bits_vec);

    tmp0 = vqmovn_s32(dst0);
    tmp1 = vqmovn_s32(dst1);
    tmp2 = vqmovn_s32(dst2);
    tmp3 = vqmovn_s32(dst3);
    tmp4 = vcombine_s16(tmp0, tmp1);
    tmp5 = vcombine_s16(tmp2, tmp3);
    tmp4 = vmaxq_s16(tmp4, zero);
    tmp5 = vmaxq_s16(tmp5, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(tmp4));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(tmp5));
  } else {
    const int16x4_t round_bits_vec = vdup_n_s16(-round_bits);
    tmp_u0 = vhadd_u16(res0, d0);
    tmp_u1 = vhadd_u16(res1, d1);
    tmp_u2 = vhadd_u16(res2, d2);
    tmp_u3 = vhadd_u16(res3, d3);

    tmp0 = vsub_s16(vreinterpret_s16_u16(tmp_u0), sub_const_vec);
    tmp1 = vsub_s16(vreinterpret_s16_u16(tmp_u1), sub_const_vec);
    tmp2 = vsub_s16(vreinterpret_s16_u16(tmp_u2), sub_const_vec);
    tmp3 = vsub_s16(vreinterpret_s16_u16(tmp_u3), sub_const_vec);

    tmp0 = vqrshl_s16(tmp0, round_bits_vec);
    tmp1 = vqrshl_s16(tmp1, round_bits_vec);
    tmp2 = vqrshl_s16(tmp2, round_bits_vec);
    tmp3 = vqrshl_s16(tmp3, round_bits_vec);

    tmp4 = vcombine_s16(tmp0, tmp1);
    tmp5 = vcombine_s16(tmp2, tmp3);
    tmp4 = vmaxq_s16(tmp4, zero);
    tmp5 = vmaxq_s16(tmp5, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(tmp4));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(tmp5));
  }
}

static INLINE void compute_avg_8x4(
    uint16x8_t res0, uint16x8_t res1, uint16x8_t res2, uint16x8_t res3,
    uint16x8_t d0, uint16x8_t d1, uint16x8_t d2, uint16x8_t d3,
    const uint16_t fwd_offset, const uint16_t bck_offset,
    const int16x4_t sub_const, const int16_t round_bits,
    const int32_t use_jnt_comp_avg, uint8x8_t *t0, uint8x8_t *t1, uint8x8_t *t2,
    uint8x8_t *t3) {
  int16x4_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int16x8_t f0, f1, f2, f3;
  uint32x4_t sum0, sum1, sum2, sum3;
  uint32x4_t sum4, sum5, sum6, sum7;
  int32x4_t dst0, dst1, dst2, dst3;
  int32x4_t dst4, dst5, dst6, dst7;
  uint16x8_t tmp_u0, tmp_u1, tmp_u2, tmp_u3;
  const int16x8_t zero = vdupq_n_s16(0);

  if (use_jnt_comp_avg) {
    const int32x4_t sub_const_vec = vmovl_s16(sub_const);
    const int32x4_t round_bits_vec = vdupq_n_s32(-(int32_t)round_bits);

    sum0 = vmull_n_u16(vget_low_u16(res0), fwd_offset);
    sum0 = vmlal_n_u16(sum0, vget_low_u16(d0), bck_offset);
    sum1 = vmull_n_u16(vget_low_u16(res1), fwd_offset);
    sum1 = vmlal_n_u16(sum1, vget_low_u16(d1), bck_offset);
    sum0 = vshrq_n_u32(sum0, DIST_PRECISION_BITS);
    sum1 = vshrq_n_u32(sum1, DIST_PRECISION_BITS);

    sum2 = vmull_n_u16(vget_high_u16(res0), fwd_offset);
    sum2 = vmlal_n_u16(sum2, vget_high_u16(d0), bck_offset);
    sum3 = vmull_n_u16(vget_high_u16(res1), fwd_offset);
    sum3 = vmlal_n_u16(sum3, vget_high_u16(d1), bck_offset);
    sum2 = vshrq_n_u32(sum2, DIST_PRECISION_BITS);
    sum3 = vshrq_n_u32(sum3, DIST_PRECISION_BITS);

    sum4 = vmull_n_u16(vget_low_u16(res2), fwd_offset);
    sum4 = vmlal_n_u16(sum4, vget_low_u16(d2), bck_offset);
    sum5 = vmull_n_u16(vget_low_u16(res3), fwd_offset);
    sum5 = vmlal_n_u16(sum5, vget_low_u16(d3), bck_offset);
    sum4 = vshrq_n_u32(sum4, DIST_PRECISION_BITS);
    sum5 = vshrq_n_u32(sum5, DIST_PRECISION_BITS);

    sum6 = vmull_n_u16(vget_high_u16(res2), fwd_offset);
    sum6 = vmlal_n_u16(sum6, vget_high_u16(d2), bck_offset);
    sum7 = vmull_n_u16(vget_high_u16(res3), fwd_offset);
    sum7 = vmlal_n_u16(sum7, vget_high_u16(d3), bck_offset);
    sum6 = vshrq_n_u32(sum6, DIST_PRECISION_BITS);
    sum7 = vshrq_n_u32(sum7, DIST_PRECISION_BITS);

    dst0 = vsubq_s32(vreinterpretq_s32_u32(sum0), sub_const_vec);
    dst1 = vsubq_s32(vreinterpretq_s32_u32(sum1), sub_const_vec);
    dst2 = vsubq_s32(vreinterpretq_s32_u32(sum2), sub_const_vec);
    dst3 = vsubq_s32(vreinterpretq_s32_u32(sum3), sub_const_vec);
    dst4 = vsubq_s32(vreinterpretq_s32_u32(sum4), sub_const_vec);
    dst5 = vsubq_s32(vreinterpretq_s32_u32(sum5), sub_const_vec);
    dst6 = vsubq_s32(vreinterpretq_s32_u32(sum6), sub_const_vec);
    dst7 = vsubq_s32(vreinterpretq_s32_u32(sum7), sub_const_vec);

    dst0 = vqrshlq_s32(dst0, round_bits_vec);
    dst1 = vqrshlq_s32(dst1, round_bits_vec);
    dst2 = vqrshlq_s32(dst2, round_bits_vec);
    dst3 = vqrshlq_s32(dst3, round_bits_vec);
    dst4 = vqrshlq_s32(dst4, round_bits_vec);
    dst5 = vqrshlq_s32(dst5, round_bits_vec);
    dst6 = vqrshlq_s32(dst6, round_bits_vec);
    dst7 = vqrshlq_s32(dst7, round_bits_vec);

    tmp0 = vqmovn_s32(dst0);
    tmp1 = vqmovn_s32(dst1);
    tmp2 = vqmovn_s32(dst2);
    tmp3 = vqmovn_s32(dst3);
    tmp4 = vqmovn_s32(dst4);
    tmp5 = vqmovn_s32(dst5);
    tmp6 = vqmovn_s32(dst6);
    tmp7 = vqmovn_s32(dst7);

    f0 = vcombine_s16(tmp0, tmp2);
    f1 = vcombine_s16(tmp1, tmp3);
    f2 = vcombine_s16(tmp4, tmp6);
    f3 = vcombine_s16(tmp5, tmp7);

    f0 = vmaxq_s16(f0, zero);
    f1 = vmaxq_s16(f1, zero);
    f2 = vmaxq_s16(f2, zero);
    f3 = vmaxq_s16(f3, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(f0));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(f1));
    *t2 = vqmovn_u16(vreinterpretq_u16_s16(f2));
    *t3 = vqmovn_u16(vreinterpretq_u16_s16(f3));

  } else {
    const int16x8_t sub_const_vec = vcombine_s16(sub_const, sub_const);
    const int16x8_t round_bits_vec = vdupq_n_s16(-round_bits);

    tmp_u0 = vhaddq_u16(res0, d0);
    tmp_u1 = vhaddq_u16(res1, d1);
    tmp_u2 = vhaddq_u16(res2, d2);
    tmp_u3 = vhaddq_u16(res3, d3);

    f0 = vsubq_s16(vreinterpretq_s16_u16(tmp_u0), sub_const_vec);
    f1 = vsubq_s16(vreinterpretq_s16_u16(tmp_u1), sub_const_vec);
    f2 = vsubq_s16(vreinterpretq_s16_u16(tmp_u2), sub_const_vec);
    f3 = vsubq_s16(vreinterpretq_s16_u16(tmp_u3), sub_const_vec);

    f0 = vqrshlq_s16(f0, round_bits_vec);
    f1 = vqrshlq_s16(f1, round_bits_vec);
    f2 = vqrshlq_s16(f2, round_bits_vec);
    f3 = vqrshlq_s16(f3, round_bits_vec);

    f0 = vmaxq_s16(f0, zero);
    f1 = vmaxq_s16(f1, zero);
    f2 = vmaxq_s16(f2, zero);
    f3 = vmaxq_s16(f3, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(f0));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(f1));
    *t2 = vqmovn_u16(vreinterpretq_u16_s16(f2));
    *t3 = vqmovn_u16(vreinterpretq_u16_s16(f3));
  }
}

static INLINE void jnt_convolve_2d_horiz_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    int16_t *x_filter_tmp, const int im_h, int w, const int round_0) {
  const int bd = 8;
  const uint8_t *s;
  int16_t *dst_ptr;
  int dst_stride;
  int width, height;
  uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;

  dst_ptr = im_block;
  dst_stride = im_stride;
  height = im_h;
  width = w;

  if (w == 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3;

    const int16x4_t horiz_const = vdup_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x4_t shift_round_0 = vdup_n_s16(-(round_0));

    do {
      s = src;
      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s0 = vget_low_s16(tt0);
      s1 = vget_low_s16(tt1);
      s2 = vget_low_s16(tt2);
      s3 = vget_low_s16(tt3);
      s4 = vget_high_s16(tt0);
      s5 = vget_high_s16(tt1);
      s6 = vget_high_s16(tt2);
      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      s += 7;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s7 = vget_low_s16(tt0);
      s8 = vget_low_s16(tt1);
      s9 = vget_low_s16(tt2);
      s10 = vget_low_s16(tt3);

      d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                             horiz_const, shift_round_0);
      d1 = convolve8_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                             horiz_const, shift_round_0);
      d2 = convolve8_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                             horiz_const, shift_round_0);
      d3 = convolve8_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                             horiz_const, shift_round_0);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      vst1_s16((dst_ptr + 0 * dst_stride), d0);
      vst1_s16((dst_ptr + 1 * dst_stride), d1);
      vst1_s16((dst_ptr + 2 * dst_stride), d2);
      vst1_s16((dst_ptr + 3 * dst_stride), d3);

      src += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    int16_t *d_tmp;
    int16x8_t s11, s12, s13, s14;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int16x8_t res0, res1, res2, res3, res4, res5, res6, res7;

    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x8_t shift_round_0 = vdupq_n_s16(-(round_0));

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      __builtin_prefetch(src + 7 * src_stride);
      load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      width = w;
      s = src + 7;
      d_tmp = dst_ptr;
      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      __builtin_prefetch(dst_ptr + 4 * dst_stride);
      __builtin_prefetch(dst_ptr + 5 * dst_stride);
      __builtin_prefetch(dst_ptr + 6 * dst_stride);
      __builtin_prefetch(dst_ptr + 7 * dst_stride);

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        res0 = convolve8_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res1 = convolve8_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res2 = convolve8_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res3 = convolve8_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res4 = convolve8_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res5 = convolve8_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res6 = convolve8_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res7 = convolve8_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                 x_filter_tmp, horiz_const, shift_round_0);

        transpose_s16_8x8(&res0, &res1, &res2, &res3, &res4, &res5, &res6,
                          &res7);

        store_s16_8x8(d_tmp, dst_stride, res0, res1, res2, res3, res4, res5,
                      res6, res7);
        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src += 8 * src_stride;
      dst_ptr += 8 * dst_stride;
      height -= 8;
    } while (height > 0);
  }
}

static INLINE void jnt_convolve_2d_vert_neon(
    int16_t *im_block, const int im_stride, uint8_t *dst8, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  uint8_t *dst_u8_ptr, *d_u8;
  CONV_BUF_TYPE *dst_ptr, *dst;
  int16_t *src_ptr, *s;
  uint16_t *d;

  const int bd = 8;
  int height;
  int dst_stride = conv_params->dst_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t sub_const = (1 << (offset_bits - conv_params->round_1)) +
                            (1 << (offset_bits - conv_params->round_1 - 1));

  const int16_t round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int32x4_t round_shift_vec = vdupq_n_s32(-(conv_params->round_1));
  const int32x4_t offset_const = vdupq_n_s32(1 << offset);
  const int16x4_t sub_const_vec = vdup_n_s16(sub_const);
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  const int do_average = conv_params->do_average;
  const int use_jnt_comp_avg = conv_params->use_jnt_comp_avg;

  int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  uint16x4_t res4, res5, res6, res7;
  uint16x4_t d0, d1, d2, d3;
  uint8x8_t t0, t1;

  dst = conv_params->dst;
  src_ptr = im_block;
  dst_u8_ptr = dst8;
  dst_ptr = dst;
  height = h;

  do {
    d = dst_ptr;
    d_u8 = dst_u8_ptr;
    s = src_ptr;
    height = h;

    __builtin_prefetch(s + 0 * im_stride);
    __builtin_prefetch(s + 1 * im_stride);
    __builtin_prefetch(s + 2 * im_stride);
    __builtin_prefetch(s + 3 * im_stride);
    __builtin_prefetch(s + 4 * im_stride);
    __builtin_prefetch(s + 5 * im_stride);
    __builtin_prefetch(s + 6 * im_stride);
    __builtin_prefetch(s + 7 * im_stride);

    load_s16_4x8(s, im_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
    s += (7 * im_stride);

    do {
      load_s16_4x4(s, im_stride, &s7, &s8, &s9, &s10);
      s += (im_stride << 2);

      __builtin_prefetch(d + 0 * dst_stride);
      __builtin_prefetch(d + 1 * dst_stride);
      __builtin_prefetch(d + 2 * dst_stride);
      __builtin_prefetch(d + 3 * dst_stride);

      __builtin_prefetch(d_u8 + 4 * dst8_stride);
      __builtin_prefetch(d_u8 + 5 * dst8_stride);
      __builtin_prefetch(d_u8 + 6 * dst8_stride);
      __builtin_prefetch(d_u8 + 7 * dst8_stride);

      d0 = convolve8_4x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                             round_shift_vec, offset_const);
      d1 = convolve8_4x4_s32(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                             round_shift_vec, offset_const);
      d2 = convolve8_4x4_s32(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                             round_shift_vec, offset_const);
      d3 = convolve8_4x4_s32(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                             round_shift_vec, offset_const);

      if (do_average) {
        load_u16_4x4(d, dst_stride, &res4, &res5, &res6, &res7);
        d += (dst_stride << 2);

        compute_avg_4x4(res4, res5, res6, res7, d0, d1, d2, d3, fwd_offset,
                        bck_offset, sub_const_vec, round_bits, use_jnt_comp_avg,
                        &t0, &t1);

        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                      0);  // 00 01 02 03
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                      1);  // 10 11 12 13
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                      0);  // 20 21 22 23
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                      1);  // 30 31 32 33
        d_u8 += dst8_stride;

      } else {
        store_u16_4x4(d, dst_stride, d0, d1, d2, d3);
        d += (dst_stride << 2);
      }
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      height -= 4;
    } while (height > 0);
    src_ptr += 4;
    dst_ptr += 4;
    dst_u8_ptr += 4;
    w -= 4;
  } while (w > 0);
}

void av1_jnt_convolve_2d_neon(const uint8_t *src, int src_stride, uint8_t *dst8,
                              int dst8_stride, int w, int h,
                              InterpFilterParams *filter_params_x,
                              InterpFilterParams *filter_params_y,
                              const int subpel_x_q4, const int subpel_y_q4,
                              ConvolveParams *conv_params) {
  assert(!(w % 4));
  assert(!(h % 4));

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

  const int im_h = h + filter_params_y->taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int round_0 = conv_params->round_0 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

  int16_t x_filter_tmp[8];
  int16x8_t filter_x_coef = vld1q_s16(x_filter);

  // filter coeffs are even, so downshifting by 1 to reduce intermediate
  // precision requirements.
  filter_x_coef = vshrq_n_s16(filter_x_coef, 1);
  vst1q_s16(&x_filter_tmp[0], filter_x_coef);

  jnt_convolve_2d_horiz_neon(src_ptr, src_stride, im_block, im_stride,
                             x_filter_tmp, im_h, w, round_0);

  jnt_convolve_2d_vert_neon(im_block, im_stride, dst8, dst8_stride, conv_params,
                            y_filter, h, w);
}

void av1_jnt_convolve_x_neon(const uint8_t *src, int src_stride, uint8_t *dst8,
                             int dst8_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params) {
  assert(!(w % 4));
  assert(!(h % 4));

  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  const int use_jnt_comp_avg = conv_params->use_jnt_comp_avg;

  (void)filter_params_y;
  (void)subpel_y_q4;

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

  const uint8_t *src_ptr = src - horiz_offset;

  int16_t x_filter_tmp[8];
  int16x8_t filter_x_coef = vld1q_s16(x_filter);

  // filter coeffs are even, so downshifting by 1 to reduce intermediate
  // precision requirements.
  filter_x_coef = vshrq_n_s16(filter_x_coef, 1);
  vst1q_s16(&x_filter_tmp[0], filter_x_coef);

  const uint8_t *s;
  uint8_t *d_u8;
  uint8_t *dst_u8_ptr;
  CONV_BUF_TYPE *d, *dst_ptr;
  int width, height;
  uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;

  s = src_ptr;
  dst_ptr = dst;
  dst_u8_ptr = dst8;
  width = w;
  height = h;

  if ((w == 4) || (h == 4)) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3;
    uint16x4_t res4, res5, res6, res7;
    const int16x4_t zero = vdup_n_s16(0);
    const int16x4_t round_offset_vec = vdup_n_s16(round_offset);
    const int16x4_t shift_round_0 = vdup_n_s16(-conv_params->round_0 + 1);
    const int16x4_t horiz_const = vdup_n_s16(bits);
    do {
      s = src_ptr;
      d = dst_ptr;
      d_u8 = dst_u8_ptr;
      width = w;
      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s0 = vget_low_s16(tt0);
      s1 = vget_low_s16(tt1);
      s2 = vget_low_s16(tt2);
      s3 = vget_low_s16(tt3);
      s4 = vget_high_s16(tt0);
      s5 = vget_high_s16(tt1);
      s6 = vget_high_s16(tt2);
      __builtin_prefetch(d + 0 * dst_stride);
      __builtin_prefetch(d + 1 * dst_stride);
      __builtin_prefetch(d + 2 * dst_stride);
      __builtin_prefetch(d + 3 * dst_stride);
      s += 7;
      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        transpose_u8_8x4(&t0, &t1, &t2, &t3);
        tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s7 = vget_low_s16(tt0);
        s8 = vget_low_s16(tt1);
        s9 = vget_low_s16(tt2);
        s10 = vget_low_s16(tt3);

        d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                               zero, shift_round_0);
        d0 = vrshl_s16(d0, horiz_const);
        d0 = vadd_s16(d0, round_offset_vec);
        d1 = convolve8_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                               zero, shift_round_0);
        d1 = vrshl_s16(d1, horiz_const);
        d1 = vadd_s16(d1, round_offset_vec);
        d2 = convolve8_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                               zero, shift_round_0);
        d2 = vrshl_s16(d2, horiz_const);
        d2 = vadd_s16(d2, round_offset_vec);
        d3 = convolve8_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                               zero, shift_round_0);
        d3 = vrshl_s16(d3, horiz_const);
        d3 = vadd_s16(d3, round_offset_vec);

        transpose_s16_4x4d(&d0, &d1, &d2, &d3);

        if (conv_params->do_average) {
          __builtin_prefetch(d + 0 * dst_stride);
          __builtin_prefetch(d + 1 * dst_stride);
          __builtin_prefetch(d + 2 * dst_stride);
          __builtin_prefetch(d + 3 * dst_stride);

          __builtin_prefetch(d_u8 + 0 * dst8_stride);
          __builtin_prefetch(d_u8 + 1 * dst8_stride);
          __builtin_prefetch(d_u8 + 2 * dst8_stride);
          __builtin_prefetch(d_u8 + 3 * dst8_stride);

          load_u16_4x4(d, dst_stride, &res4, &res5, &res6, &res7);

          compute_avg_4x4(res4, res5, res6, res7, vreinterpret_u16_s16(d0),
                          vreinterpret_u16_s16(d1), vreinterpret_u16_s16(d2),
                          vreinterpret_u16_s16(d3), fwd_offset, bck_offset,
                          round_offset_vec, round_bits, use_jnt_comp_avg, &t0,
                          &t1);

          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          vst1_lane_u32((uint32_t *)(d_u8 + dst8_stride),
                        vreinterpret_u32_u8(t0),
                        1);  // 10 11 12 13
          vst1_lane_u32((uint32_t *)(d_u8 + 2 * dst8_stride),
                        vreinterpret_u32_u8(t1),
                        0);  // 20 21 22 23
          vst1_lane_u32((uint32_t *)(d_u8 + 3 * dst8_stride),
                        vreinterpret_u32_u8(t1),
                        1);  // 30 31 32 33
        } else {
          store_u16_4x4(d, dst_stride, vreinterpret_u16_s16(d0),
                        vreinterpret_u16_s16(d1), vreinterpret_u16_s16(d2),
                        vreinterpret_u16_s16(d3));
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;

        s += 4;
        width -= 4;
        d += 4;
        d_u8 += 4;
      } while (width > 0);
      src_ptr += (src_stride << 2);
      dst_ptr += (dst_stride << 2);
      dst_u8_ptr += (dst8_stride << 2);
      height -= 4;
    } while (height > 0);
  } else {
    CONV_BUF_TYPE *d_tmp;
    uint8_t *d_u8_tmp;
    int16x8_t s11, s12, s13, s14;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int16x8_t res0, res1, res2, res3, res4, res5, res6, res7;
    uint16x8_t res8, res9, res10, res11;

    const int16x8_t round_offset128 = vdupq_n_s16(round_offset);
    const int16x4_t round_offset64 = vdup_n_s16(round_offset);
    const int16x8_t shift_round_0 = vdupq_n_s16(-conv_params->round_0 + 1);
    const int16x8_t horiz_const = vdupq_n_s16(bits);
    const int16x8_t zero = vdupq_n_s16(0);

    d = dst_ptr = dst;
    d_u8 = dst_u8_ptr = dst8;
    do {
      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);
      __builtin_prefetch(src_ptr + 4 * src_stride);
      __builtin_prefetch(src_ptr + 5 * src_stride);
      __builtin_prefetch(src_ptr + 6 * src_stride);
      __builtin_prefetch(src_ptr + 7 * src_stride);
      load_u8_8x8(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      width = w;
      s = src_ptr + 7;
      d = dst_ptr;
      d_u8_tmp = dst_u8_ptr;

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      __builtin_prefetch(dst_ptr + 4 * dst_stride);
      __builtin_prefetch(dst_ptr + 5 * dst_stride);
      __builtin_prefetch(dst_ptr + 6 * dst_stride);
      __builtin_prefetch(dst_ptr + 7 * dst_stride);

      do {
        d_u8 = d_u8_tmp;
        d_tmp = d;

        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        res0 = convolve8_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                                 zero, shift_round_0);

        res0 = vrshlq_s16(res0, horiz_const);
        res0 = vaddq_s16(res0, round_offset128);

        res1 = convolve8_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                                 zero, shift_round_0);
        res1 = vrshlq_s16(res1, horiz_const);
        res1 = vaddq_s16(res1, round_offset128);
        res2 = convolve8_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                                 zero, shift_round_0);
        res2 = vrshlq_s16(res2, horiz_const);
        res2 = vaddq_s16(res2, round_offset128);
        res3 = convolve8_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                                 zero, shift_round_0);
        res3 = vrshlq_s16(res3, horiz_const);
        res3 = vaddq_s16(res3, round_offset128);
        res4 = convolve8_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, x_filter_tmp,
                                 zero, shift_round_0);
        res4 = vrshlq_s16(res4, horiz_const);
        res4 = vaddq_s16(res4, round_offset128);
        res5 = convolve8_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                 x_filter_tmp, zero, shift_round_0);
        res5 = vrshlq_s16(res5, horiz_const);
        res5 = vaddq_s16(res5, round_offset128);
        res6 = convolve8_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                 x_filter_tmp, zero, shift_round_0);
        res6 = vrshlq_s16(res6, horiz_const);
        res6 = vaddq_s16(res6, round_offset128);
        res7 = convolve8_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                 x_filter_tmp, zero, shift_round_0);
        res7 = vrshlq_s16(res7, horiz_const);
        res7 = vaddq_s16(res7, round_offset128);

        transpose_s16_8x8(&res0, &res1, &res2, &res3, &res4, &res5, &res6,
                          &res7);

        if (conv_params->do_average) {
          load_u16_8x4(d_tmp, dst_stride, &res8, &res9, &res10, &res11);
          d_tmp += (dst_stride << 2);

          compute_avg_8x4(
              res8, res9, res10, res11, vreinterpretq_u16_s16(res0),
              vreinterpretq_u16_s16(res1), vreinterpretq_u16_s16(res2),
              vreinterpretq_u16_s16(res3), fwd_offset, bck_offset,
              round_offset64, round_bits, use_jnt_comp_avg, &t0, &t1, &t2, &t3);

          store_u8_8x4(d_u8, dst8_stride, t0, t1, t2, t3);
          d_u8 += (dst8_stride << 2);

          load_u16_8x4(d_tmp, dst_stride, &res8, &res9, &res10, &res11);
          d_tmp += (dst_stride << 2);

          compute_avg_8x4(
              res8, res9, res10, res11, vreinterpretq_u16_s16(res4),
              vreinterpretq_u16_s16(res5), vreinterpretq_u16_s16(res6),
              vreinterpretq_u16_s16(res7), fwd_offset, bck_offset,
              round_offset64, round_bits, use_jnt_comp_avg, &t0, &t1, &t2, &t3);

          store_u8_8x4(d_u8, dst8_stride, t0, t1, t2, t3);
          d_u8 += (dst8_stride << 2);
        } else {
          store_u16_8x8(
              d_tmp, dst_stride, vreinterpretq_u16_s16(res0),
              vreinterpretq_u16_s16(res1), vreinterpretq_u16_s16(res2),
              vreinterpretq_u16_s16(res3), vreinterpretq_u16_s16(res4),
              vreinterpretq_u16_s16(res5), vreinterpretq_u16_s16(res6),
              vreinterpretq_u16_s16(res7));
          d_tmp += (dst_stride << 3);
        }

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d += 8;
        width -= 8;
        d_u8_tmp += 8;
      } while (width > 0);
      src_ptr += 8 * src_stride;
      dst_ptr += 8 * dst_stride;
      dst_u8_ptr += 8 * dst8_stride;
      height -= 8;
    } while (height > 0);
  }
}

void av1_jnt_convolve_y_neon(const uint8_t *src, int src_stride, uint8_t *dst8,
                             int dst8_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params) {
  assert(!(w % 4));
  assert(!(h % 4));

  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  const int use_jnt_comp_avg = conv_params->use_jnt_comp_avg;
  const int shift_value = (conv_params->round_1 - 1 - bits);

  (void)filter_params_x;
  (void)subpel_x_q4;

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

  const uint8_t *src_ptr = src - (vert_offset * src_stride);

  int16_t y_filter_tmp[8];
  int16x8_t filter_y_coef = vld1q_s16(y_filter);

  // filter coeffs are even, so downshifting by 1 to reduce intermediate
  // precision requirements.
  filter_y_coef = vshrq_n_s16(filter_y_coef, 1);
  vst1q_s16(&y_filter_tmp[0], filter_y_coef);

  const uint8_t *s;
  uint8_t *d_u8;
  uint8_t *dst_u8_ptr;
  CONV_BUF_TYPE *d, *dst_ptr;
  int width, height;
  uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;

  s = src_ptr;
  dst_ptr = dst;
  dst_u8_ptr = dst8;
  width = w;
  height = h;

  // used to get rid of multiplication = (vertical filter output sum) *
  // (1<<bits).
  assert((conv_params->round_1 - 2) >= bits);

  if ((w == 4) || (h == 4)) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    uint16x4_t res4, res5, res6, res7;
    uint8x8x2_t d4, d5, d6, d7;

    const int16x4_t round_offset64 = vdup_n_s16(round_offset);
    const int16x4_t shift_vec = vdup_n_s16(-shift_value);
    uint8x8_t zero = vdup_n_u8(0);
    do {
      s = src_ptr;
      d = dst_ptr;
      d_u8 = dst_u8_ptr;
      height = h;
      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      d4 = vzip_u8(t0, zero);
      d5 = vzip_u8(t1, zero);
      d6 = vzip_u8(t2, zero);
      d7 = vzip_u8(t3, zero);

      res4 = vreinterpret_u16_u8(d4.val[0]);
      res5 = vreinterpret_u16_u8(d5.val[0]);
      res6 = vreinterpret_u16_u8(d6.val[0]);
      res7 = vreinterpret_u16_u8(d7.val[0]);

      s0 = vreinterpret_s16_u16(res4);
      s1 = vreinterpret_s16_u16(res5);
      s2 = vreinterpret_s16_u16(res6);
      s3 = vreinterpret_s16_u16(res7);

      d4 = vzip_u8(t4, zero);
      d5 = vzip_u8(t5, zero);
      d6 = vzip_u8(t6, zero);

      res4 = vreinterpret_u16_u8(d4.val[0]);
      res5 = vreinterpret_u16_u8(d5.val[0]);
      res6 = vreinterpret_u16_u8(d6.val[0]);

      s4 = vreinterpret_s16_u16(res4);
      s5 = vreinterpret_s16_u16(res5);
      s6 = vreinterpret_s16_u16(res6);

      __builtin_prefetch(d + 0 * dst_stride);
      __builtin_prefetch(d + 1 * dst_stride);
      __builtin_prefetch(d + 2 * dst_stride);
      __builtin_prefetch(d + 3 * dst_stride);

      s += (7 * src_stride);
      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);

        d4 = vzip_u8(t0, zero);
        d5 = vzip_u8(t1, zero);
        d6 = vzip_u8(t2, zero);
        d7 = vzip_u8(t3, zero);

        res4 = vreinterpret_u16_u8(d4.val[0]);
        res5 = vreinterpret_u16_u8(d5.val[0]);
        res6 = vreinterpret_u16_u8(d6.val[0]);
        res7 = vreinterpret_u16_u8(d7.val[0]);

        s7 = vreinterpret_s16_u16(res4);
        s8 = vreinterpret_s16_u16(res5);
        s9 = vreinterpret_s16_u16(res6);
        s10 = vreinterpret_s16_u16(res7);

        d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter_tmp,
                               vreinterpret_s16_u8(zero), shift_vec);
        d0 = vadd_s16(d0, round_offset64);
        d1 = convolve8_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, y_filter_tmp,
                               vreinterpret_s16_u8(zero), shift_vec);
        d1 = vadd_s16(d1, round_offset64);
        d2 = convolve8_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, y_filter_tmp,
                               vreinterpret_s16_u8(zero), shift_vec);
        d2 = vadd_s16(d2, round_offset64);
        d3 = convolve8_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, y_filter_tmp,
                               vreinterpret_s16_u8(zero), shift_vec);
        d3 = vadd_s16(d3, round_offset64);

        if (conv_params->do_average) {
          __builtin_prefetch(d + 0 * dst_stride);
          __builtin_prefetch(d + 1 * dst_stride);
          __builtin_prefetch(d + 2 * dst_stride);
          __builtin_prefetch(d + 3 * dst_stride);

          __builtin_prefetch(d_u8 + 0 * dst8_stride);
          __builtin_prefetch(d_u8 + 1 * dst8_stride);
          __builtin_prefetch(d_u8 + 2 * dst8_stride);
          __builtin_prefetch(d_u8 + 3 * dst8_stride);

          load_u16_4x4(d, dst_stride, &res4, &res5, &res6, &res7);
          d += (dst_stride << 2);

          compute_avg_4x4(res4, res5, res6, res7, vreinterpret_u16_s16(d0),
                          vreinterpret_u16_s16(d1), vreinterpret_u16_s16(d2),
                          vreinterpret_u16_s16(d3), fwd_offset, bck_offset,
                          round_offset64, round_bits, use_jnt_comp_avg, &t0,
                          &t1);

          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          d_u8 += dst8_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                        1);  // 10 11 12 13
          d_u8 += dst8_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                        0);  // 20 21 22 23
          d_u8 += dst8_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                        1);  // 30 31 32 33
          d_u8 += dst8_stride;
        } else {
          store_u16_4x4(d, dst_stride, vreinterpret_u16_s16(d0),
                        vreinterpret_u16_s16(d1), vreinterpret_u16_s16(d2),
                        vreinterpret_u16_s16(d3));
          d += (dst_stride << 2);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;

        s += (src_stride << 2);
        height -= 4;
      } while (height > 0);
      src_ptr += 4;
      dst_ptr += 4;
      dst_u8_ptr += 4;
      width -= 4;
    } while (width > 0);
  } else {
    CONV_BUF_TYPE *d_tmp;
    int16x8_t s11, s12, s13, s14;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int16x8_t res0, res1, res2, res3, res4, res5, res6, res7;
    uint16x8_t res8, res9, res10, res11;
    const int16x8_t round_offset128 = vdupq_n_s16(round_offset);
    const int16x8_t shift_vec = vdupq_n_s16(-shift_value);
    const int16x4_t round_offset64 = vdup_n_s16(round_offset);
    const int16x8_t zero = vdupq_n_s16(0);

    dst_ptr = dst;
    dst_u8_ptr = dst8;
    do {
      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);
      __builtin_prefetch(src_ptr + 4 * src_stride);
      __builtin_prefetch(src_ptr + 5 * src_stride);
      __builtin_prefetch(src_ptr + 6 * src_stride);
      __builtin_prefetch(src_ptr + 7 * src_stride);
      load_u8_8x8(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      height = h;
      s = src_ptr + (7 * src_stride);
      d_tmp = dst_ptr;
      d_u8 = dst_u8_ptr;

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        __builtin_prefetch(dst_ptr + 0 * dst_stride);
        __builtin_prefetch(dst_ptr + 1 * dst_stride);
        __builtin_prefetch(dst_ptr + 2 * dst_stride);
        __builtin_prefetch(dst_ptr + 3 * dst_stride);

        res0 = convolve8_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter_tmp,
                                 zero, shift_vec);
        res0 = vaddq_s16(res0, round_offset128);
        res1 = convolve8_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, y_filter_tmp,
                                 zero, shift_vec);
        res1 = vaddq_s16(res1, round_offset128);
        res2 = convolve8_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, y_filter_tmp,
                                 zero, shift_vec);
        res2 = vaddq_s16(res2, round_offset128);
        res3 = convolve8_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, y_filter_tmp,
                                 zero, shift_vec);
        res3 = vaddq_s16(res3, round_offset128);
        res4 = convolve8_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, y_filter_tmp,
                                 zero, shift_vec);
        res4 = vaddq_s16(res4, round_offset128);
        res5 = convolve8_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                 y_filter_tmp, zero, shift_vec);
        res5 = vaddq_s16(res5, round_offset128);
        res6 = convolve8_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                 y_filter_tmp, zero, shift_vec);
        res6 = vaddq_s16(res6, round_offset128);
        res7 = convolve8_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                 y_filter_tmp, zero, shift_vec);
        res7 = vaddq_s16(res7, round_offset128);

        if (conv_params->do_average) {
          __builtin_prefetch(d_tmp + 0 * dst8_stride);
          __builtin_prefetch(d_tmp + 1 * dst8_stride);
          __builtin_prefetch(d_tmp + 2 * dst8_stride);
          __builtin_prefetch(d_tmp + 3 * dst8_stride);

          load_u16_8x4(d_tmp, dst_stride, &res8, &res9, &res10, &res11);
          d_tmp += (dst_stride << 2);

          compute_avg_8x4(
              res8, res9, res10, res11, vreinterpretq_u16_s16(res0),
              vreinterpretq_u16_s16(res1), vreinterpretq_u16_s16(res2),
              vreinterpretq_u16_s16(res3), fwd_offset, bck_offset,
              round_offset64, round_bits, use_jnt_comp_avg, &t0, &t1, &t2, &t3);

          store_u8_8x4(d_u8, dst8_stride, t0, t1, t2, t3);
          d_u8 += (dst8_stride << 2);

          load_u16_8x4(d_tmp, dst_stride, &res8, &res9, &res10, &res11);
          d_tmp += (dst_stride << 2);

          compute_avg_8x4(
              res8, res9, res10, res11, vreinterpretq_u16_s16(res4),
              vreinterpretq_u16_s16(res5), vreinterpretq_u16_s16(res6),
              vreinterpretq_u16_s16(res7), fwd_offset, bck_offset,
              round_offset64, round_bits, use_jnt_comp_avg, &t0, &t1, &t2, &t3);

          store_u8_8x4(d_u8, dst8_stride, t0, t1, t2, t3);
          d_u8 += (dst8_stride << 2);
        } else {
          store_u16_8x8(
              d_tmp, dst_stride, vreinterpretq_u16_s16(res0),
              vreinterpretq_u16_s16(res1), vreinterpretq_u16_s16(res2),
              vreinterpretq_u16_s16(res3), vreinterpretq_u16_s16(res4),
              vreinterpretq_u16_s16(res5), vreinterpretq_u16_s16(res6),
              vreinterpretq_u16_s16(res7));
          d_tmp += (dst_stride << 3);
        }

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += (8 * src_stride);
        height -= 8;
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      dst_u8_ptr += 8;
      width -= 8;
    } while (width > 0);
  }
}
