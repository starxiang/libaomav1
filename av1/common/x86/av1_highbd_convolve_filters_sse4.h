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
<<<<<<< HEAD   (c1ca94 Merge changes from topic 'update_dering' into nextgenv2)

#ifndef AV1_COMMON_X86_AV1_HIGHBD_CONVOLVE_FILTERS_SSE4_H_
#define AV1_COMMON_X86_AV1_HIGHBD_CONVOLVE_FILTERS_SSE4_H_

#include "./aom_config.h"

#if CONFIG_AOM_HIGHBITDEPTH
#if CONFIG_EXT_INTERP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_10sharp_highbd_ver_signal_dir[15][6][8]) = {
  {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 127, -6, 127, -6, 127, -6, 127 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -2, 5, -2, 5, -2, 5, -2, 5 },
      { -12, 124, -12, 124, -12, 124, -12, 124 },
      { 18, -7, 18, -7, 18, -7, 18, -7 },
      { 3, -2, 3, -2, 3, -2, 3, -2 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -3, 7, -3, 7, -3, 7, -3, 7 },
      { -17, 119, -17, 119, -17, 119, -17, 119 },
      { 28, -11, 28, -11, 28, -11, 28, -11 },
      { 5, -2, 5, -2, 5, -2, 5, -2 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { -20, 114, -20, 114, -20, 114, -20, 114 },
      { 38, -14, 38, -14, 38, -14, 38, -14 },
      { 7, -3, 7, -3, 7, -3, 7, -3 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -4, 9, -4, 9, -4, 9, -4, 9 },
      { -22, 107, -22, 107, -22, 107, -22, 107 },
      { 49, -17, 49, -17, 49, -17, 49, -17 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 2, 0, 2, 0, 2, 0, 2 },
      { -5, 10, -5, 10, -5, 10, -5, 10 },
      { -24, 99, -24, 99, -24, 99, -24, 99 },
      { 59, -20, 59, -20, 59, -20, 59, -20 },
      { 9, -4, 9, -4, 9, -4, 9, -4 },
      { 2, 0, 2, 0, 2, 0, 2, 0 },
  },
  {
      { 0, 2, 0, 2, 0, 2, 0, 2 },
      { -5, 10, -5, 10, -5, 10, -5, 10 },
      { -24, 90, -24, 90, -24, 90, -24, 90 },
      { 70, -22, 70, -22, 70, -22, 70, -22 },
      { 10, -5, 10, -5, 10, -5, 10, -5 },
      { 2, 0, 2, 0, 2, 0, 2, 0 },
  },
  {
      { 0, 2, 0, 2, 0, 2, 0, 2 },
      { -5, 10, -5, 10, -5, 10, -5, 10 },
      { -23, 80, -23, 80, -23, 80, -23, 80 },
      { 80, -23, 80, -23, 80, -23, 80, -23 },
      { 10, -5, 10, -5, 10, -5, 10, -5 },
      { 2, 0, 2, 0, 2, 0, 2, 0 },
  },
  {
      { 0, 2, 0, 2, 0, 2, 0, 2 },
      { -5, 10, -5, 10, -5, 10, -5, 10 },
      { -22, 70, -22, 70, -22, 70, -22, 70 },
      { 90, -24, 90, -24, 90, -24, 90, -24 },
      { 10, -5, 10, -5, 10, -5, 10, -5 },
      { 2, 0, 2, 0, 2, 0, 2, 0 },
  },
  {
      { 0, 2, 0, 2, 0, 2, 0, 2 },
      { -4, 9, -4, 9, -4, 9, -4, 9 },
      { -20, 59, -20, 59, -20, 59, -20, 59 },
      { 99, -24, 99, -24, 99, -24, 99, -24 },
      { 10, -5, 10, -5, 10, -5, 10, -5 },
      { 2, 0, 2, 0, 2, 0, 2, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { -17, 49, -17, 49, -17, 49, -17, 49 },
      { 107, -22, 107, -22, 107, -22, 107, -22 },
      { 9, -4, 9, -4, 9, -4, 9, -4 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -3, 7, -3, 7, -3, 7, -3, 7 },
      { -14, 38, -14, 38, -14, 38, -14, 38 },
      { 114, -20, 114, -20, 114, -20, 114, -20 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -2, 5, -2, 5, -2, 5, -2, 5 },
      { -11, 28, -11, 28, -11, 28, -11, 28 },
      { 119, -17, 119, -17, 119, -17, 119, -17 },
      { 7, -3, 7, -3, 7, -3, 7, -3 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { -2, 3, -2, 3, -2, 3, -2, 3 },
      { -7, 18, -7, 18, -7, 18, -7, 18 },
      { 124, -12, 124, -12, 124, -12, 124, -12 },
      { 5, -2, 5, -2, 5, -2, 5, -2 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { 127, -6, 127, -6, 127, -6, 127, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
  },
};
#endif
#endif
#if CONFIG_AOM_HIGHBITDEPTH
#if CONFIG_EXT_INTERP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_12sharp_highbd_ver_signal_dir[15][6][8]) = {
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -2, 3, -2, 3, -2, 3, -2, 3 },
      { -7, 127, -7, 127, -7, 127, -7, 127 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -3, 6, -3, 6, -3, 6, -3, 6 },
      { -13, 124, -13, 124, -13, 124, -13, 124 },
      { 18, -8, 18, -8, 18, -8, 18, -8 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { -18, 120, -18, 120, -18, 120, -18, 120 },
      { 28, -12, 28, -12, 28, -12, 28, -12 },
      { 7, -4, 7, -4, 7, -4, 7, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 10, -6, 10, -6, 10, -6, 10 },
      { -21, 115, -21, 115, -21, 115, -21, 115 },
      { 38, -15, 38, -15, 38, -15, 38, -15 },
      { 8, -5, 8, -5, 8, -5, 8, -5 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -6, 12, -6, 12, -6, 12, -6, 12 },
      { -24, 108, -24, 108, -24, 108, -24, 108 },
      { 49, -18, 49, -18, 49, -18, 49, -18 },
      { 10, -6, 10, -6, 10, -6, 10, -6 },
      { 3, -2, 3, -2, 3, -2, 3, -2 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -7, 13, -7, 13, -7, 13, -7, 13 },
      { -25, 100, -25, 100, -25, 100, -25, 100 },
      { 60, -21, 60, -21, 60, -21, 60, -21 },
      { 11, -7, 11, -7, 11, -7, 11, -7 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -7, 13, -7, 13, -7, 13, -7, 13 },
      { -26, 91, -26, 91, -26, 91, -26, 91 },
      { 71, -24, 71, -24, 71, -24, 71, -24 },
      { 13, -7, 13, -7, 13, -7, 13, -7 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -7, 13, -7, 13, -7, 13, -7, 13 },
      { -25, 81, -25, 81, -25, 81, -25, 81 },
      { 81, -25, 81, -25, 81, -25, 81, -25 },
      { 13, -7, 13, -7, 13, -7, 13, -7 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -7, 13, -7, 13, -7, 13, -7, 13 },
      { -24, 71, -24, 71, -24, 71, -24, 71 },
      { 91, -26, 91, -26, 91, -26, 91, -26 },
      { 13, -7, 13, -7, 13, -7, 13, -7 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -7, 11, -7, 11, -7, 11, -7, 11 },
      { -21, 60, -21, 60, -21, 60, -21, 60 },
      { 100, -25, 100, -25, 100, -25, 100, -25 },
      { 13, -7, 13, -7, 13, -7, 13, -7 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -2, 3, -2, 3, -2, 3, -2, 3 },
      { -6, 10, -6, 10, -6, 10, -6, 10 },
      { -18, 49, -18, 49, -18, 49, -18, 49 },
      { 108, -24, 108, -24, 108, -24, 108, -24 },
      { 12, -6, 12, -6, 12, -6, 12, -6 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -5, 8, -5, 8, -5, 8, -5, 8 },
      { -15, 38, -15, 38, -15, 38, -15, 38 },
      { 115, -21, 115, -21, 115, -21, 115, -21 },
      { 10, -6, 10, -6, 10, -6, 10, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 7, -4, 7, -4, 7, -4, 7 },
      { -12, 28, -12, 28, -12, 28, -12, 28 },
      { 120, -18, 120, -18, 120, -18, 120, -18 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -8, 18, -8, 18, -8, 18, -8, 18 },
      { 124, -13, 124, -13, 124, -13, 124, -13 },
      { 6, -3, 6, -3, 6, -3, 6, -3 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { 127, -7, 127, -7, 127, -7, 127, -7 },
      { 3, -2, 3, -2, 3, -2, 3, -2 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
};
#endif
#endif
#if CONFIG_AOM_HIGHBITDEPTH
#if USE_TEMPORALFILTER_12TAP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_temporalfilter_12_highbd_ver_signal_dir[15][6]
                                                                       [8]) = {
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -7, 127, -7, 127, -7, 127, -7, 127 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
      { 0, 0, 0, 0, 0, 0, 0, 0 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -3, 5, -3, 5, -3, 5, -3, 5 },
      { -12, 124, -12, 124, -12, 124, -12, 124 },
      { 18, -8, 18, -8, 18, -8, 18, -8 },
      { 4, -2, 4, -2, 4, -2, 4, -2 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { -17, 120, -17, 120, -17, 120, -17, 120 },
      { 28, -11, 28, -11, 28, -11, 28, -11 },
      { 6, -3, 6, -3, 6, -3, 6, -3 },
      { 1, -1, 1, -1, 1, -1, 1, -1 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 10, -4, 10, -4, 10, -4, 10 },
      { -21, 114, -21, 114, -21, 114, -21, 114 },
      { 38, -15, 38, -15, 38, -15, 38, -15 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -5, 11, -5, 11, -5, 11, -5, 11 },
      { -23, 107, -23, 107, -23, 107, -23, 107 },
      { 49, -18, 49, -18, 49, -18, 49, -18 },
      { 9, -5, 9, -5, 9, -5, 9, -5 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 12, -6, 12, -6, 12, -6, 12 },
      { -25, 99, -25, 99, -25, 99, -25, 99 },
      { 60, -21, 60, -21, 60, -21, 60, -21 },
      { 11, -6, 11, -6, 11, -6, 11, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 12, -6, 12, -6, 12, -6, 12 },
      { -25, 90, -25, 90, -25, 90, -25, 90 },
      { 70, -23, 70, -23, 70, -23, 70, -23 },
      { 12, -6, 12, -6, 12, -6, 12, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 12, -6, 12, -6, 12, -6, 12 },
      { -24, 80, -24, 80, -24, 80, -24, 80 },
      { 80, -24, 80, -24, 80, -24, 80, -24 },
      { 12, -6, 12, -6, 12, -6, 12, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 12, -6, 12, -6, 12, -6, 12 },
      { -23, 70, -23, 70, -23, 70, -23, 70 },
      { 90, -25, 90, -25, 90, -25, 90, -25 },
      { 12, -6, 12, -6, 12, -6, 12, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 3, -1, 3, -1, 3, -1, 3 },
      { -6, 11, -6, 11, -6, 11, -6, 11 },
      { -21, 60, -21, 60, -21, 60, -21, 60 },
      { 99, -25, 99, -25, 99, -25, 99, -25 },
      { 12, -6, 12, -6, 12, -6, 12, -6 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -5, 9, -5, 9, -5, 9, -5, 9 },
      { -18, 49, -18, 49, -18, 49, -18, 49 },
      { 107, -23, 107, -23, 107, -23, 107, -23 },
      { 11, -5, 11, -5, 11, -5, 11, -5 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
  },
  {
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { -15, 38, -15, 38, -15, 38, -15, 38 },
      { 114, -21, 114, -21, 114, -21, 114, -21 },
      { 10, -4, 10, -4, 10, -4, 10, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { -1, 1, -1, 1, -1, 1, -1, 1 },
      { -3, 6, -3, 6, -3, 6, -3, 6 },
      { -11, 28, -11, 28, -11, 28, -11, 28 },
      { 120, -17, 120, -17, 120, -17, 120, -17 },
      { 8, -4, 8, -4, 8, -4, 8, -4 },
      { 2, -1, 2, -1, 2, -1, 2, -1 },
  },
  {
      { 0, 1, 0, 1, 0, 1, 0, 1 },
      { -2, 4, -2, 4, -2, 4, -2, 4 },
      { -8, 18, -8, 18, -8, 18, -8, 18 },
      { 124, -12, 124, -12, 124, -12, 124, -12 },
      { 5, -3, 5, -3, 5, -3, 5, -3 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
  },
  {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { -1, 2, -1, 2, -1, 2, -1, 2 },
      { -4, 8, -4, 8, -4, 8, -4, 8 },
      { 127, -7, 127, -7, 127, -7, 127, -7 },
      { 3, -1, 3, -1, 3, -1, 3, -1 },
      { 1, 0, 1, 0, 1, 0, 1, 0 },
=======
#ifndef AV1_COMMON_X86_AV1_HIGHBD_CONVOLVE_FILTERS_SSE4_H_
#define AV1_COMMON_X86_AV1_HIGHBD_CONVOLVE_FILTERS_SSE4_H_

#include "./aom_config.h"
#include "av1/common/filter.h"

#if CONFIG_AOM_HIGHBITDEPTH
#if CONFIG_EXT_INTERP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_10sharp_highbd_ver_signal_dir[15][6][8]) = {
  {
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
      {
          -1, 3, -1, 3, -1, 3, -1, 3,
      },
      {
          -6, 127, -6, 127, -6, 127, -6, 127,
      },
      {
          8, -4, 8, -4, 8, -4, 8, -4,
      },
      {
          2, -1, 2, -1, 2, -1, 2, -1,
      },
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -2, 5, -2, 5, -2, 5, -2, 5,
      },
      {
          -12, 124, -12, 124, -12, 124, -12, 124,
      },
      {
          18, -7, 18, -7, 18, -7, 18, -7,
      },
      {
          3, -2, 3, -2, 3, -2, 3, -2,
      },
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -3, 7, -3, 7, -3, 7, -3, 7,
      },
      {
          -17, 119, -17, 119, -17, 119, -17, 119,
      },
      {
          28, -11, 28, -11, 28, -11, 28, -11,
      },
      {
          5, -2, 5, -2, 5, -2, 5, -2,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -4, 8, -4, 8, -4, 8, -4, 8,
      },
      {
          -20, 114, -20, 114, -20, 114, -20, 114,
      },
      {
          38, -14, 38, -14, 38, -14, 38, -14,
      },
      {
          7, -3, 7, -3, 7, -3, 7, -3,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -4, 9, -4, 9, -4, 9, -4, 9,
      },
      {
          -22, 107, -22, 107, -22, 107, -22, 107,
      },
      {
          49, -17, 49, -17, 49, -17, 49, -17,
      },
      {
          8, -4, 8, -4, 8, -4, 8, -4,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 2, 0, 2, 0, 2, 0, 2,
      },
      {
          -5, 10, -5, 10, -5, 10, -5, 10,
      },
      {
          -24, 99, -24, 99, -24, 99, -24, 99,
      },
      {
          59, -20, 59, -20, 59, -20, 59, -20,
      },
      {
          9, -4, 9, -4, 9, -4, 9, -4,
      },
      {
          2, 0, 2, 0, 2, 0, 2, 0,
      },
  },
  {
      {
          0, 2, 0, 2, 0, 2, 0, 2,
      },
      {
          -5, 10, -5, 10, -5, 10, -5, 10,
      },
      {
          -24, 90, -24, 90, -24, 90, -24, 90,
      },
      {
          70, -22, 70, -22, 70, -22, 70, -22,
      },
      {
          10, -5, 10, -5, 10, -5, 10, -5,
      },
      {
          2, 0, 2, 0, 2, 0, 2, 0,
      },
  },
  {
      {
          0, 2, 0, 2, 0, 2, 0, 2,
      },
      {
          -5, 10, -5, 10, -5, 10, -5, 10,
      },
      {
          -23, 80, -23, 80, -23, 80, -23, 80,
      },
      {
          80, -23, 80, -23, 80, -23, 80, -23,
      },
      {
          10, -5, 10, -5, 10, -5, 10, -5,
      },
      {
          2, 0, 2, 0, 2, 0, 2, 0,
      },
  },
  {
      {
          0, 2, 0, 2, 0, 2, 0, 2,
      },
      {
          -5, 10, -5, 10, -5, 10, -5, 10,
      },
      {
          -22, 70, -22, 70, -22, 70, -22, 70,
      },
      {
          90, -24, 90, -24, 90, -24, 90, -24,
      },
      {
          10, -5, 10, -5, 10, -5, 10, -5,
      },
      {
          2, 0, 2, 0, 2, 0, 2, 0,
      },
  },
  {
      {
          0, 2, 0, 2, 0, 2, 0, 2,
      },
      {
          -4, 9, -4, 9, -4, 9, -4, 9,
      },
      {
          -20, 59, -20, 59, -20, 59, -20, 59,
      },
      {
          99, -24, 99, -24, 99, -24, 99, -24,
      },
      {
          10, -5, 10, -5, 10, -5, 10, -5,
      },
      {
          2, 0, 2, 0, 2, 0, 2, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -4, 8, -4, 8, -4, 8, -4, 8,
      },
      {
          -17, 49, -17, 49, -17, 49, -17, 49,
      },
      {
          107, -22, 107, -22, 107, -22, 107, -22,
      },
      {
          9, -4, 9, -4, 9, -4, 9, -4,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -3, 7, -3, 7, -3, 7, -3, 7,
      },
      {
          -14, 38, -14, 38, -14, 38, -14, 38,
      },
      {
          114, -20, 114, -20, 114, -20, 114, -20,
      },
      {
          8, -4, 8, -4, 8, -4, 8, -4,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -2, 5, -2, 5, -2, 5, -2, 5,
      },
      {
          -11, 28, -11, 28, -11, 28, -11, 28,
      },
      {
          119, -17, 119, -17, 119, -17, 119, -17,
      },
      {
          7, -3, 7, -3, 7, -3, 7, -3,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
      {
          -2, 3, -2, 3, -2, 3, -2, 3,
      },
      {
          -7, 18, -7, 18, -7, 18, -7, 18,
      },
      {
          124, -12, 124, -12, 124, -12, 124, -12,
      },
      {
          5, -2, 5, -2, 5, -2, 5, -2,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
      {
          -1, 2, -1, 2, -1, 2, -1, 2,
      },
      {
          -4, 8, -4, 8, -4, 8, -4, 8,
      },
      {
          127, -6, 127, -6, 127, -6, 127, -6,
      },
      {
          3, -1, 3, -1, 3, -1, 3, -1,
      },
      {
          0, 0, 0, 0, 0, 0, 0, 0,
      },
  },
};
#endif
#endif
#if CONFIG_AOM_HIGHBITDEPTH
#if CONFIG_EXT_INTERP
DECLARE_ALIGNED(16, static const int16_t,
                sub_pel_filters_12sharp_highbd_ver_signal_dir[15][6][8]) = {
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -2, 3, -2, 3, -2, 3, -2, 3,
      },
      {
          -7, 127, -7, 127, -7, 127, -7, 127,
      },
      {
          8, -4, 8, -4, 8, -4, 8, -4,
      },
      {
          2, -1, 2, -1, 2, -1, 2, -1,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
  },
  {
      {
          -1, 2, -1, 2, -1, 2, -1, 2,
      },
      {
          -3, 6, -3, 6, -3, 6, -3, 6,
      },
      {
          -13, 124, -13, 124, -13, 124, -13, 124,
      },
      {
          18, -8, 18, -8, 18, -8, 18, -8,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
      {
          2, -1, 2, -1, 2, -1, 2, -1,
      },
  },
  {
      {
          -1, 3, -1, 3, -1, 3, -1, 3,
      },
      {
          -4, 8, -4, 8, -4, 8, -4, 8,
      },
      {
          -18, 120, -18, 120, -18, 120, -18, 120,
      },
      {
          28, -12, 28, -12, 28, -12, 28, -12,
      },
      {
          7, -4, 7, -4, 7, -4, 7, -4,
      },
      {
          2, -1, 2, -1, 2, -1, 2, -1,
      },
  },
  {
      {
          -1, 3, -1, 3, -1, 3, -1, 3,
      },
      {
          -6, 10, -6, 10, -6, 10, -6, 10,
      },
      {
          -21, 115, -21, 115, -21, 115, -21, 115,
      },
      {
          38, -15, 38, -15, 38, -15, 38, -15,
      },
      {
          8, -5, 8, -5, 8, -5, 8, -5,
      },
      {
          3, -1, 3, -1, 3, -1, 3, -1,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -6, 12, -6, 12, -6, 12, -6, 12,
      },
      {
          -24, 108, -24, 108, -24, 108, -24, 108,
      },
      {
          49, -18, 49, -18, 49, -18, 49, -18,
      },
      {
          10, -6, 10, -6, 10, -6, 10, -6,
      },
      {
          3, -2, 3, -2, 3, -2, 3, -2,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -7, 13, -7, 13, -7, 13, -7, 13,
      },
      {
          -25, 100, -25, 100, -25, 100, -25, 100,
      },
      {
          60, -21, 60, -21, 60, -21, 60, -21,
      },
      {
          11, -7, 11, -7, 11, -7, 11, -7,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -7, 13, -7, 13, -7, 13, -7, 13,
      },
      {
          -26, 91, -26, 91, -26, 91, -26, 91,
      },
      {
          71, -24, 71, -24, 71, -24, 71, -24,
      },
      {
          13, -7, 13, -7, 13, -7, 13, -7,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -7, 13, -7, 13, -7, 13, -7, 13,
      },
      {
          -25, 81, -25, 81, -25, 81, -25, 81,
      },
      {
          81, -25, 81, -25, 81, -25, 81, -25,
      },
      {
          13, -7, 13, -7, 13, -7, 13, -7,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -7, 13, -7, 13, -7, 13, -7, 13,
      },
      {
          -24, 71, -24, 71, -24, 71, -24, 71,
      },
      {
          91, -26, 91, -26, 91, -26, 91, -26,
      },
      {
          13, -7, 13, -7, 13, -7, 13, -7,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -7, 11, -7, 11, -7, 11, -7, 11,
      },
      {
          -21, 60, -21, 60, -21, 60, -21, 60,
      },
      {
          100, -25, 100, -25, 100, -25, 100, -25,
      },
      {
          13, -7, 13, -7, 13, -7, 13, -7,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -2, 3, -2, 3, -2, 3, -2, 3,
      },
      {
          -6, 10, -6, 10, -6, 10, -6, 10,
      },
      {
          -18, 49, -18, 49, -18, 49, -18, 49,
      },
      {
          108, -24, 108, -24, 108, -24, 108, -24,
      },
      {
          12, -6, 12, -6, 12, -6, 12, -6,
      },
      {
          4, -2, 4, -2, 4, -2, 4, -2,
      },
  },
  {
      {
          -1, 3, -1, 3, -1, 3, -1, 3,
      },
      {
          -5, 8, -5, 8, -5, 8, -5, 8,
      },
      {
          -15, 38, -15, 38, -15, 38, -15, 38,
      },
      {
          115, -21, 115, -21, 115, -21, 115, -21,
      },
      {
          10, -6, 10, -6, 10, -6, 10, -6,
      },
      {
          3, -1, 3, -1, 3, -1, 3, -1,
      },
  },
  {
      {
          -1, 2, -1, 2, -1, 2, -1, 2,
      },
      {
          -4, 7, -4, 7, -4, 7, -4, 7,
      },
      {
          -12, 28, -12, 28, -12, 28, -12, 28,
      },
      {
          120, -18, 120, -18, 120, -18, 120, -18,
      },
      {
          8, -4, 8, -4, 8, -4, 8, -4,
      },
      {
          3, -1, 3, -1, 3, -1, 3, -1,
      },
  },
  {
      {
          -1, 2, -1, 2, -1, 2, -1, 2,
      },
      {
          -2, 4, -2, 4, -2, 4, -2, 4,
      },
      {
          -8, 18, -8, 18, -8, 18, -8, 18,
      },
      {
          124, -13, 124, -13, 124, -13, 124, -13,
      },
      {
          6, -3, 6, -3, 6, -3, 6, -3,
      },
      {
          2, -1, 2, -1, 2, -1, 2, -1,
      },
  },
  {
      {
          0, 1, 0, 1, 0, 1, 0, 1,
      },
      {
          -1, 2, -1, 2, -1, 2, -1, 2,
      },
      {
          -4, 8, -4, 8, -4, 8, -4, 8,
      },
      {
          127, -7, 127, -7, 127, -7, 127, -7,
      },
      {
          3, -2, 3, -2, 3, -2, 3, -2,
      },
      {
          1, 0, 1, 0, 1, 0, 1, 0,
      },
>>>>>>> BRANCH (749267 Fix clang-format issues.)
  },
};
#endif
#endif
#endif  // AV1_COMMON_X86_AV1_HIGHBD_CONVOLVE_FILTERS_SSE4_H_
