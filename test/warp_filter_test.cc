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
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/warp_filter_test_util.h"
using libaom_test::ACMRandom;
#if CONFIG_EXT_WARP
using libaom_test::AV1ExtHighbdWarpFilter::AV1ExtHighbdWarpFilterTest;
using libaom_test::AV1ExtWarpFilter::AV1ExtWarpFilterTest;
#endif  // CONFIG_EXT_WARP
using libaom_test::AV1HighbdWarpFilter::AV1HighbdWarpFilterTest;
using libaom_test::AV1WarpFilter::AV1WarpFilterTest;
using ::testing::make_tuple;
using ::testing::tuple;

namespace {

TEST_P(AV1WarpFilterTest, CheckOutput) {
  RunCheckOutput(::testing::get<3>(GET_PARAM(0)));
}
TEST_P(AV1WarpFilterTest, DISABLED_Speed) {
  RunSpeedTest(::testing::get<3>(GET_PARAM(0)));
}

INSTANTIATE_TEST_CASE_P(
    C, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_c));

#if CONFIG_EXT_WARP
TEST_P(AV1ExtWarpFilterTest, CheckOutput) {
  RunCheckOutput(::testing::get<3>(GET_PARAM(0)));
}
TEST_P(AV1ExtWarpFilterTest, DISABLED_Speed) {
  RunSpeedTest(::testing::get<3>(GET_PARAM(0)));
}

INSTANTIATE_TEST_CASE_P(
    C, AV1ExtWarpFilterTest,
    libaom_test::AV1ExtWarpFilter::BuildParams(av1_ext_warp_affine_c));
#endif  // CONFIG_EXT_WARP

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_sse4_1));

TEST_P(AV1HighbdWarpFilterTest, CheckOutput) {
  RunCheckOutput(::testing::get<4>(GET_PARAM(0)));
}
TEST_P(AV1HighbdWarpFilterTest, DISABLED_Speed) {
  RunSpeedTest(::testing::get<4>(GET_PARAM(0)));
}

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdWarpFilterTest,
                        libaom_test::AV1HighbdWarpFilter::BuildParams(
                            av1_highbd_warp_affine_sse4_1));
#if CONFIG_EXT_WARP
TEST_P(AV1ExtHighbdWarpFilterTest, CheckOutput) {
  RunCheckOutput(::testing::get<4>(GET_PARAM(0)));
}
TEST_P(AV1ExtHighbdWarpFilterTest, DISABLED_Speed) {
  RunSpeedTest(::testing::get<4>(GET_PARAM(0)));
}

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1ExtHighbdWarpFilterTest,
                        libaom_test::AV1ExtHighbdWarpFilter::BuildParams(
                            av1_ext_highbd_warp_affine_sse4_1));
#endif  // CONFIG_EXT_WARP
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_avx2));
#if CONFIG_EXT_WARP
INSTANTIATE_TEST_CASE_P(
    AVX2, AV1ExtWarpFilterTest,
    libaom_test::AV1ExtWarpFilter::BuildParams(av1_ext_warp_affine_avx2));
#endif  // CONFIG_EXT_WARP
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_neon));
#endif  // HAVE_NEON

}  // namespace
