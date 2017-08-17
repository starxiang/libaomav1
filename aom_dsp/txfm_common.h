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

#ifndef AOM_DSP_TXFM_COMMON_H_
#define AOM_DSP_TXFM_COMMON_H_

#include "aom_dsp/aom_dsp_common.h"

// Constants and Macros used by all idct/dct functions
#define DCT_CONST_BITS 14
#define DCT_CONST_ROUNDING (1 << (DCT_CONST_BITS - 1))

#define UNIT_QUANT_SHIFT 2
#define UNIT_QUANT_FACTOR (1 << UNIT_QUANT_SHIFT)

typedef struct txfm_param {
  // for both forward and inverse transforms
  int tx_type;
  int tx_size;
  int lossless;
  int bd;
#if CONFIG_MRC_TX || CONFIG_LGT
  int is_inter;
#endif  // CONFIG_MRC_TX || CONFIG_LGT
#if CONFIG_MRC_TX || CONFIG_LGT_FROM_PRED
  int stride;
  uint8_t *dst;
#if CONFIG_MRC_TX
  int *valid_mask;
#endif  // CONFIG_MRC_TX
#if CONFIG_LGT_FROM_PRED
  int mode;
  int use_lgt;
#endif  // CONFIG_LGT_FROM_PRED
#endif  // CONFIG_MRC_TX || CONFIG_LGT_FROM_PRED
// for inverse transforms only
#if CONFIG_ADAPT_SCAN
  const int16_t *eob_threshold;
#endif
  int eob;
} TxfmParam;

// Constants:
//  for (int i = 1; i< 32; ++i)
//    printf("static const int cospi_%d_64 = %.0f;\n", i,
//           round(16384 * cos(i*M_PI/64)));
// Note: sin(k*Pi/64) = cos((32-k)*Pi/64)
static const tran_high_t cospi_1_64 = 16364;
static const tran_high_t cospi_2_64 = 16305;
static const tran_high_t cospi_3_64 = 16207;
static const tran_high_t cospi_4_64 = 16069;
static const tran_high_t cospi_5_64 = 15893;
static const tran_high_t cospi_6_64 = 15679;
static const tran_high_t cospi_7_64 = 15426;
static const tran_high_t cospi_8_64 = 15137;
static const tran_high_t cospi_9_64 = 14811;
static const tran_high_t cospi_10_64 = 14449;
static const tran_high_t cospi_11_64 = 14053;
static const tran_high_t cospi_12_64 = 13623;
static const tran_high_t cospi_13_64 = 13160;
static const tran_high_t cospi_14_64 = 12665;
static const tran_high_t cospi_15_64 = 12140;
static const tran_high_t cospi_16_64 = 11585;
static const tran_high_t cospi_17_64 = 11003;
static const tran_high_t cospi_18_64 = 10394;
static const tran_high_t cospi_19_64 = 9760;
static const tran_high_t cospi_20_64 = 9102;
static const tran_high_t cospi_21_64 = 8423;
static const tran_high_t cospi_22_64 = 7723;
static const tran_high_t cospi_23_64 = 7005;
static const tran_high_t cospi_24_64 = 6270;
static const tran_high_t cospi_25_64 = 5520;
static const tran_high_t cospi_26_64 = 4756;
static const tran_high_t cospi_27_64 = 3981;
static const tran_high_t cospi_28_64 = 3196;
static const tran_high_t cospi_29_64 = 2404;
static const tran_high_t cospi_30_64 = 1606;
static const tran_high_t cospi_31_64 = 804;

//  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
static const tran_high_t sinpi_1_9 = 5283;
static const tran_high_t sinpi_2_9 = 9929;
static const tran_high_t sinpi_3_9 = 13377;
static const tran_high_t sinpi_4_9 = 15212;

// 16384 * sqrt(2)
static const tran_high_t Sqrt2 = 23170;

static INLINE tran_high_t fdct_round_shift(tran_high_t input) {
  tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
  return rv;
}

#if CONFIG_LGT_FROM_PRED
// Use negative numbers so they do not coincide with lgt*[0][0], which are
// always nonnegative.
typedef enum {
  DCT4 = -1,
  ADST4 = -2,
  DCT8 = -3,
  ADST8 = -4,
  DCT16 = -5,
  ADST16 = -6,
  DCT32 = -7,
  ADST32 = -8,
} ButterflyLgt;

/* These are some LGTs already implementated in the codec. When any of them
 * is chosen, the flgt or ilgt function will call the existing fast
 * transform instead of the matrix product implementation. Thus, we
 * do not need the actual basis functions here */
static const tran_high_t lgt4_000[1][1] = { { (tran_high_t)DCT4 } };
static const tran_high_t lgt4_100[1][1] = { { (tran_high_t)ADST4 } };
static const tran_high_t lgt8_000[1][1] = { { (tran_high_t)DCT8 } };
static const tran_high_t lgt8_200[1][1] = { { (tran_high_t)ADST8 } };
static const tran_high_t lgt16_000[1][1] = { { (tran_high_t)DCT16 } };
static const tran_high_t lgt16_200[1][1] = { { (tran_high_t)ADST16 } };
static const tran_high_t lgt32_000[1][1] = { { (tran_high_t)DCT32 } };
static const tran_high_t lgt32_200[1][1] = { { (tran_high_t)ADST32 } };

/* The Line Graph Transforms (LGTs) matrices are written as follows.
   Each 2D array is sqrt(2)*16384 times an LGT matrix, which is the
   matrix of eigenvectors of the graph Laplacian matrix of the associated
   line graph. Some of those transforms have fast algorithms but not
   implemented yet for now. */

// LGT4 name: lgt4_150_000w3
// Self loops: 1.500, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000
static const tran_high_t lgt4_150_000w3[4][4] = {
  { 0, 0, 0, 23170 },
  { 5991, 13537, 17825, 0 },
  { 15515, 10788, -13408, 0 },
  { 16133, -15403, 6275, 0 },
};

// LGT4 name: lgt4_100_000w3
// Self loops: 1.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000
static const tran_high_t lgt4_100_000w3[4][4] = {
  { 0, 0, 0, 23170 },
  { 7600, 13694, 17076, 0 },
  { 17076, 7600, -13694, 0 },
  { 13694, -17076, 7600, 0 },
};

// LGT4 name: lgt4_060_000w3
// Self loops: 0.600, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000
static const tran_high_t lgt4_060_000w3[4][4] = {
  { 0, 0, 0, 23170 },
  { 9449, 13755, 16075, 0 },
  { 17547, 4740, -14370, 0 },
  { 11819, -18034, 8483, 0 },
};

// LGT4 name: lgt4_000w3
// Self loops: 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000
static const tran_high_t lgt4_000w3[4][4] = {
  { 0, 0, 0, 23170 },
  { 13377, 13377, 13377, 0 },
  { 16384, 0, -16384, 0 },
  { 9459, -18919, 9459, 0 },
};

// LGT4 name: lgt4_150_000w2
// Self loops: 1.500, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000
static const tran_high_t lgt4_150_000w2[4][4] = {
  { 10362, 20724, 0, 0 },
  { 20724, -10362, 0, 0 },
  { 0, 0, 16384, 16384 },
  { 0, 0, 16384, -16384 },
};

// LGT4 name: lgt4_100_000w2
// Self loops: 1.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000
static const tran_high_t lgt4_100_000w2[4][4] = {
  { 12181, 19710, 0, 0 },
  { 19710, -12181, 0, 0 },
  { 0, 0, 16384, 16384 },
  { 0, 0, 16384, -16384 },
};

// LGT4 name: lgt4_060_000w2
// Self loops: 0.600, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000
static const tran_high_t lgt4_060_000w2[4][4] = {
  { 13831, 18590, 0, 0 },
  { 18590, -13831, 0, 0 },
  { 0, 0, 16384, 16384 },
  { 0, 0, 16384, -16384 },
};

// LGT4 name: lgt4_000w2
// Self loops: 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000
static const tran_high_t lgt4_000w2[4][4] = {
  { 16384, 16384, 0, 0 },
  { 16384, -16384, 0, 0 },
  { 0, 0, 16384, 16384 },
  { 0, 0, 16384, -16384 },
};

// LGT4 name: lgt4_150_000w1
// Self loops: 1.500, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000
static const tran_high_t lgt4_150_000w1[4][4] = {
  { 23170, 0, 0, 0 },
  { 0, 13377, 13377, 13377 },
  { 0, 16384, 0, -16384 },
  { 0, 9459, -18919, 9459 },
};

// LGT4 name: lgt4_100_000w1
// Self loops: 1.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000
static const tran_high_t lgt4_100_000w1[4][4] = {
  { 23170, 0, 0, 0 },
  { 0, 13377, 13377, 13377 },
  { 0, 16384, 0, -16384 },
  { 0, 9459, -18919, 9459 },
};

// LGT4 name: lgt4_060_000w1
// Self loops: 0.600, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000
static const tran_high_t lgt4_060_000w1[4][4] = {
  { 23170, 0, 0, 0 },
  { 0, 13377, 13377, 13377 },
  { 0, 16384, 0, -16384 },
  { 0, 9459, -18919, 9459 },
};

// LGT4 name: lgt4_000w1
// Self loops: 0.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000
static const tran_high_t lgt4_000w1[4][4] = {
  { 23170, 0, 0, 0 },
  { 0, 13377, 13377, 13377 },
  { 0, 16384, 0, -16384 },
  { 0, 9459, -18919, 9459 },
};

// LGT4 name: lgt4_060
// Self loops: 0.600, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000
static const tran_high_t lgt4_060[4][4] = {
  { 6971, 10504, 13060, 14400 },
  { 14939, 11211, -2040, -13559 },
  { 14096, -8258, -12561, 10593 },
  { 8150, -15253, 14295, -5784 },
};

// LGT4 name: lgt4_150
// Self loops: 1.500, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000
static const tran_high_t lgt4_150[4][4] = {
  { 3998, 9435, 13547, 15759 },
  { 11106, 15105, 1886, -13483 },
  { 15260, -1032, -14674, 9361 },
  { 12833, -14786, 11596, -4372 },
};

// LGT8 name: lgt8_150_000w7
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 0.000
static const tran_high_t lgt8_150_000w7[8][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 32768 },
  { 2522, 6185, 9551, 12461, 14775, 16381, 17204, 0 },
  { 7390, 15399, 16995, 11515, 1240, -9551, -16365, 0 },
  { 11716, 16625, 3560, -13353, -15831, -1194, 14733, 0 },
  { 15073, 8866, -14291, -10126, 13398, 11308, -12401, 0 },
  { 16848, -4177, -13724, 14441, 2923, -16628, 9513, 0 },
  { 15942, -14888, 5405, 7137, -15640, 15288, -6281, 0 },
  { 10501, -14293, 16099, -15670, 13063, -8642, 3021, 0 },
};

// LGT8 name: lgt8_100_000w7
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 0.000
static const tran_high_t lgt8_100_000w7[8][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 32768 },
  { 3518, 6883, 9946, 12575, 14654, 16093, 16829, 0 },
  { 9946, 16093, 16093, 9946, 0, -9946, -16093, 0 },
  { 14654, 14654, 0, -14654, -14654, 0, 14654, 0 },
  { 16829, 3518, -16093, -6883, 14654, 9946, -12575, 0 },
  { 16093, -9946, -9946, 16093, 0, -16093, 9946, 0 },
  { 12575, -16829, 9946, 3518, -14654, 16093, -6883, 0 },
  { 6883, -12575, 16093, -16829, 14654, -9946, 3518, 0 },
};

// LGT8 name: lgt8_060_000w7
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 0.000
static const tran_high_t lgt8_060_000w7[8][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 32768 },
  { 5087, 7951, 10521, 12701, 14411, 15587, 16186, 0 },
  { 13015, 16486, 14464, 7621, -1762, -10557, -15834, 0 },
  { 16581, 11475, -4050, -15898, -13311, 1362, 14798, 0 },
  { 16536, -1414, -16981, -3927, 15746, 8879, -12953, 0 },
  { 14104, -13151, -7102, 16932, -1912, -15914, 10385, 0 },
  { 10156, -17168, 11996, 1688, -14174, 16602, -7249, 0 },
  { 5295, -11721, 15961, -17224, 15274, -10476, 3723, 0 },
};

// LGT8 name: lgt8_000w7
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 0.000
static const tran_high_t lgt8_000w7[8][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 32768 },
  { 12385, 12385, 12385, 12385, 12385, 12385, 12385, 0 },
  { 17076, 13694, 7600, 0, -7600, -13694, -17076, 0 },
  { 15781, 3898, -10921, -17515, -10921, 3898, 15781, 0 },
  { 13694, -7600, -17076, 0, 17076, 7600, -13694, 0 },
  { 10921, -15781, -3898, 17515, -3898, -15781, 10921, 0 },
  { 7600, -17076, 13694, 0, -13694, 17076, -7600, 0 },
  { 3898, -10921, 15781, -17515, 15781, -10921, 3898, 0 },
};

// LGT8 name: lgt8_150_000w6
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 0.000, 1.000
static const tran_high_t lgt8_150_000w6[8][8] = {
  { 0, 0, 0, 0, 0, 0, 23170, 23170 },
  { 0, 0, 0, 0, 0, 0, 23170, -23170 },
  { 3157, 7688, 11723, 15002, 17312, 18506, 0, 0 },
  { 9167, 17832, 16604, 6164, -7696, -17286, 0, 0 },
  { 14236, 15584, -4969, -18539, -6055, 14938, 0, 0 },
  { 17558, 1891, -18300, 5288, 16225, -11653, 0, 0 },
  { 17776, -13562, -647, 14380, -17514, 7739, 0, 0 },
  { 12362, -16318, 17339, -15240, 10399, -3688, 0, 0 },
};

// LGT8 name: lgt8_100_000w6
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 0.000, 1.000
static const tran_high_t lgt8_100_000w6[8][8] = {
  { 0, 0, 0, 0, 0, 0, 23170, 23170 },
  { 0, 0, 0, 0, 0, 0, 23170, -23170 },
  { 4350, 8447, 12053, 14959, 16995, 18044, 0, 0 },
  { 12053, 18044, 14959, 4350, -8447, -16995, 0, 0 },
  { 16995, 12053, -8447, -18044, -4350, 14959, 0, 0 },
  { 18044, -4350, -16995, 8447, 14959, -12053, 0, 0 },
  { 14959, -16995, 4350, 12053, -18044, 8447, 0, 0 },
  { 8447, -14959, 18044, -16995, 12053, -4350, 0, 0 },
};

// LGT8 name: lgt8_060_000w6
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 0.000, 1.000
static const tran_high_t lgt8_060_000w6[8][8] = {
  { 0, 0, 0, 0, 0, 0, 23170, 23170 },
  { 0, 0, 0, 0, 0, 0, 23170, -23170 },
  { 6154, 9551, 12487, 14823, 16446, 17277, 0, 0 },
  { 15149, 17660, 12503, 1917, -9502, -16795, 0, 0 },
  { 18166, 7740, -11772, -17465, -2656, 15271, 0, 0 },
  { 16682, -8797, -15561, 10779, 14189, -12586, 0, 0 },
  { 12436, -18234, 7007, 10763, -18483, 8945, 0, 0 },
  { 6591, -14172, 18211, -17700, 12766, -4642, 0, 0 },
};

// LGT8 name: lgt8_000w6
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 0.000, 1.000
static const tran_high_t lgt8_000w6[8][8] = {
  { 0, 0, 0, 0, 0, 0, 23170, 23170 },
  { 0, 0, 0, 0, 0, 0, 23170, -23170 },
  { 13377, 13377, 13377, 13377, 13377, 13377, 0, 0 },
  { 18274, 13377, 4896, -4896, -13377, -18274, 0, 0 },
  { 16384, 0, -16384, -16384, 0, 16384, 0, 0 },
  { 13377, -13377, -13377, 13377, 13377, -13377, 0, 0 },
  { 9459, -18919, 9459, 9459, -18919, 9459, 0, 0 },
  { 4896, -13377, 18274, -18274, 13377, -4896, 0, 0 },
};

// LGT8 name: lgt8_150_000w5
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 0.000, 1.000, 1.000
static const tran_high_t lgt8_150_000w5[8][8] = {
  { 0, 0, 0, 0, 0, 18919, 18919, 18919 },
  { 0, 0, 0, 0, 0, 23170, 0, -23170 },
  { 0, 0, 0, 0, 0, 13377, -26755, 13377 },
  { 4109, 9895, 14774, 18299, 20146, 0, 0, 0 },
  { 11753, 20300, 13161, -4148, -18252, 0, 0, 0 },
  { 17573, 10921, -16246, -12895, 14679, 0, 0, 0 },
  { 19760, -9880, -9880, 19760, -9880, 0, 0, 0 },
  { 14815, -18624, 17909, -12844, 4658, 0, 0, 0 },
};

// LGT8 name: lgt8_100_000w5
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 0.000, 1.000, 1.000
static const tran_high_t lgt8_100_000w5[8][8] = {
  { 0, 0, 0, 0, 0, 18919, 18919, 18919 },
  { 0, 0, 0, 0, 0, 23170, 0, -23170 },
  { 0, 0, 0, 0, 0, 13377, -26755, 13377 },
  { 5567, 10683, 14933, 17974, 19559, 0, 0, 0 },
  { 14933, 19559, 10683, -5567, -17974, 0, 0, 0 },
  { 19559, 5567, -17974, -10683, 14933, 0, 0, 0 },
  { 17974, -14933, -5567, 19559, -10683, 0, 0, 0 },
  { 10683, -17974, 19559, -14933, 5567, 0, 0, 0 },
};

// LGT8 name: lgt8_060_000w5
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 0.000, 1.000, 1.000
static const tran_high_t lgt8_060_000w5[8][8] = {
  { 0, 0, 0, 0, 0, 18919, 18919, 18919 },
  { 0, 0, 0, 0, 0, 23170, 0, -23170 },
  { 0, 0, 0, 0, 0, 13377, -26755, 13377 },
  { 7650, 11741, 15069, 17415, 18628, 0, 0, 0 },
  { 17824, 18002, 7558, -7345, -17914, 0, 0, 0 },
  { 19547, 569, -19303, -8852, 15505, 0, 0, 0 },
  { 15592, -17548, -2862, 19625, -11374, 0, 0, 0 },
  { 8505, -17423, 20218, -15907, 6006, 0, 0, 0 },
};

// LGT8 name: lgt8_000w5
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 0.000, 1.000, 1.000
static const tran_high_t lgt8_000w5[8][8] = {
  { 0, 0, 0, 0, 0, 18919, 18919, 18919 },
  { 0, 0, 0, 0, 0, 23170, 0, -23170 },
  { 0, 0, 0, 0, 0, 13377, -26755, 13377 },
  { 14654, 14654, 14654, 14654, 14654, 0, 0, 0 },
  { 19710, 12181, 0, -12181, -19710, 0, 0, 0 },
  { 16766, -6404, -20724, -6404, 16766, 0, 0, 0 },
  { 12181, -19710, 0, 19710, -12181, 0, 0, 0 },
  { 6404, -16766, 20724, -16766, 6404, 0, 0, 0 },
};

// LGT8 name: lgt8_150_000w4
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 0.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_150_000w4[8][8] = {
  { 5655, 13343, 19159, 22286, 0, 0, 0, 0 },
  { 15706, 21362, 2667, -19068, 0, 0, 0, 0 },
  { 21580, -1459, -20752, 13238, 0, 0, 0, 0 },
  { 18148, -20910, 16399, -6183, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 16384, 16384, 16384, 16384 },
  { 0, 0, 0, 0, 21407, 8867, -8867, -21407 },
  { 0, 0, 0, 0, 16384, -16384, -16384, 16384 },
  { 0, 0, 0, 0, 8867, -21407, 21407, -8867 },
};

// LGT8 name: lgt8_100_000w4
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 0.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_100_000w4[8][8] = {
  { 7472, 14042, 18919, 21513, 0, 0, 0, 0 },
  { 18919, 18919, 0, -18919, 0, 0, 0, 0 },
  { 21513, -7472, -18919, 14042, 0, 0, 0, 0 },
  { 14042, -21513, 18919, -7472, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 16384, 16384, 16384, 16384 },
  { 0, 0, 0, 0, 21407, 8867, -8867, -21407 },
  { 0, 0, 0, 0, 16384, -16384, -16384, 16384 },
  { 0, 0, 0, 0, 8867, -21407, 21407, -8867 },
};

// LGT8 name: lgt8_060_000w4
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 0.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_060_000w4[8][8] = {
  { 9858, 14855, 18470, 20365, 0, 0, 0, 0 },
  { 21127, 15855, -2886, -19175, 0, 0, 0, 0 },
  { 19935, -11679, -17764, 14980, 0, 0, 0, 0 },
  { 11525, -21570, 20217, -8180, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 16384, 16384, 16384, 16384 },
  { 0, 0, 0, 0, 21407, 8867, -8867, -21407 },
  { 0, 0, 0, 0, 16384, -16384, -16384, 16384 },
  { 0, 0, 0, 0, 8867, -21407, 21407, -8867 },
};

// LGT8 name: lgt8_000w4
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 0.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_000w4[8][8] = {
  { 16384, 16384, 16384, 16384, 0, 0, 0, 0 },
  { 21407, 8867, -8867, -21407, 0, 0, 0, 0 },
  { 16384, -16384, -16384, 16384, 0, 0, 0, 0 },
  { 8867, -21407, 21407, -8867, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 16384, 16384, 16384, 16384 },
  { 0, 0, 0, 0, 21407, 8867, -8867, -21407 },
  { 0, 0, 0, 0, 16384, -16384, -16384, 16384 },
  { 0, 0, 0, 0, 8867, -21407, 21407, -8867 },
};

// LGT8 name: lgt8_150_000w3
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_150_000w3[8][8] = {
  { 8473, 19144, 25209, 0, 0, 0, 0, 0 },
  { 21942, 15257, -18961, 0, 0, 0, 0, 0 },
  { 22815, -21783, 8874, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 14654, 14654, 14654, 14654, 14654 },
  { 0, 0, 0, 19710, 12181, 0, -12181, -19710 },
  { 0, 0, 0, 16766, -6404, -20724, -6404, 16766 },
  { 0, 0, 0, 12181, -19710, 0, 19710, -12181 },
  { 0, 0, 0, 6404, -16766, 20724, -16766, 6404 },
};

// LGT8 name: lgt8_100_000w3
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_100_000w3[8][8] = {
  { 10747, 19366, 24149, 0, 0, 0, 0, 0 },
  { 24149, 10747, -19366, 0, 0, 0, 0, 0 },
  { 19366, -24149, 10747, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 14654, 14654, 14654, 14654, 14654 },
  { 0, 0, 0, 19710, 12181, 0, -12181, -19710 },
  { 0, 0, 0, 16766, -6404, -20724, -6404, 16766 },
  { 0, 0, 0, 12181, -19710, 0, 19710, -12181 },
  { 0, 0, 0, 6404, -16766, 20724, -16766, 6404 },
};

// LGT8 name: lgt8_060_000w3
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_060_000w3[8][8] = {
  { 13363, 19452, 22733, 0, 0, 0, 0, 0 },
  { 24815, 6704, -20323, 0, 0, 0, 0, 0 },
  { 16715, -25503, 11997, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 14654, 14654, 14654, 14654, 14654 },
  { 0, 0, 0, 19710, 12181, 0, -12181, -19710 },
  { 0, 0, 0, 16766, -6404, -20724, -6404, 16766 },
  { 0, 0, 0, 12181, -19710, 0, 19710, -12181 },
  { 0, 0, 0, 6404, -16766, 20724, -16766, 6404 },
};

// LGT8 name: lgt8_000w3
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 0.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_000w3[8][8] = {
  { 18919, 18919, 18919, 0, 0, 0, 0, 0 },
  { 23170, 0, -23170, 0, 0, 0, 0, 0 },
  { 13377, -26755, 13377, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 14654, 14654, 14654, 14654, 14654 },
  { 0, 0, 0, 19710, 12181, 0, -12181, -19710 },
  { 0, 0, 0, 16766, -6404, -20724, -6404, 16766 },
  { 0, 0, 0, 12181, -19710, 0, 19710, -12181 },
  { 0, 0, 0, 6404, -16766, 20724, -16766, 6404 },
};

// LGT8 name: lgt8_150_000w2
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_150_000w2[8][8] = {
  { 14654, 29309, 0, 0, 0, 0, 0, 0 },
  { 29309, -14654, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 13377, 13377, 13377, 13377, 13377, 13377 },
  { 0, 0, 18274, 13377, 4896, -4896, -13377, -18274 },
  { 0, 0, 16384, 0, -16384, -16384, 0, 16384 },
  { 0, 0, 13377, -13377, -13377, 13377, 13377, -13377 },
  { 0, 0, 9459, -18919, 9459, 9459, -18919, 9459 },
  { 0, 0, 4896, -13377, 18274, -18274, 13377, -4896 },
};

// LGT8 name: lgt8_100_000w2
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_100_000w2[8][8] = {
  { 17227, 27874, 0, 0, 0, 0, 0, 0 },
  { 27874, -17227, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 13377, 13377, 13377, 13377, 13377, 13377 },
  { 0, 0, 18274, 13377, 4896, -4896, -13377, -18274 },
  { 0, 0, 16384, 0, -16384, -16384, 0, 16384 },
  { 0, 0, 13377, -13377, -13377, 13377, 13377, -13377 },
  { 0, 0, 9459, -18919, 9459, 9459, -18919, 9459 },
  { 0, 0, 4896, -13377, 18274, -18274, 13377, -4896 },
};

// LGT8 name: lgt8_060_000w2
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_060_000w2[8][8] = {
  { 19560, 26290, 0, 0, 0, 0, 0, 0 },
  { 26290, -19560, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 13377, 13377, 13377, 13377, 13377, 13377 },
  { 0, 0, 18274, 13377, 4896, -4896, -13377, -18274 },
  { 0, 0, 16384, 0, -16384, -16384, 0, 16384 },
  { 0, 0, 13377, -13377, -13377, 13377, 13377, -13377 },
  { 0, 0, 9459, -18919, 9459, 9459, -18919, 9459 },
  { 0, 0, 4896, -13377, 18274, -18274, 13377, -4896 },
};

// LGT8 name: lgt8_000w2
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 0.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_000w2[8][8] = {
  { 23170, 23170, 0, 0, 0, 0, 0, 0 },
  { 23170, -23170, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 13377, 13377, 13377, 13377, 13377, 13377 },
  { 0, 0, 18274, 13377, 4896, -4896, -13377, -18274 },
  { 0, 0, 16384, 0, -16384, -16384, 0, 16384 },
  { 0, 0, 13377, -13377, -13377, 13377, 13377, -13377 },
  { 0, 0, 9459, -18919, 9459, 9459, -18919, 9459 },
  { 0, 0, 4896, -13377, 18274, -18274, 13377, -4896 },
};

// LGT8 name: lgt8_150_000w1
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_150_000w1[8][8] = {
  { 32768, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 12385, 12385, 12385, 12385, 12385, 12385, 12385 },
  { 0, 17076, 13694, 7600, 0, -7600, -13694, -17076 },
  { 0, 15781, 3898, -10921, -17515, -10921, 3898, 15781 },
  { 0, 13694, -7600, -17076, 0, 17076, 7600, -13694 },
  { 0, 10921, -15781, -3898, 17515, -3898, -15781, 10921 },
  { 0, 7600, -17076, 13694, 0, -13694, 17076, -7600 },
  { 0, 3898, -10921, 15781, -17515, 15781, -10921, 3898 },
};

// LGT8 name: lgt8_100_000w1
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_100_000w1[8][8] = {
  { 32768, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 12385, 12385, 12385, 12385, 12385, 12385, 12385 },
  { 0, 17076, 13694, 7600, 0, -7600, -13694, -17076 },
  { 0, 15781, 3898, -10921, -17515, -10921, 3898, 15781 },
  { 0, 13694, -7600, -17076, 0, 17076, 7600, -13694 },
  { 0, 10921, -15781, -3898, 17515, -3898, -15781, 10921 },
  { 0, 7600, -17076, 13694, 0, -13694, 17076, -7600 },
  { 0, 3898, -10921, 15781, -17515, 15781, -10921, 3898 },
};

// LGT8 name: lgt8_060_000w1
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_060_000w1[8][8] = {
  { 32768, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 12385, 12385, 12385, 12385, 12385, 12385, 12385 },
  { 0, 17076, 13694, 7600, 0, -7600, -13694, -17076 },
  { 0, 15781, 3898, -10921, -17515, -10921, 3898, 15781 },
  { 0, 13694, -7600, -17076, 0, 17076, 7600, -13694 },
  { 0, 10921, -15781, -3898, 17515, -3898, -15781, 10921 },
  { 0, 7600, -17076, 13694, 0, -13694, 17076, -7600 },
  { 0, 3898, -10921, 15781, -17515, 15781, -10921, 3898 },
};

// LGT8 name: lgt8_000w1
// Self loops: 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 0.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_000w1[8][8] = {
  { 32768, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 12385, 12385, 12385, 12385, 12385, 12385, 12385 },
  { 0, 17076, 13694, 7600, 0, -7600, -13694, -17076 },
  { 0, 15781, 3898, -10921, -17515, -10921, 3898, 15781 },
  { 0, 13694, -7600, -17076, 0, 17076, 7600, -13694 },
  { 0, 10921, -15781, -3898, 17515, -3898, -15781, 10921 },
  { 0, 7600, -17076, 13694, 0, -13694, 17076, -7600 },
  { 0, 3898, -10921, 15781, -17515, 15781, -10921, 3898 },
};

// LGT8 name: lgt8_060
// Self loops: 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_060[8][8] = {
  { 4295, 6746, 8999, 10987, 12653, 13947, 14832, 15280 },
  { 11303, 15101, 14912, 10786, 3812, -4168, -11047, -15010 },
  { 15051, 13208, 1823, -10879, -15721, -9207, 3959, 14265 },
  { 15871, 3800, -13441, -12395, 5516, 15922, 4665, -12939 },
  { 14630, -7269, -13926, 8618, 13091, -9886, -12133, 11062 },
  { 12008, -14735, 180, 14586, -12245, -4458, 15932, -8720 },
  { 8472, -15623, 14088, -4721, -7272, 15221, -14708, 6018 },
  { 4372, -9862, 13927, -15981, 15727, -13202, 8770, -3071 },
};

// LGT8 name: lgt8_100
// Self loops: 1.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_100[8][8] = {
  { 2921, 5742, 8368, 10708, 12684, 14228, 15288, 15827 },
  { 8368, 14228, 15827, 12684, 5742, -2921, -10708, -15288 },
  { 12684, 15288, 5742, -8368, -15827, -10708, 2921, 14228 },
  { 15288, 8368, -10708, -14228, 2921, 15827, 5742, -12684 },
  { 15827, -2921, -15288, 5742, 14228, -8368, -12684, 10708 },
  { 14228, -12684, -2921, 15288, -10708, -5742, 15827, -8368 },
  { 10708, -15827, 12684, -2921, -8368, 15288, -14228, 5742 },
  { 5742, -10708, 14228, -15827, 15288, -12684, 8368, -2921 },
};
#endif  // CONFIG_LGT_FROM_PRED

#if CONFIG_LGT || CONFIG_LGT_FROM_PRED
// LGT4 name: lgt4_170
// Self loops: 1.700, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000
static const tran_high_t lgt4_170[4][4] = {
  { 3636, 9287, 13584, 15902 },
  { 10255, 15563, 2470, -13543 },
  { 14786, 711, -15249, 9231 },
  { 14138, -14420, 10663, -3920 },
};

// LGT4 name: lgt4_140
// Self loops: 1.400, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000
static const tran_high_t lgt4_140[4][4] = {
  { 4206, 9518, 13524, 15674 },
  { 11552, 14833, 1560, -13453 },
  { 15391, -1906, -14393, 9445 },
  { 12201, -14921, 12016, -4581 },
};

// LGT8 name: lgt8_170
// Self loops: 1.700, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_170[8][8] = {
  { 1858, 4947, 7850, 10458, 12672, 14411, 15607, 16217 },
  { 5494, 13022, 16256, 14129, 7343, -1864, -10456, -15601 },
  { 8887, 16266, 9500, -5529, -15749, -12273, 1876, 14394 },
  { 11870, 13351, -6199, -15984, -590, 15733, 7273, -12644 },
  { 14248, 5137, -15991, 291, 15893, -5685, -13963, 10425 },
  { 15716, -5450, -10010, 15929, -6665, -8952, 16036, -7835 },
  { 15533, -13869, 6559, 3421, -12009, 15707, -13011, 5018 },
  { 11357, -13726, 14841, -14600, 13025, -10259, 6556, -2254 },
};

// LGT8 name: lgt8_150
// Self loops: 1.500, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000
// Edges: 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000
static const tran_high_t lgt8_150[8][8] = {
  { 2075, 5110, 7958, 10511, 12677, 14376, 15544, 16140 },
  { 6114, 13307, 16196, 13845, 7015, -2084, -10509, -15534 },
  { 9816, 16163, 8717, -6168, -15790, -11936, 2104, 14348 },
  { 12928, 12326, -7340, -15653, 242, 15763, 6905, -12632 },
  { 15124, 3038, -16033, 1758, 15507, -6397, -13593, 10463 },
  { 15895, -7947, -7947, 15895, -7947, -7947, 15895, -7947 },
  { 14325, -15057, 9030, 1050, -10659, 15483, -13358, 5236 },
  { 9054, -12580, 14714, -15220, 14043, -11312, 7330, -2537 },
};
#endif  // CONFIG_LGT || CONFIG_FROM_PRED
#endif  // AOM_DSP_TXFM_COMMON_H_
