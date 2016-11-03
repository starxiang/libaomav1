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

// Simple Encoder
// ==============
//
// This is an example of a simple encoder loop. It takes an input file in
// YV12 format, passes it through the encoder, and writes the compressed
// frames to disk in IVF format. Other decoder examples build upon this
// one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For encoders, you only have to include `aom_encoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// aom.
//
// Getting The Default Configuration
// ---------------------------------
// Encoders have the notion of "usage profiles." For example, an encoder
// may want to publish default configurations for both a video
// conferencing application and a best quality offline encoder. These
// obviously have very different default settings. Consult the
// documentation for your codec to see if it provides any default
// configurations. All codecs provide a default configuration, number 0,
// which is valid for material in the vacinity of QCIF/QVGA.
//
// Updating The Configuration
// ---------------------------------
// Almost all applications will want to update the default configuration
// with settings specific to their usage. Here we set the width and height
// of the video file to that specified on the command line. We also scale
// the default bitrate based on the ratio between the default resolution
// and the resolution specified on the command line.
//
// Initializing The Codec
// ----------------------
// The encoder is initialized by the following code.
//
// Encoding A Frame
// ----------------
// The frame is read as a continuous block (size width * height * 3 / 2)
// from the input file. If a frame was read (the input file has not hit
// EOF) then the frame is passed to the encoder. Otherwise, a NULL
// is passed, indicating the End-Of-Stream condition to the encoder. The
// `frame_cnt` is reused as the presentation time stamp (PTS) and each
// frame is shown for one frame-time in duration. The flags parameter is
// unused in this example. The deadline is set to AOM_DL_REALTIME to
// make the example run as quickly as possible.

// Forced Keyframes
// ----------------
// Keyframes can be forced by setting the AOM_EFLAG_FORCE_KF bit of the
// flags passed to `aom_codec_control()`. In this example, we force a
// keyframe every <keyframe-interval> frames. Note, the output stream can
// contain additional keyframes beyond those that have been forced using the
// AOM_EFLAG_FORCE_KF flag because of automatic keyframe placement by the
// encoder.
//
// Processing The Encoded Data
// ---------------------------
// Each packet of type `AOM_CODEC_CX_FRAME_PKT` contains the encoded data
// for this frame. We write a IVF frame header, followed by the raw data.
//
// Cleanup
// -------
// The `aom_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exeptions, aom_codec functions return an enumerated error status,
// with the value `0` indicating success.
//
// Error Resiliency Features
// -------------------------
// Error resiliency is controlled by the g_error_resilient member of the
// configuration structure. Use the `decode_with_drops` example to decode with
// frames 5-10 dropped. Compare the output for a file encoded with this example
// versus one encoded with the `simple_encoder` example.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"

#include "../tools_common.h"
#include "../video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <codec> <width> <height> <infile> <outfile> "
          "<keyframe-interval> <error-resilient> <frames to encode>\n"
          "See comments in simple_encoder.c for more information.\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static int encode_frame(aom_codec_ctx_t *codec, aom_image_t *img,
                        int frame_index, int flags, AvxVideoWriter *writer) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res =
      aom_codec_encode(codec, img, frame_index, 1, flags, AOM_DL_GOOD_QUALITY);
  if (res != AOM_CODEC_OK) die_codec(codec, "Failed to encode frame");

  while ((pkt = aom_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
      if (!aom_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts)) {
        die_codec(codec, "Failed to write compressed frame");
      }
      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}

// TODO(tomfinegan): Improve command line parsing and add args for bitrate/fps.
int main(int argc, char **argv) {
  FILE *infile = NULL;
  aom_codec_ctx_t codec;
  aom_codec_enc_cfg_t cfg;
  int frame_count = 0;
  aom_image_t raw;
  aom_codec_err_t res;
<<<<<<< HEAD   (c1ca94 Merge changes from topic 'update_dering' into nextgenv2)
  AvxVideoInfo info;
=======
  AvxVideoInfo info = { 0 };
>>>>>>> BRANCH (749267 Fix clang-format issues.)
  AvxVideoWriter *writer = NULL;
  const AvxInterface *encoder = NULL;
  const int fps = 30;
  const int bitrate = 200;
  int keyframe_interval = 0;
  int max_frames = 0;
  int frames_encoded = 0;
  const char *codec_arg = NULL;
  const char *width_arg = NULL;
  const char *height_arg = NULL;
  const char *infile_arg = NULL;
  const char *outfile_arg = NULL;
  const char *keyframe_interval_arg = NULL;

  exec_name = argv[0];

<<<<<<< HEAD   (c1ca94 Merge changes from topic 'update_dering' into nextgenv2)
  // Clear explicitly, as simply assigning "{ 0 }" generates
  // "missing-field-initializers" warning in some compilers.
  memset(&info, 0, sizeof(info));

=======
>>>>>>> BRANCH (749267 Fix clang-format issues.)
  if (argc != 9) die("Invalid number of arguments");

  codec_arg = argv[1];
  width_arg = argv[2];
  height_arg = argv[3];
  infile_arg = argv[4];
  outfile_arg = argv[5];
  keyframe_interval_arg = argv[6];
  max_frames = strtol(argv[8], NULL, 0);

  encoder = get_aom_encoder_by_name(codec_arg);
  if (!encoder) die("Unsupported codec.");

  info.codec_fourcc = encoder->fourcc;
  info.frame_width = strtol(width_arg, NULL, 0);
  info.frame_height = strtol(height_arg, NULL, 0);
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image.");
  }

  keyframe_interval = strtol(keyframe_interval_arg, NULL, 0);
  if (keyframe_interval < 0) die("Invalid keyframe interval value.");

  printf("Using %s\n", aom_codec_iface_name(encoder->codec_interface()));

  res = aom_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res) die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = strtol(argv[7], NULL, 0);

  writer = aom_video_writer_open(outfile_arg, kContainerIVF, &info);
  if (!writer) die("Failed to open %s for writing.", outfile_arg);

  if (!(infile = fopen(infile_arg, "rb")))
    die("Failed to open %s for reading.", infile_arg);

  if (aom_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
    die_codec(&codec, "Failed to initialize encoder");

  // Encode frames.
  while (aom_img_read(&raw, infile)) {
    int flags = 0;
    if (keyframe_interval > 0 && frame_count % keyframe_interval == 0)
      flags |= AOM_EFLAG_FORCE_KF;
    encode_frame(&codec, &raw, frame_count++, flags, writer);
    frames_encoded++;
    if (max_frames > 0 && frames_encoded >= max_frames) break;
  }

  // Flush encoder.
  while (encode_frame(&codec, NULL, -1, 0, writer)) continue;

  printf("\n");
  fclose(infile);
  printf("Processed %d frames.\n", frame_count);

  aom_img_free(&raw);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  aom_video_writer_close(writer);

  return EXIT_SUCCESS;
}
