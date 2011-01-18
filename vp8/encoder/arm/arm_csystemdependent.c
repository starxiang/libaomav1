/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "vpx_ports/arm.h"
#include "variance.h"
#include "onyx_int.h"

extern void (*vp8_yv12_copy_partial_frame_ptr)(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction);
extern void vp8_yv12_copy_partial_frame(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction);
extern void vpxyv12_copy_partial_frame_neon(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc, int Fraction);

extern unsigned int vp8_sub_pixel_variance16x16_neon_func
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
);
unsigned int vp8_sub_pixel_variance16x16_neon
(
    const unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    const unsigned char *dst_ptr,
    int dst_pixels_per_line,
    unsigned int *sse
)
{
  if (xoffset == 4 && yoffset == 0)
      return vp8_variance_halfpixvar16x16_h_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else if (xoffset == 0 && yoffset == 4)
      return vp8_variance_halfpixvar16x16_v_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else if (xoffset == 4 && yoffset == 4)
      return vp8_variance_halfpixvar16x16_hv_neon(src_ptr, src_pixels_per_line, dst_ptr, dst_pixels_per_line, sse);
  else
      return vp8_sub_pixel_variance16x16_neon_func(src_ptr, src_pixels_per_line, xoffset, yoffset, dst_ptr, dst_pixels_per_line, sse);
}

void vp8_arch_arm_encoder_init(VP8_COMP *cpi)
{
#if CONFIG_RUNTIME_CPU_DETECT
    int flags = cpi->common.rtcd.flags;
    int has_edsp = flags & HAS_EDSP;
    int has_media = flags & HAS_MEDIA;
    int has_neon = flags & HAS_NEON;

#if HAVE_ARMV6
    if (has_media)
    {
        /*cpi->rtcd.variance.sad16x16              = vp8_sad16x16_c;
        cpi->rtcd.variance.sad16x8               = vp8_sad16x8_c;
        cpi->rtcd.variance.sad8x16               = vp8_sad8x16_c;
        cpi->rtcd.variance.sad8x8                = vp8_sad8x8_c;
        cpi->rtcd.variance.sad4x4                = vp8_sad4x4_c;*/

        /*cpi->rtcd.variance.var4x4                = vp8_variance4x4_c;
        cpi->rtcd.variance.var8x8                = vp8_variance8x8_c;
        cpi->rtcd.variance.var8x16               = vp8_variance8x16_c;
        cpi->rtcd.variance.var16x8               = vp8_variance16x8_c;
        cpi->rtcd.variance.var16x16              = vp8_variance16x16_c;*/

        /*cpi->rtcd.variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;
        cpi->rtcd.variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_c;
        cpi->rtcd.variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
        cpi->rtcd.variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;
        cpi->rtcd.variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_c;*/

        /*cpi->rtcd.variance.mse16x16              = vp8_mse16x16_c;
        cpi->rtcd.variance.getmbss               = vp8_get_mb_ss_c;*/

        /*cpi->rtcd.variance.get16x16prederror     = vp8_get16x16pred_error_c;
        cpi->rtcd.variance.get8x8var             = vp8_get8x8var_c;
        cpi->rtcd.variance.get16x16var           = vp8_get16x16var_c;;
        cpi->rtcd.variance.get4x4sse_cs          = vp8_get4x4sse_cs_c;*/

        /*cpi->rtcd.fdct.short4x4                  = vp8_short_fdct4x4_c;
        cpi->rtcd.fdct.short8x4                  = vp8_short_fdct8x4_c;
        cpi->rtcd.fdct.fast4x4                   = vp8_fast_fdct4x4_c;
        cpi->rtcd.fdct.fast8x4                   = vp8_fast_fdct8x4_c;*/
        cpi->rtcd.fdct.walsh_short4x4            = vp8_short_walsh4x4_armv6;

        /*cpi->rtcd.encodemb.berr                  = vp8_block_error_c;
        cpi->rtcd.encodemb.mberr                 = vp8_mbblock_error_c;
        cpi->rtcd.encodemb.mbuverr               = vp8_mbuverror_c;
        cpi->rtcd.encodemb.subb                  = vp8_subtract_b_c;
        cpi->rtcd.encodemb.submby                = vp8_subtract_mby_c;
        cpi->rtcd.encodemb.submbuv               = vp8_subtract_mbuv_c;*/

        /*cpi->rtcd.quantize.quantb                = vp8_regular_quantize_b;
        cpi->rtcd.quantize.fastquantb            = vp8_fast_quantize_b_c;*/
    }
#endif

#if HAVE_ARMV7
    if (has_neon)
    {
        cpi->rtcd.variance.sad16x16              = vp8_sad16x16_neon;
        cpi->rtcd.variance.sad16x8               = vp8_sad16x8_neon;
        cpi->rtcd.variance.sad8x16               = vp8_sad8x16_neon;
        cpi->rtcd.variance.sad8x8                = vp8_sad8x8_neon;
        cpi->rtcd.variance.sad4x4                = vp8_sad4x4_neon;

        /*cpi->rtcd.variance.var4x4                = vp8_variance4x4_c;*/
        cpi->rtcd.variance.var8x8                = vp8_variance8x8_neon;
        cpi->rtcd.variance.var8x16               = vp8_variance8x16_neon;
        cpi->rtcd.variance.var16x8               = vp8_variance16x8_neon;
        cpi->rtcd.variance.var16x16              = vp8_variance16x16_neon;

        /*cpi->rtcd.variance.subpixvar4x4          = vp8_sub_pixel_variance4x4_c;*/
        cpi->rtcd.variance.subpixvar8x8          = vp8_sub_pixel_variance8x8_neon;
        /*cpi->rtcd.variance.subpixvar8x16         = vp8_sub_pixel_variance8x16_c;
        cpi->rtcd.variance.subpixvar16x8         = vp8_sub_pixel_variance16x8_c;*/
        cpi->rtcd.variance.subpixvar16x16        = vp8_sub_pixel_variance16x16_neon;
        cpi->rtcd.variance.halfpixvar16x16_h     = vp8_variance_halfpixvar16x16_h_neon;
        cpi->rtcd.variance.halfpixvar16x16_v     = vp8_variance_halfpixvar16x16_v_neon;
        cpi->rtcd.variance.halfpixvar16x16_hv    = vp8_variance_halfpixvar16x16_hv_neon;

        cpi->rtcd.variance.mse16x16              = vp8_mse16x16_neon;
        /*cpi->rtcd.variance.getmbss               = vp8_get_mb_ss_c;*/

        cpi->rtcd.variance.get16x16prederror     = vp8_get16x16pred_error_neon;
        /*cpi->rtcd.variance.get8x8var             = vp8_get8x8var_c;
        cpi->rtcd.variance.get16x16var           = vp8_get16x16var_c;*/
        cpi->rtcd.variance.get4x4sse_cs          = vp8_get4x4sse_cs_neon;

        cpi->rtcd.fdct.short4x4                  = vp8_short_fdct4x4_neon;
        cpi->rtcd.fdct.short8x4                  = vp8_short_fdct8x4_neon;
        cpi->rtcd.fdct.fast4x4                   = vp8_fast_fdct4x4_neon;
        cpi->rtcd.fdct.fast8x4                   = vp8_fast_fdct8x4_neon;
        cpi->rtcd.fdct.walsh_short4x4            = vp8_short_walsh4x4_neon;

        /*cpi->rtcd.encodemb.berr                  = vp8_block_error_c;
        cpi->rtcd.encodemb.mberr                 = vp8_mbblock_error_c;
        cpi->rtcd.encodemb.mbuverr               = vp8_mbuverror_c;*/
        cpi->rtcd.encodemb.subb                  = vp8_subtract_b_neon;
        cpi->rtcd.encodemb.submby                = vp8_subtract_mby_neon;
        cpi->rtcd.encodemb.submbuv               = vp8_subtract_mbuv_neon;

        /*cpi->rtcd.quantize.quantb                = vp8_regular_quantize_b;
        cpi->rtcd.quantize.fastquantb            = vp8_fast_quantize_b_c;*/
        /* The neon quantizer has not been updated to match the new exact
         * quantizer introduced in commit e04e2935
         */
        /*cpi->rtcd.quantize.fastquantb            = vp8_fast_quantize_b_neon;*/
    }
#endif

#if HAVE_ARMV7
#if CONFIG_RUNTIME_CPU_DETECT
    if (has_neon)
#endif
    {
        vp8_yv12_copy_partial_frame_ptr = vpxyv12_copy_partial_frame_neon;
    }
#endif
#endif
}
