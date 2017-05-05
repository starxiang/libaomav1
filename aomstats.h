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

#ifndef AOMSTATS_H_
#define AOMSTATS_H_

#include <stdio.h>

#include "aom/aom_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This structure is used to abstract the different ways of handling
 * first pass statistics
 */
typedef struct {
  AomFixedBufT buf;
  int pass;
  FILE *file;
  char *buf_ptr;
  size_t buf_alloc_sz;
} StatsIoT;

int stats_open_file(StatsIoT *stats, const char *fpf, int pass);
int stats_open_mem(StatsIoT *stats, int pass);
void stats_close(StatsIoT *stats, int last_pass);
void stats_write(StatsIoT *stats, const void *pkt, size_t len);
AomFixedBufT stats_get(StatsIoT *stats);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOMSTATS_H_
