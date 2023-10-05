/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_ARM_NEON_SHIFT_NEON_H_
#define AOM_AV1_ENCODER_ARM_NEON_SHIFT_NEON_H_

#include <arm_neon.h>

#include "aom/aom_integer.h"  // For AOM_INLINE.

#define SHIFT_LOOP_HELPER(name, type, intrinsic, shift)              \
  static AOM_INLINE void name(const type *in, type *out, int size) { \
    int i = 0;                                                       \
    do {                                                             \
      out[i] = intrinsic(in[i], shift);                              \
    } while (++i < size);                                            \
  }

SHIFT_LOOP_HELPER(shift_right_1_round_s16_x4, int16x4_t, vrshr_n_s16, 1)
SHIFT_LOOP_HELPER(shift_right_1_round_s16_x8, int16x8_t, vrshrq_n_s16, 1)
SHIFT_LOOP_HELPER(shift_right_2_round_s16_x8, int16x8_t, vrshrq_n_s16, 2)
SHIFT_LOOP_HELPER(shift_right_4_round_s16_x8, int16x8_t, vrshrq_n_s16, 4)
SHIFT_LOOP_HELPER(shift_left_1_s16_x4, int16x4_t, vshl_n_s16, 1)
SHIFT_LOOP_HELPER(shift_left_1_s16_x8, int16x8_t, vshlq_n_s16, 1)
SHIFT_LOOP_HELPER(shift_left_2_s16_x4, int16x4_t, vshl_n_s16, 2)
SHIFT_LOOP_HELPER(shift_left_2_s16_x8, int16x8_t, vshlq_n_s16, 2)
SHIFT_LOOP_HELPER(shift_right_2_round_s32_x4, int32x4_t, vrshrq_n_s32, 2)

#undef SHIFT_LOOP_HELPER

#endif  // AOM_AV1_ENCODER_ARM_NEON_SHIFT_NEON_H_
