/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

static INLINE void accumulate_sse_sum(__m256i regx_sum, __m256i regx2_sum,
                                      int *x_sum, int *x2_sum) {
  __m256i sum_buffer, sse_buffer;
  __m128i out_buffer;
  sum_buffer = _mm256_permute2f128_si256(regx_sum, regx_sum, 1);
  regx_sum = _mm256_add_epi32(sum_buffer, regx_sum);
  regx_sum = _mm256_hadd_epi32(regx_sum, regx_sum);
  regx_sum = _mm256_hadd_epi32(regx_sum, regx_sum);
  sse_buffer = _mm256_permute2f128_si256(regx2_sum, regx2_sum, 1);
  regx2_sum = _mm256_add_epi32(sse_buffer, regx2_sum);
  regx2_sum = _mm256_hadd_epi32(regx2_sum, regx2_sum);
  regx2_sum = _mm256_hadd_epi32(regx2_sum, regx2_sum);
  out_buffer = _mm256_castsi256_si128(regx_sum);
  *x_sum = _mm_cvtsi128_si32(out_buffer);
  out_buffer = _mm256_castsi256_si128(regx2_sum);
  *x2_sum = _mm_cvtsi128_si32(out_buffer);
}

static INLINE void sse_sum_wd4_avx2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int *x2_sum) {
  __m128i row1, row2, row3;
  __m256i regx_sum, regx2_sum, load_pixels, sum_buffer, sse_buffer;
  const int16_t *data_tmp = data;
  __m256i one = _mm256_set1_epi16(1);
  regx_sum = _mm256_setzero_si256();
  regx2_sum = regx_sum;
  for (int j = 0; j<bh>> 2; ++j) {
    row1 = _mm_loadl_epi64((__m128i const *)(data_tmp));
    row2 = _mm_loadl_epi64((__m128i const *)(data_tmp + stride));
    row1 = _mm_unpacklo_epi64(row1, row2);
    row2 = _mm_loadl_epi64((__m128i const *)(data_tmp + 2 * stride));
    row3 = _mm_loadl_epi64((__m128i const *)(data_tmp + 3 * stride));
    row2 = _mm_unpacklo_epi64(row2, row3);
    load_pixels =
        _mm256_insertf128_si256(_mm256_castsi128_si256(row1), row2, 1);
    sum_buffer = _mm256_madd_epi16(load_pixels, one);
    sse_buffer = _mm256_madd_epi16(load_pixels, load_pixels);
    regx_sum = _mm256_add_epi32(sum_buffer, regx_sum);
    regx2_sum = _mm256_add_epi32(sse_buffer, regx2_sum);
    data_tmp += 4 * stride;
  }
  accumulate_sse_sum(regx_sum, regx2_sum, x_sum, x2_sum);
}

static INLINE void sse_sum_wd8_avx2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int *x2_sum) {
  __m128i load_128bit, load_next_128bit;
  __m256i regx_sum, regx2_sum, load_pixels, sum_buffer, sse_buffer;
  const int16_t *data_tmp = data;
  __m256i one = _mm256_set1_epi16(1);
  regx_sum = _mm256_setzero_si256();
  regx2_sum = regx_sum;
  for (int j = 0; j<bh>> 1; ++j) {
    load_128bit = _mm_load_si128((__m128i const *)(data_tmp));
    load_next_128bit = _mm_load_si128((__m128i const *)(data_tmp + stride));
    load_pixels = _mm256_insertf128_si256(_mm256_castsi128_si256(load_128bit),
                                          load_next_128bit, 1);
    sum_buffer = _mm256_madd_epi16(load_pixels, one);
    sse_buffer = _mm256_madd_epi16(load_pixels, load_pixels);
    regx_sum = _mm256_add_epi32(sum_buffer, regx_sum);
    regx2_sum = _mm256_add_epi32(sse_buffer, regx2_sum);
    data_tmp += 2 * stride;
  }
  accumulate_sse_sum(regx_sum, regx2_sum, x_sum, x2_sum);
}

static INLINE void sse_sum_wd16_avx2(const int16_t *data, int stride, int bh,
                                     int *x_sum, int *x2_sum, int loop_count) {
  __m256i regx_sum, regx2_sum, load_pixels, sum_buffer, sse_buffer;
  const int16_t *data_tmp = data;
  __m256i one = _mm256_set1_epi16(1);
  regx_sum = _mm256_setzero_si256();
  regx2_sum = regx_sum;
  for (int i = 0; i < loop_count; ++i) {
    data_tmp = data + 16 * i;
    for (int j = 0; j < bh; ++j) {
      load_pixels = _mm256_lddqu_si256((__m256i const *)(data_tmp));
      sum_buffer = _mm256_madd_epi16(load_pixels, one);
      sse_buffer = _mm256_madd_epi16(load_pixels, load_pixels);
      regx_sum = _mm256_add_epi32(sum_buffer, regx_sum);
      regx2_sum = _mm256_add_epi32(sse_buffer, regx2_sum);
      data_tmp += stride;
    }
  }
  accumulate_sse_sum(regx_sum, regx2_sum, x_sum, x2_sum);
}

void aom_get_blk_sse_sum_avx2(const int16_t *data, int stride, int bw, int bh,
                              int *x_sum, int *x2_sum) {
  *x_sum = 0;
  *x2_sum = 0;

  if ((bh & 3) == 0) {
    switch (bw) {
      case 4: sse_sum_wd4_avx2(data, stride, bh, x_sum, x2_sum); break;

      case 8: sse_sum_wd8_avx2(data, stride, bh, x_sum, x2_sum); break;

      case 16:
      case 32:
      case 64:
      case 128:
        sse_sum_wd16_avx2(data, stride, bh, x_sum, x2_sum, bw >> 4);
        break;

      default: aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
    }
  } else {
    aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
  }
}
