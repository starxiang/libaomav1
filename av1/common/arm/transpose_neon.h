/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AV1_DSP_ARM_TRANSPOSE_NEON_H_
#define AV1_DSP_ARM_TRANSPOSE_NEON_H_

#include <arm_neon.h>

static INLINE void transpose_u8_8x8(uint8x8_t *a0, uint8x8_t *a1, uint8x8_t *a2,
                                    uint8x8_t *a3, uint8x8_t *a4, uint8x8_t *a5,
                                    uint8x8_t *a6, uint8x8_t *a7) {
  // Swap 8 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16  40 50 42 52 44 54 46 56
  // b0.val[1]: 01 11 03 13 05 15 07 17  41 51 43 53 45 55 47 57
  // b1.val[0]: 20 30 22 32 24 34 26 36  60 70 62 72 64 74 66 76
  // b1.val[1]: 21 31 23 33 25 35 27 37  61 71 63 73 65 75 67 77

  const uint8x16x2_t b0 =
      vtrnq_u8(vcombine_u8(*a0, *a4), vcombine_u8(*a1, *a5));
  const uint8x16x2_t b1 =
      vtrnq_u8(vcombine_u8(*a2, *a6), vcombine_u8(*a3, *a7));

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34  40 50 60 70 44 54 64 74
  // c0.val[1]: 02 12 22 32 06 16 26 36  42 52 62 72 46 56 66 76
  // c1.val[0]: 01 11 21 31 05 15 25 35  41 51 61 71 45 55 65 75
  // c1.val[1]: 03 13 23 33 07 17 27 37  43 53 63 73 47 57 67 77

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));

  // Unzip 32 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70  01 11 21 31 41 51 61 71
  // d0.val[1]: 04 14 24 34 44 54 64 74  05 15 25 35 45 55 65 75
  // d1.val[0]: 02 12 22 32 42 52 62 72  03 13 23 33 43 53 63 73
  // d1.val[1]: 06 16 26 36 46 56 66 76  07 17 27 37 47 57 67 77
  const uint32x4x2_t d0 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c1.val[0]));
  const uint32x4x2_t d1 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c1.val[1]));

  *a0 = vreinterpret_u8_u32(vget_low_u32(d0.val[0]));
  *a1 = vreinterpret_u8_u32(vget_high_u32(d0.val[0]));
  *a2 = vreinterpret_u8_u32(vget_low_u32(d1.val[0]));
  *a3 = vreinterpret_u8_u32(vget_high_u32(d1.val[0]));
  *a4 = vreinterpret_u8_u32(vget_low_u32(d0.val[1]));
  *a5 = vreinterpret_u8_u32(vget_high_u32(d0.val[1]));
  *a6 = vreinterpret_u8_u32(vget_low_u32(d1.val[1]));
  *a7 = vreinterpret_u8_u32(vget_high_u32(d1.val[1]));
}

static INLINE void transpose_u8_8x4(uint8x8_t *a0, uint8x8_t *a1, uint8x8_t *a2,
                                    uint8x8_t *a3) {
  // Swap 8 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37

  const uint8x8x2_t b0 = vtrn_u8(*a0, *a1);
  const uint8x8x2_t b1 = vtrn_u8(*a2, *a3);

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37

  const uint16x4x2_t c0 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[0]), vreinterpret_u16_u8(b1.val[0]));
  const uint16x4x2_t c1 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[1]), vreinterpret_u16_u8(b1.val[1]));

  *a0 = vreinterpret_u8_u16(c0.val[0]);
  *a1 = vreinterpret_u8_u16(c1.val[0]);
  *a2 = vreinterpret_u8_u16(c0.val[1]);
  *a3 = vreinterpret_u8_u16(c1.val[1]);
}

static INLINE void transpose_u16_4x8(uint16x4_t *a0, uint16x4_t *a1,
                                     uint16x4_t *a2, uint16x4_t *a3,
                                     uint16x4_t *a4, uint16x4_t *a5,
                                     uint16x4_t *a6, uint16x4_t *a7,
                                     uint16x8_t *o0, uint16x8_t *o1,
                                     uint16x8_t *o2, uint16x8_t *o3) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // a4: 40 41 42 43
  // a5: 50 51 52 53
  // a6: 60 61 62 63
  // a7: 70 71 72 73
  // to:
  // b0.val[0]: 00 10 02 12
  // b0.val[1]: 01 11 03 13
  // b1.val[0]: 20 30 22 32
  // b1.val[1]: 21 31 23 33
  // b2.val[0]: 40 50 42 52
  // b2.val[1]: 41 51 43 53
  // b3.val[0]: 60 70 62 72
  // b3.val[1]: 61 71 63 73

  uint16x4x2_t b0 = vtrn_u16(*a0, *a1);
  uint16x4x2_t b1 = vtrn_u16(*a2, *a3);
  uint16x4x2_t b2 = vtrn_u16(*a4, *a5);
  uint16x4x2_t b3 = vtrn_u16(*a6, *a7);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30
  // c0.val[1]: 02 12 22 32
  // c1.val[0]: 01 11 21 31
  // c1.val[1]: 03 13 23 33
  // c2.val[0]: 40 50 60 70
  // c2.val[1]: 42 52 62 72
  // c3.val[0]: 41 51 61 71
  // c3.val[1]: 43 53 63 73

  uint32x2x2_t c0 = vtrn_u32(vreinterpret_u32_u16(b0.val[0]),
                             vreinterpret_u32_u16(b1.val[0]));
  uint32x2x2_t c1 = vtrn_u32(vreinterpret_u32_u16(b0.val[1]),
                             vreinterpret_u32_u16(b1.val[1]));
  uint32x2x2_t c2 = vtrn_u32(vreinterpret_u32_u16(b2.val[0]),
                             vreinterpret_u32_u16(b3.val[0]));
  uint32x2x2_t c3 = vtrn_u32(vreinterpret_u32_u16(b2.val[1]),
                             vreinterpret_u32_u16(b3.val[1]));

  // Swap 64 bit elements resulting in:
  // o0: 00 10 20 30 40 50 60 70
  // o1: 01 11 21 31 41 51 61 71
  // o2: 02 12 22 32 42 52 62 72
  // o3: 03 13 23 33 43 53 63 73

  *o0 = vcombine_u16(vreinterpret_u16_u32(c0.val[0]),
                     vreinterpret_u16_u32(c2.val[0]));
  *o1 = vcombine_u16(vreinterpret_u16_u32(c1.val[0]),
                     vreinterpret_u16_u32(c3.val[0]));
  *o2 = vcombine_u16(vreinterpret_u16_u32(c0.val[1]),
                     vreinterpret_u16_u32(c2.val[1]));
  *o3 = vcombine_u16(vreinterpret_u16_u32(c1.val[1]),
                     vreinterpret_u16_u32(c3.val[1]));
}

static INLINE void transpose_u16_8x8(uint16x8_t *a0, uint16x8_t *a1,
                                     uint16x8_t *a2, uint16x8_t *a3,
                                     uint16x8_t *a4, uint16x8_t *a5,
                                     uint16x8_t *a6, uint16x8_t *a7) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  // b2.val[0]: 40 50 42 52 44 54 46 56
  // b2.val[1]: 41 51 43 53 45 55 47 57
  // b3.val[0]: 60 70 62 72 64 74 66 76
  // b3.val[1]: 61 71 63 73 65 75 67 77

  const uint16x8x2_t b0 = vtrnq_u16(*a0, *a1);
  const uint16x8x2_t b1 = vtrnq_u16(*a2, *a3);
  const uint16x8x2_t b2 = vtrnq_u16(*a4, *a5);
  const uint16x8x2_t b3 = vtrnq_u16(*a6, *a7);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37
  // c2.val[0]: 40 50 60 70 44 54 64 74
  // c2.val[1]: 42 52 62 72 46 56 66 76
  // c3.val[0]: 41 51 61 71 45 55 65 75
  // c3.val[1]: 43 53 63 73 47 57 67 77

  const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[0]),
                                    vreinterpretq_u32_u16(b1.val[0]));
  const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[1]),
                                    vreinterpretq_u32_u16(b1.val[1]));
  const uint32x4x2_t c2 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[0]),
                                    vreinterpretq_u32_u16(b3.val[0]));
  const uint32x4x2_t c3 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[1]),
                                    vreinterpretq_u32_u16(b3.val[1]));

  *a0 = vcombine_u16(vget_low_u16(vreinterpretq_u16_u32(c0.val[0])),
                     vget_low_u16(vreinterpretq_u16_u32(c2.val[0])));
  *a4 = vcombine_u16(vget_high_u16(vreinterpretq_u16_u32(c0.val[0])),
                     vget_high_u16(vreinterpretq_u16_u32(c2.val[0])));

  *a2 = vcombine_u16(vget_low_u16(vreinterpretq_u16_u32(c0.val[1])),
                     vget_low_u16(vreinterpretq_u16_u32(c2.val[1])));
  *a6 = vcombine_u16(vget_high_u16(vreinterpretq_u16_u32(c0.val[1])),
                     vget_high_u16(vreinterpretq_u16_u32(c2.val[1])));

  *a1 = vcombine_u16(vget_low_u16(vreinterpretq_u16_u32(c1.val[0])),
                     vget_low_u16(vreinterpretq_u16_u32(c3.val[0])));
  *a5 = vcombine_u16(vget_high_u16(vreinterpretq_u16_u32(c1.val[0])),
                     vget_high_u16(vreinterpretq_u16_u32(c3.val[0])));

  *a3 = vcombine_u16(vget_low_u16(vreinterpretq_u16_u32(c1.val[1])),
                     vget_low_u16(vreinterpretq_u16_u32(c3.val[1])));
  *a7 = vcombine_u16(vget_high_u16(vreinterpretq_u16_u32(c1.val[1])),
                     vget_high_u16(vreinterpretq_u16_u32(c3.val[1])));
}

#endif  // AV1_DSP_ARM_TRANSPOSE_NEON_H_
