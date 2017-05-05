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

#ifndef TEST_DECODE_TEST_DRIVER_H_
#define TEST_DECODE_TEST_DRIVER_H_
#include <cstring>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "./aom_config.h"
#include "aom/aom_decoder.h"

namespace libaom_test {

class CodecFactory;
class CompressedVideoSource;

// Provides an object to handle decoding output
class DxDataIterator {
 public:
  explicit DxDataIterator(AomCodecCtxT *decoder)
      : decoder_(decoder), iter_(NULL) {}

  const AomImageT *Next() { return aom_codec_get_frame(decoder_, &iter_); }

 private:
  AomCodecCtxT *decoder_;
  AomCodecIterT iter_;
};

// Provides a simplified interface to manage one video decoding.
// Similar to Encoder class, the exact services should be added
// as more tests are added.
class Decoder {
 public:
  explicit Decoder(AomCodecDecCfgT cfg)
      : cfg_(cfg), flags_(0), init_done_(false) {
    memset(&decoder_, 0, sizeof(decoder_));
  }

  Decoder(AomCodecDecCfgT cfg, const AomCodecFlagsT flag)
      : cfg_(cfg), flags_(flag), init_done_(false) {
    memset(&decoder_, 0, sizeof(decoder_));
  }

  virtual ~Decoder() { aom_codec_destroy(&decoder_); }

  AomCodecErrT PeekStream(const uint8_t *cxdata, size_t size,
                          AomCodecStreamInfoT *stream_info);

  AomCodecErrT DecodeFrame(const uint8_t *cxdata, size_t size);

  AomCodecErrT DecodeFrame(const uint8_t *cxdata, size_t size, void *user_priv);

  DxDataIterator GetDxData() { return DxDataIterator(&decoder_); }

  void Control(int ctrl_id, int arg) { Control(ctrl_id, arg, AOM_CODEC_OK); }

  void Control(int ctrl_id, const void *arg) {
    InitOnce();
    const AomCodecErrT res = aom_codec_control_(&decoder_, ctrl_id, arg);
    ASSERT_EQ(AOM_CODEC_OK, res) << DecodeError();
  }

  void Control(int ctrl_id, int arg, AomCodecErrT expected_value) {
    InitOnce();
    const AomCodecErrT res = aom_codec_control_(&decoder_, ctrl_id, arg);
    ASSERT_EQ(expected_value, res) << DecodeError();
  }

  const char *DecodeError() {
    const char *detail = aom_codec_error_detail(&decoder_);
    return detail ? detail : aom_codec_error(&decoder_);
  }

  // Passes the external frame buffer information to libaom.
  AomCodecErrT SetFrameBufferFunctions(AomGetFrameBufferCbFnT cb_get,
                                       AomReleaseFrameBufferCbFnT cb_release,
                                       void *user_priv) {
    InitOnce();
    return aom_codec_set_frame_buffer_functions(&decoder_, cb_get, cb_release,
                                                user_priv);
  }

  const char *GetDecoderName() const {
    return aom_codec_iface_name(CodecInterface());
  }

  bool IsVP8() const;

  bool IsAV1() const;

  AomCodecCtxT *GetDecoder() { return &decoder_; }

 protected:
  virtual AomCodecIfaceT *CodecInterface() const = 0;

  void InitOnce() {
    if (!init_done_) {
      const AomCodecErrT res =
          aom_codec_dec_init(&decoder_, CodecInterface(), &cfg_, flags_);
      ASSERT_EQ(AOM_CODEC_OK, res) << DecodeError();
      init_done_ = true;
    }
  }

  AomCodecCtxT decoder_;
  AomCodecDecCfgT cfg_;
  AomCodecFlagsT flags_;
  bool init_done_;
};

// Common test functionality for all Decoder tests.
class DecoderTest {
 public:
  // Main decoding loop
  virtual void RunLoop(CompressedVideoSource *video);
  virtual void RunLoop(CompressedVideoSource *video,
                       const AomCodecDecCfgT &dec_cfg);

  virtual void set_cfg(const AomCodecDecCfgT &dec_cfg);
  virtual void set_flags(const AomCodecFlagsT flags);

  // Hook to be called before decompressing every frame.
  virtual void PreDecodeFrameHook(const CompressedVideoSource & /*video*/,
                                  Decoder * /*decoder*/) {}

  // Hook to be called to handle decode result. Return true to continue.
  virtual bool HandleDecodeResult(const AomCodecErrT res_dec,
                                  Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    return AOM_CODEC_OK == res_dec;
  }

  // Hook to be called on every decompressed frame.
  virtual void DecompressedFrameHook(const AomImageT & /*img*/,
                                     const unsigned int /*frame_number*/) {}

  // Hook to be called on peek result
  virtual void HandlePeekResult(Decoder *const decoder,
                                CompressedVideoSource *video,
                                const AomCodecErrT res_peek);

 protected:
  explicit DecoderTest(const CodecFactory *codec)
      : codec_(codec), cfg_(), flags_(0) {}

  virtual ~DecoderTest() {}

  const CodecFactory *codec_;
  AomCodecDecCfgT cfg_;
  AomCodecFlagsT flags_;
};

}  // namespace libaom_test

#endif  // TEST_DECODE_TEST_DRIVER_H_
