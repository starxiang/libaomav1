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

#include <smmintrin.h>  // SSE4.1

#include <assert.h>

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/blend_sse4.h"
#include "aom_dsp/x86/blend_mask_sse4.h"

#include "config/aom_dsp_rtcd.h"

//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_w4_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int w, int h) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w8_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int w, int h) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_m0_b = xx_loadl_64(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_m0l_b = xx_loadl_64(mask + c);
      const __m128i v_m0h_b = xx_loadl_64(mask + c + 8);
      const __m128i v_m0l_w = _mm_cvtepu8_epi16(v_m0l_b);
      const __m128i v_m0h_w = _mm_cvtepu8_epi16(v_m0h_b);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_r_b = xx_loadu_128(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_rl_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rh_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_al_b = _mm_avg_epu8(v_rl_b, _mm_srli_si128(v_rl_b, 1));
      const __m128i v_ah_b = _mm_avg_epu8(v_rh_b, _mm_srli_si128(v_rh_b, 1));

      const __m128i v_m0l_w = _mm_and_si128(v_al_b, v_zmask_b);
      const __m128i v_m0h_w = _mm_and_si128(v_ah_b, v_zmask_b);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zero = _mm_setzero_si128();
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ra_b = xx_loadu_128(mask + c);
      const __m128i v_rb_b = xx_loadu_128(mask + c + mask_stride);
      const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

      const __m128i v_m0l_w = _mm_cvtepu8_epi16(v_a_b);
      const __m128i v_m0h_w = _mm_unpackhi_epi8(v_a_b, v_zero);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadu_128(mask);
    const __m128i v_rb_b = xx_loadu_128(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ral_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rah_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_rbl_b = xx_loadu_128(mask + mask_stride + 2 * c);
      const __m128i v_rbh_b = xx_loadu_128(mask + mask_stride + 2 * c + 16);
      const __m128i v_rvsl_b = _mm_add_epi8(v_ral_b, v_rbl_b);
      const __m128i v_rvsh_b = _mm_add_epi8(v_rah_b, v_rbh_b);
      const __m128i v_rvsal_w = _mm_and_si128(v_rvsl_b, v_zmask_b);
      const __m128i v_rvsah_w = _mm_and_si128(v_rvsh_b, v_zmask_b);
      const __m128i v_rvsbl_w =
          _mm_and_si128(_mm_srli_si128(v_rvsl_b, 1), v_zmask_b);
      const __m128i v_rvsbh_w =
          _mm_and_si128(_mm_srli_si128(v_rvsh_b, 1), v_zmask_b);
      const __m128i v_rsl_w = _mm_add_epi16(v_rvsal_w, v_rvsbl_w);
      const __m128i v_rsh_w = _mm_add_epi16(v_rvsah_w, v_rvsbh_w);

      const __m128i v_m0l_w = xx_roundn_epu16(v_rsl_w, 2);
      const __m128i v_m0h_w = xx_roundn_epu16(v_rsh_w, 2);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////

void aom_blend_a64_mask_sse4_1(uint8_t *dst, uint32_t dst_stride,
                               const uint8_t *src0, uint32_t src0_stride,
                               const uint8_t *src1, uint32_t src1_stride,
                               const uint8_t *mask, uint32_t mask_stride, int w,
                               int h, int subx, int suby) {
  typedef void (*blend_fn)(
      uint8_t * dst, uint32_t dst_stride, const uint8_t *src0,
      uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int w, int h);

  // Dimensions are: width_index X subx X suby
  static const blend_fn blend[3][2][2] = {
    { // w % 16 == 0
      { blend_a64_mask_w16n_sse4_1, blend_a64_mask_sy_w16n_sse4_1 },
      { blend_a64_mask_sx_w16n_sse4_1, blend_a64_mask_sx_sy_w16n_sse4_1 } },
    { // w == 4
      { blend_a64_mask_w4_sse4_1, blend_a64_mask_sy_w4_sse4_1 },
      { blend_a64_mask_sx_w4_sse4_1, blend_a64_mask_sx_sy_w4_sse4_1 } },
    { // w == 8
      { blend_a64_mask_w8_sse4_1, blend_a64_mask_sy_w8_sse4_1 },
      { blend_a64_mask_sx_w8_sse4_1, blend_a64_mask_sx_sy_w8_sse4_1 } }
  };

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_blend_a64_mask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                         mask, mask_stride, w, h, subx, suby);
  } else {
    blend[(w >> 2) & 3][subx != 0][suby != 0](dst, dst_stride, src0,
                                              src0_stride, src1, src1_stride,
                                              mask, mask_stride, w, h);
  }
}

//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b10);
}

static void blend_a64_mask_b12_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b12);
}

static INLINE void blend_a64_mask_bn_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_m0_b = xx_loadl_64(mask + c);
      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, w, h,
                               blend_8_b10);
}

static void blend_a64_mask_b12_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, w, h,
                               blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_r_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

      const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadl_64(mask + c);
      const __m128i v_rb_b = xx_loadl_64(mask + c + mask_stride);
      const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b10);
}

static void blend_a64_mask_b12_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rb_b = xx_loadu_128(mask + 2 * c + mask_stride);
      const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
      const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
      const __m128i v_rvsb_w =
          _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
      const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

      const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, w, h,
                                     blend_8_b10);
}

static void blend_a64_mask_b12_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, w, h,
                                     blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////

void aom_highbd_blend_a64_mask_sse4_1(uint8_t *dst_8, uint32_t dst_stride,
                                      const uint8_t *src0_8,
                                      uint32_t src0_stride,
                                      const uint8_t *src1_8,
                                      uint32_t src1_stride, const uint8_t *mask,
                                      uint32_t mask_stride, int w, int h,
                                      int subx, int suby, int bd) {
  typedef void (*blend_fn)(
      uint16_t * dst, uint32_t dst_stride, const uint16_t *src0,
      uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int w, int h);

  // Dimensions are: bd_index X width_index X subx X suby
  static const blend_fn blend[2][2][2][2] = {
    {   // bd == 8 or 10
      { // w % 8 == 0
        { blend_a64_mask_b10_w8n_sse4_1, blend_a64_mask_b10_sy_w8n_sse4_1 },
        { blend_a64_mask_b10_sx_w8n_sse4_1,
          blend_a64_mask_b10_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b10_w4_sse4_1, blend_a64_mask_b10_sy_w4_sse4_1 },
        { blend_a64_mask_b10_sx_w4_sse4_1,
          blend_a64_mask_b10_sx_sy_w4_sse4_1 } } },
    {   // bd == 12
      { // w % 8 == 0
        { blend_a64_mask_b12_w8n_sse4_1, blend_a64_mask_b12_sy_w8n_sse4_1 },
        { blend_a64_mask_b12_sx_w8n_sse4_1,
          blend_a64_mask_b12_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b12_w4_sse4_1, blend_a64_mask_b12_sy_w4_sse4_1 },
        { blend_a64_mask_b12_sx_w4_sse4_1,
          blend_a64_mask_b12_sx_sy_w4_sse4_1 } } }
  };

  assert(IMPLIES(src0_8 == dst_8, src0_stride == dst_stride));
  assert(IMPLIES(src1_8 == dst_8, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);
  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_highbd_blend_a64_mask_c(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                                src1_stride, mask, mask_stride, w, h, subx,
                                suby, bd);
  } else {
    uint16_t *const dst = CONVERT_TO_SHORTPTR(dst_8);
    const uint16_t *const src0 = CONVERT_TO_SHORTPTR(src0_8);
    const uint16_t *const src1 = CONVERT_TO_SHORTPTR(src1_8);

    blend[bd == 12][(w >> 2) & 1][subx != 0][suby != 0](
        dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
        mask_stride, w, h);
  }
}

static INLINE void blend_a64_d16_mask_w16_sse41(
    uint8_t *dst, const CONV_BUF_TYPE *src0, const CONV_BUF_TYPE *src1,
    const __m128i *m0, const __m128i *m1, const __m128i *v_round_offset,
    const __m128i *v_maxval, int shift) {
  const __m128i max_minus_m0 = _mm_sub_epi16(*v_maxval, *m0);
  const __m128i max_minus_m1 = _mm_sub_epi16(*v_maxval, *m1);
  const __m128i s0_0 = xx_loadu_128(src0);
  const __m128i s0_1 = xx_loadu_128(src0 + 8);
  const __m128i s1_0 = xx_loadu_128(src1);
  const __m128i s1_1 = xx_loadu_128(src1 + 8);
  __m128i res0_lo = _mm_madd_epi16(_mm_unpacklo_epi16(s0_0, s1_0),
                                   _mm_unpacklo_epi16(*m0, max_minus_m0));
  __m128i res0_hi = _mm_madd_epi16(_mm_unpackhi_epi16(s0_0, s1_0),
                                   _mm_unpackhi_epi16(*m0, max_minus_m0));
  __m128i res1_lo = _mm_madd_epi16(_mm_unpacklo_epi16(s0_1, s1_1),
                                   _mm_unpacklo_epi16(*m1, max_minus_m1));
  __m128i res1_hi = _mm_madd_epi16(_mm_unpackhi_epi16(s0_1, s1_1),
                                   _mm_unpackhi_epi16(*m1, max_minus_m1));
  res0_lo = _mm_srai_epi32(_mm_sub_epi32(res0_lo, *v_round_offset), shift);
  res0_hi = _mm_srai_epi32(_mm_sub_epi32(res0_hi, *v_round_offset), shift);
  res1_lo = _mm_srai_epi32(_mm_sub_epi32(res1_lo, *v_round_offset), shift);
  res1_hi = _mm_srai_epi32(_mm_sub_epi32(res1_hi, *v_round_offset), shift);
  const __m128i res0 = _mm_packs_epi32(res0_lo, res0_hi);
  const __m128i res1 = _mm_packs_epi32(res1_lo, res1_hi);
  const __m128i res = _mm_packus_epi16(res0, res1);

  _mm_storeu_si128((__m128i *)(dst), res);
}

static INLINE void lowbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m = xx_loadu_128(mask + j);
      const __m128i m0 = _mm_cvtepu8_epi16(m);
      const __m128i m1 = _mm_cvtepu8_epi16(_mm_srli_si128(m, 8));

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static INLINE void lowbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i two_w = _mm_set1_epi16(2);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + 2 * j);
      const __m128i m_i01 = xx_loadu_128(mask + 2 * j + 16);
      const __m128i m_i10 = xx_loadu_128(mask + mask_stride + 2 * j);
      const __m128i m_i11 = xx_loadu_128(mask + mask_stride + 2 * j + 16);

      const __m128i m0_ac = _mm_adds_epu8(m_i00, m_i10);
      const __m128i m1_ac = _mm_adds_epu8(m_i01, m_i11);
      const __m128i m0_acbd = _mm_maddubs_epi16(m0_ac, one_b);
      const __m128i m1_acbd = _mm_maddubs_epi16(m1_ac, one_b);
      const __m128i m0 = _mm_srli_epi16(_mm_add_epi16(m0_acbd, two_w), 2);
      const __m128i m1 = _mm_srli_epi16(_mm_add_epi16(m1_acbd, two_w), 2);

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride << 1;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static INLINE void lowbd_blend_a64_d16_mask_subw1_subh0_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i zeros = _mm_setzero_si128();
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + 2 * j);
      const __m128i m_i01 = xx_loadu_128(mask + 2 * j + 16);
      const __m128i m0_ac = _mm_maddubs_epi16(m_i00, one_b);
      const __m128i m1_ac = _mm_maddubs_epi16(m_i01, one_b);
      const __m128i m0 = _mm_avg_epu16(m0_ac, zeros);
      const __m128i m1 = _mm_avg_epu16(m1_ac, zeros);

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static INLINE void lowbd_blend_a64_d16_mask_subw0_subh1_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i zeros = _mm_setzero_si128();
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + j);
      const __m128i m_i10 = xx_loadu_128(mask + mask_stride + j);

      const __m128i m_ac = _mm_avg_epu8(_mm_adds_epu8(m_i00, m_i10), zeros);
      const __m128i m0 = _mm_cvtepu8_epi16(m_ac);
      const __m128i m1 = _mm_cvtepu8_epi16(_mm_srli_si128(m_ac, 8));

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride << 1;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

void aom_lowbd_blend_a64_d16_mask_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;

  const int round_offset =
      ((1 << (round_bits + bd)) + (1 << (round_bits + bd - 1)) -
       (1 << (round_bits - 1)))
      << AOM_BLEND_A64_ROUND_BITS;

  const int shift = round_bits + AOM_BLEND_A64_ROUND_BITS;
  assert(IMPLIES((void *)src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES((void *)src1 == dst, src1_stride == dst_stride));

  assert(h >= 4);
  assert(w >= 4);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  const __m128i v_round_offset = _mm_set1_epi32(round_offset);

  if (subw == 0 && subh == 0) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw0_subh0_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw0_subh0_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }

  } else if (subw == 1 && subh == 1) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw1_subh1_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw1_subh1_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  } else if (subw == 1 && subh == 0) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw1_subh0_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw1_subh0_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw1_subh0_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  } else {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw0_subh1_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw0_subh1_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw0_subh1_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  }
}
