/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

void ReferenceBuildNmvComponentCostTable(int *mvcost,
                                         const nmv_component *const mvcomp,
                                         MvSubpelPrecision precision) {
  int i, v;
  int sign_cost[2], class_cost[MV_CLASSES], class0_cost[CLASS0_SIZE];
  int bits_cost[MV_OFFSET_BITS][2];
  int class0_fp_cost[CLASS0_SIZE][MV_FP_SIZE], fp_cost[MV_FP_SIZE];
  int class0_hp_cost[2], hp_cost[2];
  av1_cost_tokens_from_cdf(sign_cost, mvcomp->sign_cdf, NULL);
  av1_cost_tokens_from_cdf(class_cost, mvcomp->classes_cdf, NULL);
  av1_cost_tokens_from_cdf(class0_cost, mvcomp->class0_cdf, NULL);
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    av1_cost_tokens_from_cdf(bits_cost[i], mvcomp->bits_cdf[i], NULL);
  }
  for (i = 0; i < CLASS0_SIZE; ++i)
    av1_cost_tokens_from_cdf(class0_fp_cost[i], mvcomp->class0_fp_cdf[i], NULL);
  av1_cost_tokens_from_cdf(fp_cost, mvcomp->fp_cdf, NULL);
  if (precision > MV_SUBPEL_LOW_PRECISION) {
    av1_cost_tokens_from_cdf(class0_hp_cost, mvcomp->class0_hp_cdf, NULL);
    av1_cost_tokens_from_cdf(hp_cost, mvcomp->hp_cdf, NULL);
  }
  mvcost[0] = 0;
  for (v = 1; v <= MV_MAX; ++v) {
    int z, c, o, d, e, f, cost = 0;
    z = v - 1;
    c = av1_get_mv_class(z, &o);
    cost += class_cost[c];
    d = (o >> 3);     /* int mv data */
    f = (o >> 1) & 3; /* fractional pel mv data */
    e = (o & 1);      /* high precision mv data */
    if (c == MV_CLASS_0) {
      cost += class0_cost[d];
    } else {
      const int b = c + CLASS0_BITS - 1; /* number of bits */
      for (i = 0; i < b; ++i) cost += bits_cost[i][((d >> i) & 1)];
    }
    if (precision > MV_SUBPEL_NONE) {
      if (c == MV_CLASS_0) {
        cost += class0_fp_cost[d][f];
      } else {
        cost += fp_cost[f];
      }
      if (precision > MV_SUBPEL_LOW_PRECISION) {
        if (c == MV_CLASS_0) {
          cost += class0_hp_cost[e];
        } else {
          cost += hp_cost[e];
        }
      }
    }
    mvcost[v] = cost + sign_cost[0];
    mvcost[-v] = cost + sign_cost[1];
  }
}

// Test using the default context, except for sign
static const nmv_component test_component_context = {
  { AOM_CDF11(28672, 30976, 31858, 32320, 32551, 32656, 32740, 32757, 32762,
              32767) },  // class_cdf // fp
  { { AOM_CDF4(16384, 24576, 26624) },
    { AOM_CDF4(12288, 21248, 24128) } },  // class0_fp_cdf
  { AOM_CDF4(8192, 17408, 21248) },       // fp_cdf
  { AOM_CDF2(70 * 128) },                 // sign_cdf
  { AOM_CDF2(160 * 128) },                // class0_hp_cdf
  { AOM_CDF2(128 * 128) },                // hp_cdf
  { AOM_CDF2(216 * 128) },                // class0_cdf
  { { AOM_CDF2(128 * 136) },
    { AOM_CDF2(128 * 140) },
    { AOM_CDF2(128 * 148) },
    { AOM_CDF2(128 * 160) },
    { AOM_CDF2(128 * 176) },
    { AOM_CDF2(128 * 192) },
    { AOM_CDF2(128 * 224) },
    { AOM_CDF2(128 * 234) },
    { AOM_CDF2(128 * 234) },
    { AOM_CDF2(128 * 240) } },  // bits_cdf
};

void TestMvComponentCostTable(MvSubpelPrecision precision) {
  int *mvcostRefBuf = new int[MV_VALS], *mvcostBuf = new int[MV_VALS];
  int *mvcostRef = mvcostRefBuf + MV_MAX, *mvcost = mvcostBuf + MV_MAX;

  ReferenceBuildNmvComponentCostTable(mvcostRef, &test_component_context,
                                      precision);
  av1_build_nmv_component_cost_table(mvcost, &test_component_context,
                                     precision);

  for (int v = 0; v <= MV_MAX; ++v) {
    ASSERT_EQ(mvcostRef[v], mvcost[-v]) << "v = " << v;
    ASSERT_EQ(mvcostRef[-v], mvcost[-v]) << "v = " << v;
  }

  delete[] mvcostRefBuf;
  delete[] mvcostBuf;
}

TEST(MvCostTest, BuildMvComponentCostTableTest1) {
  TestMvComponentCostTable(MV_SUBPEL_NONE);
}

TEST(MvCostTest, BuildMvComponentCostTableTest2) {
  TestMvComponentCostTable(MV_SUBPEL_LOW_PRECISION);
}

TEST(MvCostTest, BuildMvComponentCostTableTest3) {
  TestMvComponentCostTable(MV_SUBPEL_HIGH_PRECISION);
}

}  // namespace