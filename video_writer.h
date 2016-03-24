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

#ifndef VIDEO_WRITER_H_
#define VIDEO_WRITER_H_

#include "./video_common.h"

typedef enum { kContainerIVF } VpxContainer;

struct VpxVideoWriterStruct;
typedef struct VpxVideoWriterStruct VpxVideoWriter;

#ifdef __cplusplus
extern "C" {
#endif

// Finds and opens writer for specified container format.
// Returns an opaque VpxVideoWriter* upon success, or NULL upon failure.
// Right now only IVF format is supported.
VpxVideoWriter *vpx_video_writer_open(const char *filename,
                                      VpxContainer container,
                                      const VpxVideoInfo *info);

// Frees all resources associated with VpxVideoWriter* returned from
// vpx_video_writer_open() call.
void vpx_video_writer_close(VpxVideoWriter *writer);

// Writes frame bytes to the file.
int vpx_video_writer_write_frame(VpxVideoWriter *writer, const uint8_t *buffer,
                                 size_t size, int64_t pts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VIDEO_WRITER_H_
