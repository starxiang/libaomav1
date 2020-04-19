/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <set>
#include <vector>
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

// All single reference convolve tests are parameterized on block size,
// bit-depth, and function to test.
//
// Note that parameterizing on these variables (and not other parameters) is
// a conscious decision - Jenkins needs some degree of parallelization to run
// the tests within the time limit, but if the number of parameters increases
// too much, the gtest framework does not handle it well (increased overhead per
// test, huge amount of output to stdout, etc.).
//
// Also note that the test suites must be named with the architecture, e.g.,
// C, C_X, AVX2_X, ... The test suite that runs on Jenkins sometimes runs tests
// that cannot deal with intrinsics (e.g., the Valgrind tests) and will disable
// tests using a filter like --gtest_filter=-:SSE4_1.*. If the test suites are
// not named this way, the testing infrastructure will not selectively filter
// them properly.
class BlockSize {
 public:
  BlockSize(int w, int h) : width_(w), height_(h) {}

  int Width() const { return width_; }
  int Height() const { return height_; }

  bool operator<(const BlockSize &other) const {
    if (Width() == other.Width()) {
      return Height() < other.Height();
    }
    return Width() < other.Width();
  }

  bool operator==(const BlockSize &other) const {
    return Width() == other.Width() && Height() == other.Height();
  }

 private:
  int width_;
  int height_;
};

// Block size / bit depth / test function used to parameterize the tests.
template <typename T>
class TestParam {
 public:
  TestParam(const BlockSize &block, int bd, T test_func)
      : block_(block), bd_(bd), test_func_(test_func) {}

  const BlockSize &Block() const { return block_; }
  int BitDepth() const { return bd_; }
  const T TestFunction() const { return test_func_; }

  bool operator==(const TestParam &other) const {
    return Block() == other.Block() && BitDepth() == other.BitDepth() &&
           TestFunction() == other.TestFunction();
  }

 private:
  BlockSize block_;
  int bd_;
  T test_func_;
};

// Generate the list of all block widths / heights that need to be tested,
// includes chroma and luma sizes, for the given bit-depths. The test
// function is the same for all generated parameters.
template <typename T>
std::vector<TestParam<T>> GetTestParams(std::initializer_list<int> bit_depths,
                                        T test_func) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
    // Add in smaller chroma sizes as well.
    if (w == 4 || h == 4) {
      sizes.insert(BlockSize(w / 2, h / 2));
    }
  }
  std::vector<TestParam<T>> result;
  for (const BlockSize &block : sizes) {
    for (int bd : bit_depths) {
      result.push_back(TestParam<T>(block, bd, test_func));
    }
  }
  return result;
}

template <typename T>
std::vector<TestParam<T>> GetLowbdTestParams(T test_func) {
  return GetTestParams({ 8 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildLowbdParams(
    T test_func) {
  return ::testing::ValuesIn(GetLowbdTestParams(test_func));
}

// Test the test-parameters generators work as expected.
class AV1ConvolveParametersTest : public ::testing::Test {};

TEST_F(AV1ConvolveParametersTest, GetLowbdTestParams) {
  auto v = GetLowbdTestParams(nullptr);
  ASSERT_EQ(27U, v.size());
  for (const auto &p : v) {
    ASSERT_EQ(8, p.BitDepth());
    ASSERT_EQ(nullptr, p.TestFunction());
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
template <typename T>
std::vector<TestParam<T>> GetHighbdTestParams(T test_func) {
  return GetTestParams({ 10, 12 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildHighbdParams(
    T test_func) {
  return ::testing::ValuesIn(GetHighbdTestParams(test_func));
}

TEST_F(AV1ConvolveParametersTest, GetHighbdTestParams) {
  auto v = GetHighbdTestParams(nullptr);
  ASSERT_EQ(54U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &p : v) {
    ASSERT_TRUE(p.BitDepth() == 10 || p.BitDepth() == 12);
    ASSERT_EQ(nullptr, p.TestFunction());
    if (p.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// AV1ConvolveTest is the base class that all convolve tests should derive from.
// It provides storage/methods for generating randomized buffers for both
// low bit-depth and high bit-depth, and setup/teardown methods for clearing
// system state. Implementors can get the bit-depth / block-size /
// test function by calling GetParam().
template <typename T>
class AV1ConvolveTest : public ::testing::TestWithParam<TestParam<T>> {
 public:
  virtual ~AV1ConvolveTest() { TearDown(); }

  virtual void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

  virtual void TearDown() override { libaom_test::ClearSystemState(); }

  // Randomizes the 8-bit input buffer and returns a pointer to it. Note that
  // the pointer is safe to use with an 8-tap filter. The stride can range
  // from width to (width + kPadding). Also note that the pointer is to the
  // same memory location.
  static constexpr int kInputPadding = 8;

  const uint8_t *FirstRandomInput8(int bit_depth) {
    // Note we can't call GetParam() directly -- gtest does not support
    // this for parameterized types.
    // Check that this is only called with low bit-depth.
    EXPECT_EQ(8, bit_depth);
    Randomize(input8_1_);
    return input8_1_ + 3 * kInputStride + 3;
  }

  const uint8_t *SecondRandomInput8(int bit_depth) {
    EXPECT_EQ(8, bit_depth);
    Randomize(input8_2_);
    return input8_2_ + 3 * kInputStride + 3;
  }

  // A buffer that can handle blocks of width or height of MAX_SB_SIZE.
  // Note that the pointer is always the same, and it is guaranteed to be
  // aligned on a 32-byte boundary.
  CONV_BUF_TYPE *FirstConvolveBuffer() { return conv_buf_1_; }
  CONV_BUF_TYPE *SecondConvolveBuffer() { return conv_buf_2_; }

  // Some of the intrinsics perform writes in 32 byte chunks. Moreover, some
  // of the instrinsics assume that the stride is also a multiple of 32.
  // As such, the stride is always MAX_SB_SIZE.
  static constexpr int kOutputStride = MAX_SB_SIZE;

  // Note that output buffers are always aligned on 32-byte boundaries and
  // can handle blocks up to MAX_SB_SIZE. The stride is kOutputStride.
  // Note that the functions always return the same pointers (this optimization
  // is because Jenkins runs the tests in a low memory environment).
  uint8_t *FirstOutput8() { return output8_1_; }
  uint8_t *SecondOutput8() { return output8_2_; }

  // Check that two 8-bit output buffers are identical.
  void AssertOutputBufferEq(const uint8_t *p1, const uint8_t *p2, int width,
                            int height) {
    ASSERT_TRUE(p1 != p2) << "Buffers must be at different memory locations";
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

  // Check that two 16-bit output buffers are identical.
  void AssertOutputBufferEq(const uint16_t *p1, const uint16_t *p2, int width,
                            int height) {
    ASSERT_TRUE(p1 != p2) << "Buffers must be in different memory locations";
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

#if CONFIG_AV1_HIGHBITDEPTH
  // Note that the randomized values are capped by bit-depth.
  const uint16_t *FirstRandomInput16(int bit_depth) {
    // Check that this is only called with high bit-depths.
    EXPECT_TRUE(bit_depth == 10 || bit_depth == 12);
    Randomize(input16_1_, bit_depth);
    return input16_1_ + 3 * kInputStride + 3;
  }

  const uint16_t *SecondRandomInput16(int bit_depth) {
    EXPECT_TRUE(bit_depth == 10 || bit_depth == 12);
    Randomize(input16_2_, bit_depth);
    return input16_2_ + 3 * kInputStride + 3;
  }

  uint16_t *FirstOutput16() { return output16_1_; }
  uint16_t *SecondOutput16() { return output16_2_; }
#endif

 private:
  void Randomize(uint8_t *p) {
    const int size = kInputStride * kInputStride;
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand8();
    }
  }

#if CONFIG_AV1_HIGHBITDEPTH
  void Randomize(uint16_t *p, int bit_depth) {
    const int size = kInputStride * kInputStride;
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand16() & ((1 << bit_depth) - 1);
    }
  }
#endif

  static constexpr int kInputStride = MAX_SB_SIZE + kInputPadding;

  libaom_test::ACMRandom rnd_;
  // Statically allocate all the memory that is needed for the tests.
  uint8_t input8_1_[kInputStride * kInputStride];
  uint8_t input8_2_[kInputStride * kInputStride];
  DECLARE_ALIGNED(32, uint8_t, output8_1_[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, output8_2_[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, CONV_BUF_TYPE, conv_buf_1_[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, CONV_BUF_TYPE, conv_buf_2_[MAX_SB_SQUARE]);

#if CONFIG_AV1_HIGHBITDEPTH
  uint16_t
      input16_1_[(MAX_SB_SIZE + kInputPadding) * (MAX_SB_SIZE + kInputPadding)];
  uint16_t
      input16_2_[(MAX_SB_SIZE + kInputPadding) * (MAX_SB_SIZE + kInputPadding)];
  DECLARE_ALIGNED(32, uint16_t, output16_1_[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint16_t, output16_2_[MAX_SB_SQUARE]);
#endif
};

////////////////////////////////////////////////////////
// Single reference convolve-x functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_x_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_x,
                                const int subpel_x_qn,
                                ConvolveParams *conv_params);

class AV1ConvolveXTest : public AV1ConvolveTest<convolve_x_func> {
 public:
  void RunTest() {
    for (int sub_x = 0; sub_x < 16; ++sub_x) {
      for (int filter = EIGHTTAP_REGULAR; filter < INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_x, f);
      }
    }
  }

 private:
  void TestConvolve(const int sub_x, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    const uint8_t *input = FirstRandomInput8(GetParam().BitDepth());
    uint8_t *reference = FirstOutput8();
    av1_convolve_x_sr(input, width, reference, kOutputStride, width, height,
                      filter_params_x, sub_x, &conv_params1);

    ConvolveParams conv_params2 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    uint8_t *test = SecondOutput8();
    convolve_x_func test_func = GetParam().TestFunction();
    test_func(input, width, test, kOutputStride, width, height, filter_params_x,
              sub_x, &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1ConvolveXTest, RunTest) { RunTest(); }
INSTANTIATE_TEST_SUITE_P(C_X, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_X, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_X, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_X, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-x functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_x_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd);

class AV1HighbdConvolveXTest : public AV1ConvolveTest<highbd_convolve_x_func> {
 public:
  void RunTest() {
    for (int sub_x = 0; sub_x < 16; ++sub_x) {
      for (int filter = EIGHTTAP_REGULAR; filter < INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_x, f);
      }
    }
  }

 private:
  void TestConvolve(const int sub_x, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    const uint16_t *input = FirstRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    av1_highbd_convolve_x_sr(input, width, reference, kOutputStride, width,
                             height, filter_params_x, sub_x, &conv_params1,
                             bit_depth);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    uint16_t *test = SecondOutput16();
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, sub_x, &conv_params2, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1HighbdConvolveXTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_X, AV1HighbdConvolveXTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3_X, AV1HighbdConvolveXTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_X, AV1HighbdConvolveXTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////////////
// Single reference convolve-y functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_y_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_y,
                                const int subpel_y_qn);

class AV1ConvolveYTest : public AV1ConvolveTest<convolve_y_func> {
 public:
  void RunTest() {
    for (int sub_y = 0; sub_y < 16; ++sub_y) {
      for (int filter = EIGHTTAP_REGULAR; filter < INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_y, f);
      }
    }
  }

 private:
  void TestConvolve(const int sub_y, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, height);
    const uint8_t *input = FirstRandomInput8(GetParam().BitDepth());
    uint8_t *reference = FirstOutput8();
    av1_convolve_y_sr(input, width, reference, kOutputStride, width, height,
                      filter_params_y, sub_y);
    uint8_t *test = SecondOutput8();
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_y, sub_y);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1ConvolveYTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_Y, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_Y, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_Y, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_Y, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-y functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_y_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    int bd);

class AV1HighbdConvolveYTest : public AV1ConvolveTest<highbd_convolve_y_func> {
 public:
  void RunTest() {
    for (int sub_y = 0; sub_y < 16; ++sub_y) {
      for (int filter = EIGHTTAP_REGULAR; filter < INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_y, f);
      }
    }
  }

 private:
  void TestConvolve(const int sub_y, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, height);
    const uint16_t *input = FirstRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    av1_highbd_convolve_y_sr(input, width, reference, kOutputStride, width,
                             height, filter_params_y, sub_y, bit_depth);
    uint16_t *test = SecondOutput16();
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_y, sub_y, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1HighbdConvolveYTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_Y, AV1HighbdConvolveYTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3_Y, AV1HighbdConvolveYTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_Y, AV1HighbdConvolveYTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (low bit-depth)
//////////////////////////////////////////////////////////////
typedef void (*convolve_copy_func)(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride, int w,
                                   int h);

class AV1ConvolveCopyTest : public AV1ConvolveTest<convolve_copy_func> {
 public:
  void RunTest() {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const uint8_t *input = FirstRandomInput8(GetParam().BitDepth());
    uint8_t *reference = FirstOutput8();
    aom_convolve_copy(input, width, reference, kOutputStride, width, height);
    uint8_t *test = SecondOutput8();
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

// Note that even though these are AOM convolve functions, we are using the
// newer AV1 test framework.
TEST_P(AV1ConvolveCopyTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_neon));
#endif

#if HAVE_MSA
INSTANTIATE_TEST_SUITE_P(MSA_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_msa));
#endif

#if HAVE_DSPR2
INSTANTIATE_TEST_SUITE_P(DSPR2_COPY, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_dspr2));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (high bit-depth)
///////////////////////////////////////////////////////////////
typedef void (*highbd_convolve_copy_func)(const uint16_t *src, int src_stride,
                                          uint16_t *dst, int dst_stride, int w,
                                          int h);

class AV1HighbdConvolveCopyTest
    : public AV1ConvolveTest<highbd_convolve_copy_func> {
 public:
  void RunTest() {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();
    const uint16_t *input = FirstRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    av1_highbd_convolve_2d_copy_sr(input, width, reference, kOutputStride,
                                   width, height);
    uint16_t *test = SecondOutput16();
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1HighbdConvolveCopyTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_COPY, AV1HighbdConvolveCopyTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_copy_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2_COPY, AV1HighbdConvolveCopyTest,
    BuildHighbdParams(av1_highbd_convolve_2d_copy_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_COPY, AV1HighbdConvolveCopyTest,
    BuildHighbdParams(av1_highbd_convolve_2d_copy_sr_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

/////////////////////////////////////////////////////////
// Single reference convolve-2D functions (low bit-depth)
/////////////////////////////////////////////////////////
typedef void (*convolve_2d_func)(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams *filter_params_x,
                                 const InterpFilterParams *filter_params_y,
                                 const int subpel_x_qn, const int subpel_y_qn,
                                 ConvolveParams *conv_params);

class AV1Convolve2DTest : public AV1ConvolveTest<convolve_2d_func> {
 public:
  void RunTest() {
    for (int sub_x = 0; sub_x < 16; ++sub_x) {
      for (int sub_y = 0; sub_y < 16; ++sub_y) {
        for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
          for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
            TestConvolve(static_cast<InterpFilter>(h_f),
                         static_cast<InterpFilter>(v_f), sub_x, sub_y);
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint8_t *input = FirstRandomInput8(GetParam().BitDepth());
    uint8_t *reference = FirstOutput8();
    ConvolveParams conv_params1 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    av1_convolve_2d_sr(input, width, reference, kOutputStride, width, height,
                       filter_params_x, filter_params_y, sub_x, sub_y,
                       &conv_params1);
    uint8_t *test = SecondOutput8();
    ConvolveParams conv_params2 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, filter_params_y, sub_x, sub_y,
                              &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1Convolve2DTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_2D, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_2D, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_2D, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_2D, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////////////
// Single reference convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////////////

typedef void (*highbd_convolve_2d_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd);

class AV1HighbdConvolve2DTest
    : public AV1ConvolveTest<highbd_convolve_2d_func> {
 public:
  void RunTest() {
    for (int sub_x = 0; sub_x < 16; ++sub_x) {
      for (int sub_y = 0; sub_y < 16; ++sub_y) {
        for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
          for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
            TestConvolve(static_cast<InterpFilter>(h_f),
                         static_cast<InterpFilter>(v_f), sub_x, sub_y);
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint16_t *input = FirstRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    av1_highbd_convolve_2d_sr(input, width, reference, kOutputStride, width,
                              height, filter_params_x, filter_params_y, sub_x,
                              sub_y, &conv_params1, bit_depth);
    uint16_t *test = SecondOutput16();
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, filter_params_y, sub_x, sub_y,
                              &conv_params2, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1HighbdConvolve2DTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_2D, AV1HighbdConvolve2DTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3_2D, AV1HighbdConvolve2DTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_2D, AV1HighbdConvolve2DTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////
// Compound Convolve Tests
//////////////////////////

// The compound functions do not work for chroma block sizes. Provide
// a function to generate test parameters for just luma block sizes.
template <typename T>
std::vector<TestParam<T>> GetLumaTestParams(
    std::initializer_list<int> bit_depths, T test_func) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
  }
  std::vector<TestParam<T>> result;
  for (int bit_depth : bit_depths) {
    for (const auto &block : sizes) {
      result.push_back(TestParam<T>(block, bit_depth, test_func));
    }
  }
  return result;
}

template <typename T>
std::vector<TestParam<T>> GetLowbdLumaTestParams(T test_func) {
  return GetLumaTestParams({ 8 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildLowbdLumaParams(
    T test_func) {
  return ::testing::ValuesIn(GetLowbdLumaTestParams(test_func));
}

TEST_F(AV1ConvolveParametersTest, GetLowbdLumaTestParams) {
  auto v = GetLowbdLumaTestParams(nullptr);
  ASSERT_EQ(22U, v.size());
  for (const auto &e : v) {
    ASSERT_EQ(8, e.BitDepth());
    ASSERT_EQ(nullptr, e.TestFunction());
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
template <typename T>
std::vector<TestParam<T>> GetHighbdLumaTestParams(T test_func) {
  return GetLumaTestParams({ 10, 12 }, test_func);
}

TEST_F(AV1ConvolveParametersTest, GetHighbdLumaTestParams) {
  auto v = GetHighbdLumaTestParams(nullptr);
  ASSERT_EQ(44U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &e : v) {
    ASSERT_TRUE(10 == e.BitDepth() || 12 == e.BitDepth());
    ASSERT_EQ(nullptr, e.TestFunction());
    if (e.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildHighbdLumaParams(
    T test_func) {
  return ::testing::ValuesIn(GetHighbdLumaTestParams(test_func));
}

#endif  // CONFIG_AV1_HIGHBITDEPTH

// Compound cases also need to test different frame offsets and weightings.
class CompoundParam {
 public:
  CompoundParam(bool use_dist_wtd_comp_avg, int fwd_offset, int bck_offset)
      : use_dist_wtd_comp_avg_(use_dist_wtd_comp_avg), fwd_offset_(fwd_offset),
        bck_offset_(bck_offset) {}

  bool UseDistWtdCompAvg() const { return use_dist_wtd_comp_avg_; }
  int FwdOffset() const { return fwd_offset_; }
  int BckOffset() const { return bck_offset_; }

 private:
  bool use_dist_wtd_comp_avg_;
  int fwd_offset_;
  int bck_offset_;
};

std::vector<CompoundParam> GetCompoundParams() {
  std::vector<CompoundParam> result;
  result.push_back(CompoundParam(false, 0, 0));
  for (int k = 0; k < 2; ++k) {
    for (int l = 0; l < 4; ++l) {
      result.push_back(CompoundParam(true, quant_dist_lookup_table[k][l][0],
                                     quant_dist_lookup_table[k][l][1]));
    }
  }
  return result;
}

TEST_F(AV1ConvolveParametersTest, GetCompoundParams) {
  auto v = GetCompoundParams();
  ASSERT_EQ(9U, v.size());
  ASSERT_FALSE(v[0].UseDistWtdCompAvg());
  for (size_t i = 1; i < v.size(); ++i) {
    ASSERT_TRUE(v[i].UseDistWtdCompAvg());
  }
}

////////////////////////////////////////////////
// Compound convolve-x functions (low bit-depth)
////////////////////////////////////////////////

ConvolveParams GetConvolveParams(int do_average, CONV_BUF_TYPE *conv_buf,
                                 int width, int bit_depth,
                                 const CompoundParam &compound) {
  ConvolveParams conv_params =
      get_conv_params_no_round(do_average, 0, conv_buf, width, 1, bit_depth);
  conv_params.use_dist_wtd_comp_avg = compound.UseDistWtdCompAvg();
  conv_params.fwd_offset = compound.FwdOffset();
  conv_params.bck_offset = compound.BckOffset();
  return conv_params;
}

class AV1CompoundConvolveXTest : public AV1ConvolveTest<convolve_x_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int sub_pix = 0; sub_pix < 16; ++sub_pix) {
      for (int f = EIGHTTAP_REGULAR; f < INTERP_FILTERS_ALL; ++f) {
        for (const auto &c : compound_params) {
          TestConvolve(sub_pix, static_cast<InterpFilter>(f), c);
        }
      }
    }
  }

 protected:
  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual convolve_x_func ReferenceFunc() const {
    return av1_dist_wtd_convolve_x;
  }

 private:
  void TestConvolve(const int sub_pix, const InterpFilter filter,
                    const CompoundParam &compound) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const uint8_t *input1 = FirstRandomInput8(bit_depth);
    const uint8_t *input2 = SecondRandomInput8(bit_depth);
    uint8_t *reference = FirstOutput8();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();
    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound, sub_pix, filter);

    uint8_t *test = SecondOutput8();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, sub_pix, filter);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(convolve_x_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const int sub_pix,
                const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params =
        FilterParams(filter, GetParam().Block());

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);
  }
};

TEST_P(AV1CompoundConvolveXTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_X, AV1CompoundConvolveXTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_X, AV1CompoundConvolveXTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_X, AV1CompoundConvolveXTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_X, AV1CompoundConvolveXTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-x functions (high bit-depth)
/////////////////////////////////////////////////
class AV1HighbdCompoundConvolveXTest
    : public AV1ConvolveTest<highbd_convolve_x_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int sub_pix = 0; sub_pix < 16; ++sub_pix) {
      for (int f = EIGHTTAP_REGULAR; f < INTERP_FILTERS_ALL; ++f) {
        for (const auto &c : compound_params) {
          TestConvolve(sub_pix, static_cast<InterpFilter>(f), c);
        }
      }
    }
  }

 protected:
  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual highbd_convolve_x_func ReferenceFunc() const {
    return av1_highbd_dist_wtd_convolve_x;
  }

 private:
  void TestConvolve(const int sub_pix, const InterpFilter filter,
                    const CompoundParam &compound) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();

    const uint16_t *input1 = FirstRandomInput16(bit_depth);
    const uint16_t *input2 = SecondRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();

    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound, sub_pix, filter);

    uint16_t *test = SecondOutput16();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, sub_pix, filter);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void Convolve(highbd_convolve_x_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const int sub_pix,
                const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params =
        FilterParams(filter, GetParam().Block());
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);
    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);
  }
};

TEST_P(AV1HighbdCompoundConvolveXTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C_X, AV1HighbdCompoundConvolveXTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1_X, AV1HighbdCompoundConvolveXTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_X, AV1HighbdCompoundConvolveXTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////
// Compound convolve-y functions (low bit-depth)
////////////////////////////////////////////////

// Note that the X and Y convolve functions have the same type signature and
// logic; they only differentiate the filter parameters and reference function.
class AV1CompoundConvolveYTest : public AV1CompoundConvolveXTest {
 protected:
  virtual const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }

  virtual convolve_x_func ReferenceFunc() const override {
    return av1_dist_wtd_convolve_y;
  }
};

TEST_P(AV1CompoundConvolveYTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_Y, AV1CompoundConvolveYTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_Y, AV1CompoundConvolveYTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_Y, AV1CompoundConvolveYTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_Y, AV1CompoundConvolveYTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-y functions (high bit-depth)
/////////////////////////////////////////////////

// Again, the X and Y convolve functions have the same type signature and logic.
class AV1HighbdCompoundConvolveYTest : public AV1HighbdCompoundConvolveXTest {
  virtual highbd_convolve_x_func ReferenceFunc() const override {
    return av1_highbd_dist_wtd_convolve_y;
  }
  virtual const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }
};

TEST_P(AV1HighbdCompoundConvolveYTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C_Y, AV1HighbdCompoundConvolveYTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1_Y, AV1HighbdCompoundConvolveYTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_Y, AV1HighbdCompoundConvolveYTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (low bit-depth)
//////////////////////////////////////////////////////
typedef void (*compound_conv_2d_copy_func)(const uint8_t *src, int src_stride,
                                           uint8_t *dst, int dst_stride, int w,
                                           int h, ConvolveParams *conv_params);

class AV1CompoundConvolve2DCopyTest
    : public AV1ConvolveTest<compound_conv_2d_copy_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (const auto &compound : compound_params) {
      TestConvolve(compound);
    }
  }

 private:
  void TestConvolve(const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();

    const uint8_t *input1 = FirstRandomInput8(bit_depth);
    const uint8_t *input2 = SecondRandomInput8(bit_depth);
    uint8_t *reference = FirstOutput8();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();
    Convolve(av1_dist_wtd_convolve_2d_copy, input1, input2, reference,
             reference_conv_buf, compound);

    uint8_t *test = SecondOutput8();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(compound_conv_2d_copy_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params);
  }
};

TEST_P(AV1CompoundConvolve2DCopyTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_COPY, AV1CompoundConvolve2DCopyTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2_COPY, AV1CompoundConvolve2DCopyTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_COPY, AV1CompoundConvolve2DCopyTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON_COPY, AV1CompoundConvolve2DCopyTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (high bit-depth)
///////////////////////////////////////////////////////
typedef void (*highbd_compound_conv_2d_copy_func)(const uint16_t *src,
                                                  int src_stride, uint16_t *dst,
                                                  int dst_stride, int w, int h,
                                                  ConvolveParams *conv_params,
                                                  int bd);

class AV1HighbdCompoundConvolve2DCopyTest
    : public AV1ConvolveTest<highbd_compound_conv_2d_copy_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (const auto &compound : compound_params) {
      TestConvolve(compound);
    }
  }

 private:
  void TestConvolve(const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();

    const uint16_t *input1 = FirstRandomInput16(bit_depth);
    const uint16_t *input2 = SecondRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();
    Convolve(av1_highbd_dist_wtd_convolve_2d_copy, input1, input2, reference,
             reference_conv_buf, compound);

    uint16_t *test = SecondOutput16();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void Convolve(highbd_compound_conv_2d_copy_func test_func,
                const uint16_t *src1, const uint16_t *src2, uint16_t *dst,
                uint16_t *conv_buf, const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);
  }
};

TEST_P(AV1HighbdCompoundConvolve2DCopyTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C_COPY, AV1HighbdCompoundConvolve2DCopyTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1_COPY, AV1HighbdCompoundConvolve2DCopyTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_COPY, AV1HighbdCompoundConvolve2DCopyTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

/////////////////////////////////////////////////
// Compound convolve-2d functions (low bit-depth)
/////////////////////////////////////////////////

class AV1CompoundConvolve2DTest : public AV1ConvolveTest<convolve_2d_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
        for (int sub_x = 0; sub_x < 16; ++sub_x) {
          for (int sub_y = 0; sub_y < 16; ++sub_y) {
            for (const auto &compound : compound_params) {
              TestConvolve(static_cast<InterpFilter>(h_f),
                           static_cast<InterpFilter>(v_f), sub_x, sub_y,
                           compound);
            }
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y,
                    const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();

    const uint8_t *input1 = FirstRandomInput8(bit_depth);
    const uint8_t *input2 = SecondRandomInput8(bit_depth);
    uint8_t *reference = FirstOutput8();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();
    Convolve(av1_dist_wtd_convolve_2d, input1, input2, reference,
             reference_conv_buf, compound, h_f, v_f, sub_x, sub_y);

    uint8_t *test = SecondOutput8();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, h_f, v_f, sub_x, sub_y);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(convolve_2d_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const InterpFilter h_f,
                const InterpFilter v_f, const int sub_x, const int sub_y) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);

    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);
  }
};

TEST_P(AV1CompoundConvolve2DTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C_2D, AV1CompoundConvolve2DTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2_2D, AV1CompoundConvolve2DTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_sse2));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3_2D, AV1CompoundConvolve2DTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2_2D, AV1CompoundConvolve2DTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON_2D, AV1CompoundConvolve2DTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////
// Compound convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////

class AV1HighbdCompoundConvolve2DTest
    : public AV1ConvolveTest<highbd_convolve_2d_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
        for (int sub_x = 0; sub_x < 16; ++sub_x) {
          for (int sub_y = 0; sub_y < 16; ++sub_y) {
            for (const auto &compound : compound_params) {
              TestConvolve(static_cast<InterpFilter>(h_f),
                           static_cast<InterpFilter>(v_f), sub_x, sub_y,
                           compound);
            }
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y,
                    const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();
    const uint16_t *input1 = FirstRandomInput16(bit_depth);
    const uint16_t *input2 = SecondRandomInput16(bit_depth);
    uint16_t *reference = FirstOutput16();
    uint16_t *reference_conv_buf = FirstConvolveBuffer();
    Convolve(av1_highbd_dist_wtd_convolve_2d, input1, input2, reference,
             reference_conv_buf, compound, h_f, v_f, sub_x, sub_y);

    uint16_t *test = SecondOutput16();
    uint16_t *test_conv_buf = SecondConvolveBuffer();
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, h_f, v_f, sub_x, sub_y);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(highbd_convolve_2d_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const InterpFilter h_f,
                const InterpFilter v_f, const int sub_x, const int sub_y) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const int bit_depth = GetParam().BitDepth();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);
  }
};

TEST_P(AV1HighbdCompoundConvolve2DTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C_2D, AV1HighbdCompoundConvolve2DTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1_2D, AV1HighbdCompoundConvolve2DTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2_2D, AV1HighbdCompoundConvolve2DTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_avx2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
