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

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "av1/common/daala_tx.h"
#include "av1/encoder/daala_fwd_txfm.h"

#if CONFIG_DAALA_TX

// Temporary while we still need av1_get_tx_scale() for testing
#include "av1/common/idct.h"

// Complete Daala TX map, sans lossless which is special cased
typedef void (*daala_ftx)(od_coeff[], const od_coeff *, int);

static daala_ftx tx_map[TX_SIZES][TX_TYPES] = {
  //  4-point transforms
  { od_bin_fdct4, od_bin_fdst4,
    od_bin_fdst4, od_bin_fidtx4 },

  //  8-point transforms
  { od_bin_fdct8, od_bin_fdst8,
    od_bin_fdst8, od_bin_fidtx8 },

  //  16-point transforms
  { od_bin_fdct16, od_bin_fdst16,
    od_bin_fdst16, od_bin_fidtx16 },

  //  32-point transforms
  { od_bin_fdct32, od_bin_fdst32,
    od_bin_fdst32, od_bin_fidtx32 },

#if CONFIG_TX64X64
  //  64-point transforms
  { od_bin_fdct64, NULL,
    NULL, od_bin_fidtx64 },
#endif
};

static int tx_flip(TX_TYPE t){
  return t==2;
}

// Daala TX toplevel entry point, same interface as av1 low-bidepth
// and high-bitdepth TX (av1_fwd_txfm and av1_highbd_fwd_txfm).  This
// same function is intended for both low and high bitdepth cases with
// a tran_low_t of 32 bits (matching od_coeff).
void daala_fwd_txfm(const int16_t *input_pixels,
                        tran_low_t *output_coeffs,
                        int input_stride,
                        TxfmParam *txfm_param) {

  const TX_SIZE tx_size = txfm_param->tx_size;
  const TX_TYPE tx_type = txfm_param->tx_type;
  assert(tx_size <= TX_SIZES);
  assert(tx_type <= TX_TYPES_1D);

  if (txfm_param->lossless) {
    // Transform function special-cased for lossless
    assert(tx_type == DCT_DCT);
    assert(tx_size == TX_4x4);
    av1_fwht4x4(input_pixels, output_coeffs, input_stride);
  } else{
    // General TX case
#if 1
    // up-3, down-1 compatability mode with av1_get_tx_scale
    const int upshift = 4;
#else
    // How we intend to run these transforms in the near future
    const int upshift = TX_COEFF_DEPTH - txfm_param->bd;
#endif

    assert(pre_upshift >= 0);
    assert(post_downshift >= 0);
    assert(sizeof(tran_low_t) == sizeof(od_coeff));
    assert(sizeof(tran_low_t) >= 4);

    // Hook into existing map translation infrastructure to select
    // appropriate TX functions
    const int col_n = tx_size_wide[tx_size];
    const int row_n = tx_size_high[tx_size];
    const TX_SIZE col_idx = txsize_vert_map[tx_size];
    const TX_SIZE row_idx = txsize_horz_map[tx_size];
    daala_ftx col_tx = tx_map[col_idx][vtx_tab[tx_type]];
    daala_ftx row_tx = tx_map[row_idx][htx_tab[tx_type]];
    int col_flip = tx_flip(vtx_tab[tx_type]);
    int row_flip = tx_flip(htx_tab[tx_type]);
    od_coeff tmp[MAX_TX_SIZE];
    int r;
    int c;

    assert(col_tx);
    assert(row_tx);

    // Transform columns
    for (c = 0; c < col_n; ++c) {
      // Cast and shift
      for (r = 0; r < row_n; ++r)
        tmp[r] = (int)input_pixels[r*input_stride + c] << upshift;
      if(col_flip)
         col_tx(tmp, tmp+col_n-1, -1);
      else
         col_tx(tmp, tmp, 1);
      // No ystride in daala_tx right now, store output vector
      // into column the long way
      for (r = 0; r < row_n; ++r)
        output_coeffs[r*col_n + c] = tmp[r];
    }

    // Transform rows
    for (r = 0; r < row_n; ++r)
      if(row_flip)
        row_tx(output_coeffs + r*col_n, output_coeffs + (r+1)*col_n-1, -1);
      else
        row_tx(output_coeffs + r*col_n, output_coeffs + r*col_n, 1);

#if 1
    // This is temporary while we're testing against existing
    // behavior (preshift up 3, then downshift by one plus av1_get_tx_scale)
    int downshift = 1 + av1_get_tx_scale(tx_size);
    for (r = 0; r < row_n; ++r)
      for (c = 0; c < col_n; ++c)
        output_coeffs[r*col_n+c] =
          (output_coeffs[r*col_n+c] + (1 << (downshift - 1))) >> downshift;
#endif
  }
}

#endif
