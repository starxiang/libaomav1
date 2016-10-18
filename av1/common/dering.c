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

// clang-format off

#include <string.h>
#include <math.h>

#include "./aom_scale_rtcd.h"
#include "aom/aom_integer.h"
#include "av1/common/dering.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/common/od_dering.h"

int compute_level_from_index(int global_level, int gi) {
  static const int dering_gains[DERING_REFINEMENT_LEVELS] = { 0, 11, 16, 22 };
  int level;
  if (global_level == 0) return 0;
  level = (global_level * dering_gains[gi] + 8) >> 4;
  return clamp(level, gi, MAX_DERING_LEVEL - 1);
}

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col) {
  int r, c;
  int maxc, maxr;
  int skip = 1;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;
  if (maxr > MAX_MIB_SIZE) maxr = MAX_MIB_SIZE;
  if (maxc > MAX_MIB_SIZE) maxc = MAX_MIB_SIZE;
  for (r = 0; r < maxr; r++) {
    for (c = 0; c < maxc; c++) {
      skip = skip &&
             cm->mi_grid_visible[(mi_row + r) * cm->mi_stride + mi_col + c]
                 ->mbmi.skip;
    }
  }
  return skip;
}

int sb_all_skip_out(const AV1_COMMON *const cm, int mi_row, int mi_col,
    unsigned char (*bskip)[2], int *count_ptr) {
  int r, c;
  int maxc, maxr;
  int skip = 1;
  MODE_INFO **grid;
  int count=0;
  grid = cm->mi_grid_visible;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;
  if (maxr > MAX_MIB_SIZE) maxr = MAX_MIB_SIZE;
  if (maxc > MAX_MIB_SIZE) maxc = MAX_MIB_SIZE;
  for (r = 0; r < maxr; r++) {
    MODE_INFO **grid_row;
    grid_row = &grid[(mi_row + r) * cm->mi_stride + mi_col];
    for (c = 0; c < maxc; c++) {
      if (!grid_row[c]->mbmi.skip) {
        skip = 0;
        bskip[count][0] = r;
        bskip[count][1] = c;
        count++;
      }
    }
  }
  *count_ptr = count;
  return skip;
}

static INLINE void copy_8x8_16_8bit(uint8_t *dst, int dstride, int16_t *src, int sstride) {
  int i, j;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      dst[i * dstride + j] = src[i * sstride + j];
}

static INLINE void copy_4x4_16_8bit(uint8_t *dst, int dstride, int16_t *src, int sstride) {
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      dst[i * dstride + j] = src[i * sstride + j];
}

/* TODO: Optimize this function for SSE. */
void copy_blocks_16_8bit(uint8_t *dst, int dstride, int16_t *src,
    unsigned char (*bskip)[2], int dering_count, int bsize)
{
  int bi, bx, by;
  if (bsize == 3) {
    for (bi = 0; bi < dering_count; bi++) {
      by = bskip[bi][0];
      bx = bskip[bi][1];
      copy_8x8_16_8bit(&dst[(by << 3) * dstride + (bx << 3)],
                     dstride,
                     &src[bi << 2*bsize], 1 << bsize);
    }
  } else {
    for (bi = 0; bi < dering_count; bi++) {
      by = bskip[bi][0];
      bx = bskip[bi][1];
      copy_4x4_16_8bit(&dst[(by << 2) * dstride + (bx << 2)],
                     dstride,
                     &src[bi << 2*bsize], 1 << bsize);
    }
  }
}

/* TODO: Optimize this function for SSE. */
static void copy_sb8_16(AV1_COMMON *cm, int16_t * restrict dst, int dstride,
    const uint8_t * restrict src, int src_voffset, int src_hoffset, int sstride,
    int vsize, int hsize)
{
  int r, c;
#if CONFIG_AOM_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    const uint16_t *base = &CONVERT_TO_SHORTPTR(src)[src_voffset * sstride
                                                     + src_hoffset];
    for (r = 0; r < vsize; r++) {
      for (c = 0; c < hsize; c++) {
        dst[r * dstride + c] = base[r*sstride + c];
      }
    }
  } else {
#endif
    const uint8_t *base = &src[src_voffset * sstride + src_hoffset];
    for (r = 0; r < vsize; r++) {
      for (c = 0; c < hsize; c++) {
        dst[r * dstride + c] = base[r*sstride + c];
      }
    }
#if CONFIG_AOM_HIGHBITDEPTH
  }
#endif

}

void av1_dering_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                      MACROBLOCKD *xd, int global_level) {
  int r, c;
  int sbr, sbc;
  int nhsb, nvsb;
  od_dering_in src[OD_DERING_INBUF_SIZE];
  int16_t *_linebuf[3];
  int16_t *linebuf[3];
  int16_t colbuf[3][OD_BSIZE_MAX + 2*OD_FILT_VBORDER][OD_FILT_HBORDER];
  unsigned char bskip[MAX_MIB_SIZE*MAX_MIB_SIZE][2];
  unsigned char *row_dering, *prev_row_dering, *curr_row_dering;
  int dering_count;
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride;
  int bsize[3];
  int dec[3];
  int pli;
  int last_sbc;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  av1_setup_dst_planes(xd->plane, frame, 0, 0);
  row_dering = aom_malloc(sizeof(*row_dering) * nhsb * 2);
  memset(row_dering, 1, sizeof(*row_dering) * (nhsb + 2) * 2);
  prev_row_dering = row_dering + 1;
  curr_row_dering = prev_row_dering + nhsb + 2;
  for (pli = 0; pli < 3; pli++) {
    dec[pli] = xd->plane[pli].subsampling_x;
    bsize[pli] = 3 - dec[pli];
  }
  stride = (cm->mi_cols << bsize[0]) + 2*OD_FILT_HBORDER;
  for (pli = 0; pli < 3; pli++) {
    int i;
    _linebuf[pli] = aom_malloc(sizeof(*_linebuf) * OD_FILT_VBORDER * stride);
    for (i = 0; i < 2 * OD_FILT_VBORDER * stride; i++)
      _linebuf[pli][i] = OD_DERING_VERY_LARGE;
    linebuf[pli] = _linebuf[pli] + OD_FILT_HBORDER;
  }
  for (sbr = 0; sbr < nvsb; sbr++) {
    last_sbc = -1;
    for (pli = 0; pli < 3; pli++) {
      for (r = 0; r < (MAX_MIB_SIZE << bsize[pli]) + 2*OD_FILT_VBORDER; r++) {
        for (c = 0; c < OD_FILT_HBORDER; c++) {
          colbuf[pli][r][c] = OD_DERING_VERY_LARGE;
        }
      }
    }
    for (sbc = 0; sbc < nhsb; sbc++) {
      int level;
      int nhb, nvb;
      int cstart = 0;
      if (sbc != last_sbc + 1)
        cstart = -OD_FILT_HBORDER;
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
      level = compute_level_from_index(
          global_level, cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                                            MAX_MIB_SIZE * sbc]
                            ->mbmi.dering_gain);
      curr_row_dering[sbc] = 0;
      if (level == 0 ||
          sb_all_skip_out(cm, sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, bskip, &dering_count))
        continue;
      curr_row_dering[sbc] = 1;
      for (pli = 0; pli < 3; pli++) {
        int16_t dst[MAX_MIB_SIZE * MAX_MIB_SIZE * 8 * 8];
        int threshold;
        int16_t *in;
        int i, j;
        int coffset;
        int rend, cend;
        if (sbc == nhsb - 1)
          cend = (nhb << bsize[pli]);
        else
          cend = (nhb << bsize[pli]) + OD_FILT_HBORDER;
        if (sbr == nvsb - 1)
          rend = (nvb << bsize[pli]);
        else
          rend = (nvb << bsize[pli]) + OD_FILT_VBORDER;
        coffset = sbc * MAX_MIB_SIZE << bsize[pli];
        if (sbc == nhsb - 1) {
          /* On the last superblock column, fill in the right border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          for (r = 0; r < rend; r++) {
            for (c = cend; c < (nhb << bsize[pli]) + OD_FILT_HBORDER; ++c) {
              src[(r + OD_FILT_VBORDER) * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER]
                  = OD_DERING_VERY_LARGE;
            }
          }
        }
        if (sbr == nvsb - 1) {
          /* On the last superblock row, fill in the bottom border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          for (r = rend; r < rend + OD_FILT_VBORDER; r++) {
            for (c = 0; c < (nhb << bsize[pli]) + 2*OD_FILT_HBORDER; c++) {
              src[(r + OD_FILT_VBORDER) * OD_FILT_BSTRIDE + c] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        in = src + OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER;
        /* We avoid filtering the pixels for which some of the pixels to average
           are outside the frame. We could change the filter instead, but it would
           add special cases for any future vectorization. */
        for (i = -OD_FILT_VBORDER; i < 0; i++) {
          for (j = -OD_FILT_HBORDER;
              j < (nhb << bsize[pli]) + OD_FILT_HBORDER;
              j++) {
            in[i * OD_FILT_BSTRIDE + j] =
                linebuf[pli][(OD_FILT_VBORDER + i) * stride +
                                  (sbc * MAX_MIB_SIZE << bsize[pli]) + j];
          }
        }
        /* Copy in the pixels we need from the current superblock for
           deringing.*/
        copy_sb8_16(cm, &src[OD_FILT_VBORDER*OD_FILT_BSTRIDE + OD_FILT_HBORDER
                             + cstart], OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
            (MAX_MIB_SIZE << bsize[pli]) * sbr, coffset + cstart,
            xd->plane[pli].dst.stride, rend, cend-cstart);
        if (!prev_row_dering[sbc]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER], OD_FILT_BSTRIDE,
              xd->plane[pli].dst.buf,
              (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER, coffset,
              xd->plane[pli].dst.stride, OD_FILT_VBORDER, nhb << bsize[pli]);
        }
        if (!prev_row_dering[sbc - 1]) {
          copy_sb8_16(cm, src, OD_FILT_BSTRIDE,
              xd->plane[pli].dst.buf,
              (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER,
              coffset - OD_FILT_HBORDER,
              xd->plane[pli].dst.stride, OD_FILT_VBORDER, OD_FILT_HBORDER);
        }
        if (!prev_row_dering[sbc + 1]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER + (nhb << bsize[pli])],
              OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
              (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER,
              coffset + (nhb << bsize[pli]),
              xd->plane[pli].dst.stride, OD_FILT_VBORDER, OD_FILT_HBORDER);
        }
        if (sbc == last_sbc + 1) {
          /* If we deringed the superblock on the left then we need to copy in
             saved pixels. */
          for (r = 0; r < rend + OD_FILT_VBORDER; r++) {
            for (c = 0; c < OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c] = colbuf[pli][r][c];
            }
          }
        }
        for (r = 0; r < rend + OD_FILT_VBORDER; r++) {
          for (c = 0; c < OD_FILT_HBORDER; c++) {
            /* Saving pixels in case we need to dering the superblock on the
               right. */
            colbuf[pli][r][c] = src[r * OD_FILT_BSTRIDE + c
                                    + (nhb << bsize[pli])];
          }
        }
        copy_sb8_16(cm, &linebuf[pli][coffset], stride, xd->plane[pli].dst.buf,
            (MAX_MIB_SIZE << bsize[pli]) * (sbr + 1) - OD_FILT_VBORDER, coffset,
            xd->plane[pli].dst.stride, OD_FILT_VBORDER,
            (nhb << bsize[pli]));

        /* FIXME: This is a temporary hack that uses more conservative
           deringing for chroma. */
        if (pli)
          threshold = (level * 5 + 4) >> 3 << coeff_shift;
        else
          threshold = level << coeff_shift;
        /* FIXME: Re-enable that optimization. */
        /*if (threshold == 0) continue;*/
        od_dering(dst, in, dec[pli], dir, pli,
                  bskip, dering_count, threshold, coeff_shift);
#if CONFIG_AOM_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          copy_blocks_16bit(
              (int16_t*)&CONVERT_TO_SHORTPTR(
                  xd->plane[pli].dst.buf)[xd->plane[pli].dst.stride *
                  (MAX_MIB_SIZE * sbr << bsize[pli]) +
                  (sbc * MAX_MIB_SIZE << bsize[pli])],
              xd->plane[pli].dst.stride, dst, bskip,
              dering_count, 3 - dec[pli]);
        } else {
#endif
          copy_blocks_16_8bit(
              &xd->plane[pli].dst.buf[xd->plane[pli].dst.stride *
                                    (MAX_MIB_SIZE * sbr << bsize[pli]) +
                                    (sbc * MAX_MIB_SIZE << bsize[pli])],
              xd->plane[pli].dst.stride, dst, bskip,
              dering_count, 3 - dec[pli]);
#if CONFIG_AOM_HIGHBITDEPTH
        }
#endif
      }
      last_sbc = sbc;
    }
    {
      unsigned char *tmp;
      tmp = prev_row_dering;
      prev_row_dering = curr_row_dering;
      curr_row_dering = tmp;
    }
  }
  aom_free(row_dering);
  for (pli = 0; pli < 3; pli++) {
    aom_free(_linebuf[pli]);
  }
}
