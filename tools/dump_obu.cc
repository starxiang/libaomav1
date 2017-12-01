/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdlib>
#include <memory>
#include <string>

#include "./aom_config.h"
#include "./ivfdec.h"
#include "tools/obu_parser.h"
#include "./tools_common.h"
#include "./webmdec.h"

namespace {

const size_t kInitialBufferSize = 100 * 1024;

struct InputContext {
  InputContext() = default;
  ~InputContext() {
    free(unit_buffer);
  }

  AvxInputContext *avx_ctx = nullptr;
#if CONFIG_WEBM_IO
  WebmInputContext *webm_ctx = nullptr;
#endif
  uint8_t *unit_buffer = nullptr;
  size_t unit_buffer_size = 0;
};

void PrintUsage() {
  printf("Libaom OBU dump.\nUsage: dump_obu <input_file>\n");
}

void InitInputContext(InputContext *ctx) {
  memset(ctx->avx_ctx, 0, sizeof(*ctx->avx_ctx));
#if CONFIG_WEBM_IO
  memset(ctx->webm_ctx, 0, sizeof(*ctx->webm_ctx));
#endif
}

VideoFileType GetFileType(InputContext *ctx) {
  if (file_is_ivf(ctx->avx_ctx))
    return FILE_TYPE_IVF;
#if CONFIG_WEBM_IO
  else if(file_is_webm(ctx->webm_ctx, ctx->avx_ctx))
    return FILE_TYPE_WEBM;
#endif
  return FILE_TYPE_RAW;
}

bool ReadTemporalUnit(InputContext *ctx, size_t *unit_size) {
  VideoFileType file_type = ctx->avx_ctx->file_type;
  if (file_type == FILE_TYPE_IVF) {
    if (ivf_read_frame(ctx->avx_ctx->file, &ctx->unit_buffer, unit_size,
                       &ctx->unit_buffer_size)) {
      return false;
    }
  }
#if CONFIG_WEBM_IO
  else if (file_type == FILE_TYPE_WEBM) {
    size_t frame_size = ctx->unit_buffer_size;
    if (webm_read_frame(ctx->webm_ctx, &ctx->unit_buffer,
                        &ctx->unit_buffer_size)) {
      return false;
    }

    if (ctx->unit_buffer_size != frame_size) {
      // webmdec realloc'd the buffer, store new size.
      ctx->unit_buffer_size = frame_size;
    }

    *unit_size = frame_size;
  }
#endif
  else {
    // TODO(tomfinegan): Abuse FILE_TYPE_RAW for AV1/OBU elementary streams?
    fprintf(stderr, "Error: Unsupported file type.\n");
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, const char *argv[]) {
  // TODO(tomfinegan): Could do with some params for verbosity.
  if (argc < 2) {
    PrintUsage();
    exit(EXIT_SUCCESS);
  }

  const std::string filename = argv[1];

  typedef std::unique_ptr<FILE, decltype(&fclose)> FilePtr;
  FilePtr input_file(fopen(filename.c_str(), "rb"), &fclose);
  if (!input_file.get()) {
    fprintf(stderr, "Error: Cannot open input file.\n");
    return EXIT_FAILURE;
  }

  AvxInputContext avx_ctx;
  InputContext input_ctx;
  input_ctx.avx_ctx = &avx_ctx;
#if CONFIG_WEBM_IO
  WebmInputContext webm_ctx;
  input_ctx.webm_ctx = &webm_ctx;
#endif

  InitInputContext(&input_ctx);
  avx_ctx.file = input_file.get();
  avx_ctx.file_type = GetFileType(&input_ctx);

  if (avx_ctx.file_type == FILE_TYPE_WEBM) {
    // TODO(tomfinegan): Fix WebM support. It dies at the end of the first unit.
    printf("Warning: dump_obu WebM support is incomplete. Parsing will fail at "
           "the end of the first temporal unit.\n");
  }

  // Note: the reader utilities will realloc the buffer using realloc() etc.
  // Can't have nice things like unique_ptr wrappers with that type of
  // behavior underneath the function calls.
  input_ctx.unit_buffer =
    reinterpret_cast<uint8_t*>(calloc(kInitialBufferSize, 1));
  if (!input_ctx.unit_buffer) {
    fprintf(stderr, "Error: No memory, can't alloc input buffer.\n");
    return EXIT_FAILURE;
  }
  input_ctx.unit_buffer_size = kInitialBufferSize;

  size_t unit_size = 0;
  int unit_number = 0;
  while (ReadTemporalUnit(&input_ctx, &unit_size)) {
    printf("Temporal unit %d\n", unit_number);
    if (aom_tools::DumpObu(input_ctx.unit_buffer, unit_size) == false) {
      fprintf(stderr, "Error: Temporal Unit parse failed on unit number %d.\n",
              unit_number);
      return EXIT_FAILURE;
    }
    ++unit_number;
  }


  return EXIT_SUCCESS;
}
