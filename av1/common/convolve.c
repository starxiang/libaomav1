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

#include <assert.h>
#include <string.h>

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "av1/common/blockd.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/resize.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

#define MAX_BLOCK_WIDTH (MAX_SB_SIZE)
#define MAX_BLOCK_HEIGHT (MAX_SB_SIZE)
#define MAX_STEP (32)

void av1_convolve_horiz_rs_c(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const int16_t *x_filters, const int x0_qn,
                             const int x_step_qn) {
  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_qn >> RS_SCALE_SUBPEL_BITS];
      const int x_filter_idx =
          (x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
      assert(x_filter_idx <= RS_SUBPEL_MASK);
      const int16_t *const x_filter =
          &x_filters[x_filter_idx * UPSCALE_NORMATIVE_TAPS];
      int sum = 0;
      for (int k = 0; k < UPSCALE_NORMATIVE_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}
// TODO(yaowu: remove "const" from pass-by-value params in this and other funcs)
void av1_highbd_convolve_horiz_rs_c(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const int16_t *x_filters, const int x0_qn,
                                    const int x_step_qn, int bd) {
  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_qn = x0_qn;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_qn >> RS_SCALE_SUBPEL_BITS];
      const int x_filter_idx =
          (x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
      assert(x_filter_idx <= RS_SUBPEL_MASK);
      const int16_t *const x_filter =
          &x_filters[x_filter_idx * UPSCALE_NORMATIVE_TAPS];
      int sum = 0;
      for (int k = 0; k < UPSCALE_NORMATIVE_TAPS; ++k)
        sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_qn += x_step_qn;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void av1_convolve_rounding_c(const int32_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h, int bits) {
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * dst_stride + c] =
          clip_pixel(ROUND_POWER_OF_TWO(src[r * src_stride + c], bits));
    }
  }
}

/* Note: For notes on hardware implementations, including the required
   bit widths for various intermediate values, see the comments above
   av1_warp_affine_c.
*/
void av1_convolve_2d_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                       int dst_stride0, int w, int h,
                       InterpFilterParams *filter_params_x,
                       InterpFilterParams *filter_params_y,
                       const int subpel_x_q4, const int subpel_y_q4,
                       ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_convolve_y_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                      int dst_stride0, int w, int h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int subpel_x_q4, const int subpel_y_q4,
                      ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  assert(bits >= 0);

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_convolve_x_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                      int dst_stride0, int w, int h,
                      InterpFilterParams *filter_params_x,
                      InterpFilterParams *filter_params_y,
                      const int subpel_x_q4, const int subpel_y_q4,
                      ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  assert(bits >= 0);

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_convolve_2d_copy_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                            int dst_stride0, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_convolve_2d_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                          int dst_stride, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(res, bits));
    }
  }
}

void av1_convolve_y_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  const int fo_vert = filter_params_y->taps / 2 - 1;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  assert(conv_params->round_0 <= FILTER_BITS);
  assert(((conv_params->round_0 + conv_params->round_1) <= (FILTER_BITS + 1)) ||
         ((conv_params->round_0 + conv_params->round_1) == (2 * FILTER_BITS)));

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      dst[y * dst_stride + x] =
          clip_pixel(ROUND_POWER_OF_TWO(res, FILTER_BITS));
    }
  }
}

void av1_convolve_x_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                         int dst_stride, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)conv_params;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = ROUND_POWER_OF_TWO(res, conv_params->round_0);
      dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(res, bits));
    }
  }
}

void av1_convolve_2d_copy_sr_c(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, int w, int h,
                               InterpFilterParams *filter_params_x,
                               InterpFilterParams *filter_params_y,
                               const int subpel_x_q4, const int subpel_y_q4,
                               ConvolveParams *conv_params) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dst[y * dst_stride + x] = src[y * src_stride + x];
    }
  }
}

void av1_jnt_convolve_2d_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                           int dst_stride0, int w, int h,
                           InterpFilterParams *filter_params_x,
                           InterpFilterParams *filter_params_y,
                           const int subpel_x_q4, const int subpel_y_q4,
                           ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_jnt_convolve_y_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                          int dst_stride0, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_jnt_convolve_x_c(const uint8_t *src, int src_stride, uint8_t *dst0,
                          int dst_stride0, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_jnt_convolve_2d_copy_c(const uint8_t *src, int src_stride,
                                uint8_t *dst0, int dst_stride0, int w, int h,
                                InterpFilterParams *filter_params_x,
                                InterpFilterParams *filter_params_y,
                                const int subpel_x_q4, const int subpel_y_q4,
                                ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_convolve_2d_scale_c(const uint8_t *src, int src_stride,
                             CONV_BUF_TYPE *dst, int dst_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int x_step_qn,
                             const int subpel_y_qn, const int y_step_qn,
                             ConvolveParams *conv_params) {
  int16_t im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bd = 8;

  // horizontal filter
  const uint8_t *src_horiz = src - fo_vert * src_stride;
  for (int y = 0; y < im_h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
      const uint8_t *const src_x = &src_horiz[(x_qn >> SCALE_SUBPEL_BITS)];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_x, x_filter_idx);
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_x[k - fo_horiz];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
    src_horiz += src_stride;
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
      const int16_t *src_y = &src_vert[(y_qn >> SCALE_SUBPEL_BITS) * im_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_y, y_filter_idx);
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_y[(k - fo_vert) * im_stride];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
    src_vert++;
  }
}

static void convolve_2d_scale_wrapper(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w,
    int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int x_step_qn, const int subpel_y_qn, const int y_step_qn,
    ConvolveParams *conv_params) {
  if (conv_params->is_compound) {
    assert(conv_params->dst != NULL);
    av1_convolve_2d_scale(src, src_stride, conv_params->dst,
                          conv_params->dst_stride, w, h, filter_params_x,
                          filter_params_y, subpel_x_qn, x_step_qn, subpel_y_qn,
                          y_step_qn, conv_params);
  } else {
    CONV_BUF_TYPE tmp_dst[MAX_SB_SIZE * MAX_SB_SIZE];
    int tmp_dst_stride = MAX_SB_SIZE;
    av1_convolve_2d_scale(src, src_stride, tmp_dst, tmp_dst_stride, w, h,
                          filter_params_x, filter_params_y, subpel_x_qn,
                          x_step_qn, subpel_y_qn, y_step_qn, conv_params);
    const int rbits =
        2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
    av1_convolve_rounding(tmp_dst, tmp_dst_stride, dst, dst_stride, w, h,
                          rbits);
  }
}

void av1_convolve_2d_facade(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilters interp_filters, const int subpel_x_q4,
                            int x_step_q4, const int subpel_y_q4, int y_step_q4,
                            int scaled, ConvolveParams *conv_params,
                            const struct scale_factors *sf) {
  (void)x_step_q4;
  (void)y_step_q4;
  (void)dst;
  (void)dst_stride;

  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  if (scaled)
    convolve_2d_scale_wrapper(src, src_stride, dst, dst_stride, w, h,
                              &filter_params_x, &filter_params_y, subpel_x_q4,
                              x_step_q4, subpel_y_q4, y_step_q4, conv_params);
  else
    sf->convolve[subpel_x_q4 != 0][subpel_y_q4 != 0][conv_params->is_compound](
        src, src_stride, dst, dst_stride, w, h, &filter_params_x,
        &filter_params_y, subpel_x_q4, subpel_y_q4, conv_params);
}

void av1_highbd_convolve_rounding_c(const int32_t *src, int src_stride,
                                    uint8_t *dst8, int dst_stride, int w, int h,
                                    int bits, int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      dst[r * dst_stride + c] = clip_pixel_highbd(
          ROUND_POWER_OF_TWO(src[r * src_stride + c], bits), bd);
    }
  }
}

void av1_highbd_convolve_2d_c(const uint16_t *src, int src_stride,
                              uint16_t *dst0, int dst_stride0, int w, int h,
                              InterpFilterParams *filter_params_x,
                              InterpFilterParams *filter_params_y,
                              const int subpel_x_q4, const int subpel_y_q4,
                              ConvolveParams *conv_params, int bd) {
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      (void)bd;
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_highbd_convolve_2d_copy_c(const uint16_t *src, int src_stride,
                                   uint16_t *dst0, int dst_stride0, int w,
                                   int h, InterpFilterParams *filter_params_x,
                                   InterpFilterParams *filter_params_y,
                                   const int subpel_x_q4, const int subpel_y_q4,
                                   ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_highbd_convolve_x_c(const uint16_t *src, int src_stride,
                             uint16_t *dst0, int dst_stride0, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  assert(bits >= 0);

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_highbd_convolve_y_c(const uint16_t *src, int src_stride,
                             uint16_t *dst0, int dst_stride0, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  assert(bits >= 0);

  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->do_average) {
        int32_t tmp = dst[y * dst_stride + x];
        tmp += res;
        dst[y * dst_stride + x] = tmp >> 1;
      } else {
        dst[y * dst_stride + x] = res;
      }
    }
  }
}

void av1_highbd_convolve_2d_copy_sr_c(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_q4,
    const int subpel_y_q4, ConvolveParams *conv_params, int bd) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;
  (void)bd;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dst[y * dst_stride + x] = src[y * src_stride + x];
    }
  }
}

void av1_highbd_convolve_x_sr_c(const uint16_t *src, int src_stride,
                                uint16_t *dst, int dst_stride, int w, int h,
                                InterpFilterParams *filter_params_x,
                                InterpFilterParams *filter_params_y,
                                const int subpel_x_q4, const int subpel_y_q4,
                                ConvolveParams *conv_params, int bd) {
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_y;
  (void)subpel_y_q4;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = ROUND_POWER_OF_TWO(res, conv_params->round_0);
      dst[y * dst_stride + x] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(res, bits), bd);
    }
  }
}

void av1_highbd_convolve_y_sr_c(const uint16_t *src, int src_stride,
                                uint16_t *dst, int dst_stride, int w, int h,
                                InterpFilterParams *filter_params_x,
                                InterpFilterParams *filter_params_y,
                                const int subpel_x_q4, const int subpel_y_q4,
                                ConvolveParams *conv_params, int bd) {
  const int fo_vert = filter_params_y->taps / 2 - 1;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  assert(conv_params->round_0 <= FILTER_BITS);
  assert(((conv_params->round_0 + conv_params->round_1) <= (FILTER_BITS + 1)) ||
         ((conv_params->round_0 + conv_params->round_1) == (2 * FILTER_BITS)));
  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      dst[y * dst_stride + x] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(res, FILTER_BITS), bd);
    }
  }
}

void av1_highbd_convolve_2d_sr_c(const uint16_t *src, int src_stride,
                                 uint16_t *dst, int dst_stride, int w, int h,
                                 InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < im_h; ++y) {
    for (int x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      dst[y * dst_stride + x] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(res, bits), bd);
    }
  }
}

void av1_highbd_jnt_convolve_2d_c(const uint16_t *src, int src_stride,
                                  uint16_t *dst0, int dst_stride0, int w, int h,
                                  InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params, int bd) {
  int x, y, k;
  int16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  (void)dst0;
  (void)dst_stride0;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (y = 0; y < im_h; ++y) {
    for (x = 0; x < w; ++x) {
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_horiz[y * src_stride + x - fo_horiz + k];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      (void)bd;
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_vert[(y - fo_vert + k) * im_stride + x];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_highbd_jnt_convolve_x_c(const uint16_t *src, int src_stride,
                                 uint16_t *dst0, int dst_stride0, int w, int h,
                                 InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_1;
  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  assert(bits >= 0);
  // horizontal filter
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_x->taps; ++k) {
        res += x_filter[k] * src[y * src_stride + x - fo_horiz + k];
      }
      res = (1 << bits) * ROUND_POWER_OF_TWO(res, conv_params->round_0);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_highbd_jnt_convolve_y_c(const uint16_t *src, int src_stride,
                                 uint16_t *dst0, int dst_stride0, int w, int h,
                                 InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int bits = FILTER_BITS - conv_params->round_0;
  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  assert(bits >= 0);
  // vertical filter
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = 0;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        res += y_filter[k] * src[(y - fo_vert + k) * src_stride + x];
      }
      res *= (1 << bits);
      res = ROUND_POWER_OF_TWO(res, conv_params->round_1);
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_highbd_jnt_convolve_2d_copy_c(
    const uint16_t *src, int src_stride, uint16_t *dst0, int dst_stride0, int w,
    int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_q4,
    const int subpel_y_q4, ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;

  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;
  (void)bd;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      CONV_BUF_TYPE res = src[y * src_stride + x] << bits;
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
  }
}

void av1_highbd_convolve_2d_scale_c(const uint16_t *src, int src_stride,
                                    CONV_BUF_TYPE *dst, int dst_stride, int w,
                                    int h, InterpFilterParams *filter_params_x,
                                    InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn, const int x_step_qn,
                                    const int subpel_y_qn, const int y_step_qn,
                                    ConvolveParams *conv_params, int bd) {
  int16_t im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  int im_stride = w;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;

  // horizontal filter
  const uint16_t *src_horiz = src - fo_vert * src_stride;
  for (int y = 0; y < im_h; ++y) {
    int x_qn = subpel_x_qn;
    for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
      const uint16_t *const src_x = &src_horiz[(x_qn >> SCALE_SUBPEL_BITS)];
      const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(x_filter_idx < SUBPEL_SHIFTS);
      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_x, x_filter_idx);
      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < filter_params_x->taps; ++k) {
        sum += x_filter[k] * src_x[k - fo_horiz];
      }
      assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
      im_block[y * im_stride + x] =
          (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
    }
    src_horiz += src_stride;
  }

  // vertical filter
  int16_t *src_vert = im_block + fo_vert * im_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  for (int x = 0; x < w; ++x) {
    int y_qn = subpel_y_qn;
    for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
      const int16_t *src_y = &src_vert[(y_qn >> SCALE_SUBPEL_BITS) * im_stride];
      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      assert(y_filter_idx < SUBPEL_SHIFTS);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(*filter_params_y, y_filter_idx);
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < filter_params_y->taps; ++k) {
        sum += y_filter[k] * src_y[(k - fo_vert) * im_stride];
      }
      assert(0 <= sum && sum < (1 << (offset_bits + 2)));
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) -
                          ((1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1)));
      if (conv_params->use_jnt_comp_avg) {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
          dst[y * dst_stride + x] = tmp >> DIST_PRECISION_BITS;
        } else {
          dst[y * dst_stride + x] = res;
        }
      } else {
        if (conv_params->do_average) {
          int32_t tmp = dst[y * dst_stride + x];
          tmp += res;
          dst[y * dst_stride + x] = tmp >> 1;
        } else {
          dst[y * dst_stride + x] = res;
        }
      }
    }
    src_vert++;
  }
}

void av1_highbd_convolve_2d_facade(const uint8_t *src8, int src_stride,
                                   uint8_t *dst8, int dst_stride, int w, int h,
                                   InterpFilters interp_filters,
                                   const int subpel_x_q4, int x_step_q4,
                                   const int subpel_y_q4, int y_step_q4,
                                   int scaled, ConvolveParams *conv_params,
                                   const struct scale_factors *sf, int bd) {
  (void)x_step_q4;
  (void)y_step_q4;
  (void)dst_stride;

  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  InterpFilterParams filter_params_x, filter_params_y;
#if CONFIG_SHORT_FILTER
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y, w, h);
#else
  av1_get_convolve_filter_params(interp_filters, &filter_params_x,
                                 &filter_params_y);
#endif

  if (scaled) {
    if (conv_params->is_compound) {
      av1_highbd_convolve_2d_scale(
          src, src_stride, conv_params->dst, conv_params->dst_stride, w, h,
          &filter_params_x, &filter_params_y, subpel_x_q4, x_step_q4,
          subpel_y_q4, y_step_q4, conv_params, bd);
    } else {
      CONV_BUF_TYPE tmp_dst[MAX_SB_SIZE * MAX_SB_SIZE];
      int tmp_dst_stride = MAX_SB_SIZE;
      av1_highbd_convolve_2d_scale(src, src_stride, tmp_dst, tmp_dst_stride, w,
                                   h, &filter_params_x, &filter_params_y,
                                   subpel_x_q4, x_step_q4, subpel_y_q4,
                                   y_step_q4, conv_params, bd);

      // 0-bit rounding just to convert from int32 to uint16
      const int rbits =
          2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
      assert(rbits >= 0);
      av1_highbd_convolve_rounding(tmp_dst, tmp_dst_stride, dst8, dst_stride, w,
                                   h, rbits, bd);
    }
  } else {
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

    sf->highbd_convolve[subpel_x_q4 != 0][subpel_y_q4 !=
                                          0][conv_params->is_compound](
        src, src_stride, dst, dst_stride, w, h, &filter_params_x,
        &filter_params_y, subpel_x_q4, subpel_y_q4, conv_params, bd);
  }
}

static INLINE int horz_scalar_product(const uint8_t *a, const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k] * b[k];
  return sum;
}

static INLINE int vert_scalar_product(const uint8_t *a, ptrdiff_t a_stride,
                                      const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k * a_stride] * b[k];
  return sum;
}

static INLINE int highbd_horz_scalar_product(const uint16_t *a,
                                             const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k] * b[k];
  return sum;
}

static INLINE int highbd_vert_scalar_product(const uint16_t *a,
                                             ptrdiff_t a_stride,
                                             const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k * a_stride] * b[k];
  return sum;
}

static const InterpKernel *get_filter_base(const int16_t *filter) {
  // NOTE: This assumes that the filter table is 256-byte aligned.
  // TODO(agrange) Modify to make independent of table alignment.
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

static void convolve_add_src_horiz_hip(const uint8_t *src, ptrdiff_t src_stride,
                                       uint16_t *dst, ptrdiff_t dst_stride,
                                       const InterpKernel *x_filters, int x0_q4,
                                       int x_step_q4, int w, int h,
                                       int round0_bits) {
  const int bd = 8;
  src -= SUBPEL_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      const int rounding = ((int)src_x[SUBPEL_TAPS / 2 - 1] << FILTER_BITS) +
                           (1 << (bd + FILTER_BITS - 1));
      const int sum = horz_scalar_product(src_x, x_filter) + rounding;
      dst[x] = (uint16_t)clamp(ROUND_POWER_OF_TWO(sum, round0_bits), 0,
                               WIENER_CLAMP_LIMIT(bd) - 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_add_src_vert_hip(const uint16_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *y_filters, int y0_q4,
                                      int y_step_q4, int w, int h,
                                      int round1_bits) {
  const int bd = 8;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (int x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (int y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      const int rounding =
          ((int)src_y[(SUBPEL_TAPS / 2 - 1) * src_stride] << FILTER_BITS) -
          (1 << (bd + round1_bits - 1));
      const int sum =
          highbd_vert_scalar_product(src_y, src_stride, y_filter) + rounding;
      dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, round1_bits));
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

void av1_wiener_convolve_add_src_hip_c(const uint8_t *src, ptrdiff_t src_stride,
                                       uint8_t *dst, ptrdiff_t dst_stride,
                                       const int16_t *filter_x, int x_step_q4,
                                       const int16_t *filter_y, int y_step_q4,
                                       int w, int h,
                                       ConvolveParams *conv_params) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  uint16_t temp[WIENER_MAX_EXT_SIZE * MAX_SB_SIZE];
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  convolve_add_src_horiz_hip(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                             src_stride, temp, MAX_SB_SIZE, filters_x, x0_q4,
                             x_step_q4, w, intermediate_height,
                             conv_params->round_0);
  convolve_add_src_vert_hip(temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1),
                            MAX_SB_SIZE, dst, dst_stride, filters_y, y0_q4,
                            y_step_q4, w, h, conv_params->round_1);
}

static void highbd_convolve_add_src_horiz_hip(
    const uint8_t *src8, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const InterpKernel *x_filters, int x0_q4,
    int x_step_q4, int w, int h, int round0_bits, int bd) {
  const int extraprec_clamp_limit = WIENER_CLAMP_LIMIT(bd);
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      const int rounding = ((int)src_x[SUBPEL_TAPS / 2 - 1] << FILTER_BITS) +
                           (1 << (bd + FILTER_BITS - 1));
      const int sum = highbd_horz_scalar_product(src_x, x_filter) + rounding;
      dst[x] = (uint16_t)clamp(ROUND_POWER_OF_TWO(sum, round0_bits), 0,
                               extraprec_clamp_limit - 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_add_src_vert_hip(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const InterpKernel *y_filters, int y0_q4,
    int y_step_q4, int w, int h, int round1_bits, int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (int x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (int y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      const int rounding =
          ((int)src_y[(SUBPEL_TAPS / 2 - 1) * src_stride] << FILTER_BITS) -
          (1 << (bd + round1_bits - 1));
      const int sum =
          highbd_vert_scalar_product(src_y, src_stride, y_filter) + rounding;
      dst[y * dst_stride] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, round1_bits), bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

void av1_highbd_wiener_convolve_add_src_hip_c(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,
    const int16_t *filter_y, int y_step_q4, int w, int h,
    ConvolveParams *conv_params, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint16_t temp[WIENER_MAX_EXT_SIZE * MAX_SB_SIZE];
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  highbd_convolve_add_src_horiz_hip(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                                    src_stride, temp, MAX_SB_SIZE, filters_x,
                                    x0_q4, x_step_q4, w, intermediate_height,
                                    conv_params->round_0, bd);
  highbd_convolve_add_src_vert_hip(
      temp + MAX_SB_SIZE * (SUBPEL_TAPS / 2 - 1), MAX_SB_SIZE, dst, dst_stride,
      filters_y, y0_q4, y_step_q4, w, h, conv_params->round_1, bd);
}
