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

// This tool is a gadget for offline probability training.
// A binary executable aom_entropy_optimizer will be generated in tools/. It
// parses a binary file consisting of counts written in the format of
// FRAME_COUNTS in entropymode.h, and computes optimized probability tables
// and CDF tables, which will be written to a new c file optimized_probs.c
// according to format in the codebase.
//
// Command line: ./aom_entropy_optimizer [directory of the count file]
//
// The input file can either be generated by encoding a single clip by
// turning on entropy_stats experiment, or be collected at a larger scale at
// which a python script which will be provided soon can be used to aggregate
// multiple stats output.

#include <assert.h>
#include <stdio.h>
#include "./aom_config.h"
#include "av1/common/entropymode.h"

#define SPACES_PER_TAB 2
#define CDF_MAX_SIZE 16

typedef unsigned int aom_count_type;
// A log file recording parsed counts
static FILE *logfile;  // TODO(yuec): make it a command line option

static INLINE uint8_t get_binary_prob_new(unsigned int n0, unsigned int n1) {
  // The "+1" will prevent this function from generating extreme probability
  // when both n0 and n1 are small
  const unsigned int den = n0 + 1 + n1 + 1;
  return get_prob(n0 + 1, den);
}

static int parse_stats(aom_count_type **ct_ptr, FILE *const probsfile, int tabs,
                       int dim_of_cts, int *cts_each_dim,
                       int flatten_last_dim) {
  if (dim_of_cts < 1) {
    fprintf(stderr, "The dimension of a counts vector should be at least 1!\n");
    return 1;
  }
  const int total_modes = cts_each_dim[0];
  if (dim_of_cts == 1) {
    aom_count_type *counts1d = *ct_ptr;
    assert(total_modes == 2);
    (*ct_ptr) += total_modes;
    uint8_t probs = get_binary_prob_new(counts1d[0], counts1d[1]);
    if (tabs > 0) fprintf(probsfile, "%*c", tabs * SPACES_PER_TAB, ' ');
    fprintf(probsfile, " %3d ", probs);
    fprintf(logfile, "%d ", counts1d[0]);
    fprintf(logfile, "%d\n", counts1d[total_modes - 1]);
  } else if (dim_of_cts == 2 && flatten_last_dim) {
    assert(cts_each_dim[1] == 2);

    for (int k = 0; k < total_modes; ++k) {
      if (k == total_modes - 1) {
        fprintf(probsfile, " %3d ",
                get_binary_prob_new((*ct_ptr)[0], (*ct_ptr)[1]));
      } else {
        fprintf(probsfile, " %3d,",
                get_binary_prob_new((*ct_ptr)[0], (*ct_ptr)[1]));
      }
      fprintf(logfile, "%d %d\n", (*ct_ptr)[0], (*ct_ptr)[1]);
      (*ct_ptr) += 2;
    }
  } else {
    for (int k = 0; k < total_modes; ++k) {
      int tabs_next_level;
      if (dim_of_cts == 2 || (dim_of_cts == 3 && flatten_last_dim)) {
        fprintf(probsfile, "%*c{", tabs * SPACES_PER_TAB, ' ');
        tabs_next_level = 0;
      } else {
        fprintf(probsfile, "%*c{\n", tabs * SPACES_PER_TAB, ' ');
        tabs_next_level = tabs + 1;
      }
      if (parse_stats(ct_ptr, probsfile, tabs_next_level, dim_of_cts - 1,
                      cts_each_dim + 1, flatten_last_dim)) {
        return 1;
      }
      if (dim_of_cts == 2 || (dim_of_cts == 3 && flatten_last_dim)) {
        if (k == total_modes - 1)
          fprintf(probsfile, "}\n");
        else
          fprintf(probsfile, "},\n");
      } else {
        if (k == total_modes - 1)
          fprintf(probsfile, "%*c}\n", tabs * SPACES_PER_TAB, ' ');
        else
          fprintf(probsfile, "%*c},\n", tabs * SPACES_PER_TAB, ' ');
      }
    }
  }
  return 0;
}

// This function parses the stats of a syntax, either binary or multi-symbol,
// in different contexts, and writes the optimized probability table to
// probsfile.
//   counts: pointer of the first count element in counts array
//   probsfile: output file
//   dim_of_cts: number of dimensions of counts array
//   cts_each_dim: an array storing size of each dimension of counts array
//   flatten_last_dim: for a binary syntax, if flatten_last_dim is 0, probs in
//                     different contexts will be written separately, e.g.,
//                     {{p1}, {p2}, ...};
//                     otherwise will be grouped together at the second last
//                     dimension, i.e.,
//                     {p1, p2, ...}.
//   prefix: declaration header for the entropy table
static void optimize_entropy_table(aom_count_type *counts,
                                   FILE *const probsfile, int dim_of_cts,
                                   int *cts_each_dim, int flatten_last_dim,
                                   char *prefix) {
  aom_count_type *ct_ptr = counts;

  assert(!flatten_last_dim || cts_each_dim[dim_of_cts - 1] == 2);

  fprintf(probsfile, "%s = {\n", prefix);
  if (parse_stats(&ct_ptr, probsfile, 1, dim_of_cts, cts_each_dim,
                  flatten_last_dim)) {
    fprintf(probsfile, "Optimizer failed!\n");
  }
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "\n");
}

static void counts_to_cdf(const aom_count_type *counts, aom_cdf_prob *cdf,
                          int modes) {
  int64_t csum[CDF_MAX_SIZE];
  assert(modes <= CDF_MAX_SIZE);

  csum[0] = counts[0] + 1;
  for (int i = 1; i < modes; ++i) csum[i] = counts[i] + 1 + csum[i - 1];

  int64_t sum = csum[modes - 1];
  const int64_t round_shift = sum >> 1;
  for (int i = 0; i < modes; ++i) {
    cdf[i] = (csum[i] * CDF_PROB_TOP + round_shift) / sum;
    cdf[i] = AOMMIN(cdf[i], CDF_PROB_TOP - (modes - 1 + i) * 4);
    cdf[i] = (i == 0) ? AOMMAX(cdf[i], 4) : AOMMAX(cdf[i], cdf[i - 1] + 4);
  }
}

static int parse_counts_for_cdf_opt(aom_count_type **ct_ptr,
                                    FILE *const probsfile, int tabs,
                                    int dim_of_cts, int *cts_each_dim) {
  if (dim_of_cts < 1) {
    fprintf(stderr, "The dimension of a counts vector should be at least 1!\n");
    return 1;
  }
  const int total_modes = cts_each_dim[0];
  if (dim_of_cts == 1) {
    assert(total_modes <= CDF_MAX_SIZE);
    aom_cdf_prob cdfs[CDF_MAX_SIZE];
    aom_count_type *counts1d = *ct_ptr;

    counts_to_cdf(counts1d, cdfs, total_modes);
    (*ct_ptr) += total_modes;

    if (tabs > 0) fprintf(probsfile, "%*c", tabs * SPACES_PER_TAB, ' ');
    fprintf(probsfile, "AOM_CDF%d( ", total_modes);
    for (int k = 0; k < total_modes - 1; ++k) {
      fprintf(probsfile, "%d", cdfs[k]);
      if (k < total_modes - 2) fprintf(probsfile, ",");
    }
    fprintf(probsfile, " )");
  } else {
    for (int k = 0; k < total_modes; ++k) {
      int tabs_next_level;

      if (dim_of_cts == 2)
        fprintf(probsfile, "%*c{", tabs * SPACES_PER_TAB, ' ');
      else
        fprintf(probsfile, "%*c{\n", tabs * SPACES_PER_TAB, ' ');
      tabs_next_level = dim_of_cts == 2 ? 0 : tabs + 1;

      if (parse_counts_for_cdf_opt(ct_ptr, probsfile, tabs_next_level,
                                   dim_of_cts - 1, cts_each_dim + 1)) {
        return 1;
      }

      if (dim_of_cts == 2) {
        if (k == total_modes - 1)
          fprintf(probsfile, "}\n");
        else
          fprintf(probsfile, "},\n");
      } else {
        if (k == total_modes - 1)
          fprintf(probsfile, "%*c}\n", tabs * SPACES_PER_TAB, ' ');
        else
          fprintf(probsfile, "%*c},\n", tabs * SPACES_PER_TAB, ' ');
      }
    }
  }
  return 0;
}

static void optimize_cdf_table(aom_count_type *counts, FILE *const probsfile,
                               int dim_of_cts, int *cts_each_dim,
                               char *prefix) {
  aom_count_type *ct_ptr = counts;

  fprintf(probsfile, "%s = {\n", prefix);
  if (parse_counts_for_cdf_opt(&ct_ptr, probsfile, 1, dim_of_cts,
                               cts_each_dim)) {
    fprintf(probsfile, "Optimizer failed!\n");
  }
  fprintf(probsfile, "};\n\n");
}

int main(int argc, const char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Please specify the input stats file!\n");
    exit(EXIT_FAILURE);
  }

  FILE *const statsfile = fopen(argv[1], "rb");
  if (statsfile == NULL) {
    fprintf(stderr, "Failed to open input file!\n");
    exit(EXIT_FAILURE);
  }

  FRAME_COUNTS fc;
  const size_t bytes = fread(&fc, sizeof(FRAME_COUNTS), 1, statsfile);
  if (!bytes) return 1;

  FILE *const probsfile = fopen("optimized_probs.c", "w");
  if (probsfile == NULL) {
    fprintf(stderr,
            "Failed to create output file for optimized entropy tables!\n");
    exit(EXIT_FAILURE);
  }

  logfile = fopen("aom_entropy_optimizer_parsed_counts.log", "w");
  if (logfile == NULL) {
    fprintf(stderr, "Failed to create log file for parsed counts!\n");
    exit(EXIT_FAILURE);
  }

  int cts_each_dim[10];

  /* Intra mode (keyframe luma) */
  cts_each_dim[0] = KF_MODE_CONTEXTS;
  cts_each_dim[1] = KF_MODE_CONTEXTS;
  cts_each_dim[2] = INTRA_MODES;
  optimize_cdf_table(&fc.kf_y_mode[0][0][0], probsfile, 3, cts_each_dim,
                     "const aom_cdf_prob\n"
                     "default_kf_y_mode_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS]"
                     "[CDF_SIZE(INTRA_MODES)]");

  cts_each_dim[0] = DIRECTIONAL_MODES;
  cts_each_dim[1] = 2 * MAX_ANGLE_DELTA + 1;
  optimize_cdf_table(&fc.angle_delta[0][0], probsfile, 2, cts_each_dim,
                     "const aom_cdf_prob\n"
                     "default_angle_delta_cdf"
                     "[DIRECTIONAL_MODES][CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)]");

  /* Intra mode (non-keyframe luma) */
  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = INTRA_MODES;
  optimize_cdf_table(
      &fc.y_mode[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)]");

  /* Intra mode (chroma) */
  cts_each_dim[0] = CFL_ALLOWED_TYPES;
  cts_each_dim[1] = INTRA_MODES;
  cts_each_dim[2] = UV_INTRA_MODES;
  optimize_cdf_table(&fc.uv_mode[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES]"
                     "[CDF_SIZE(UV_INTRA_MODES)]");

  /* Partition */
  cts_each_dim[0] = PARTITION_CONTEXTS;
  cts_each_dim[1] = EXT_PARTITION_TYPES;
  optimize_cdf_table(&fc.partition[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_"
                     "PARTITION_TYPES)]");

  /* Interpolation filter */
  cts_each_dim[0] = SWITCHABLE_FILTER_CONTEXTS;
  cts_each_dim[1] = SWITCHABLE_FILTERS;
  optimize_cdf_table(&fc.switchable_interp[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS]"
                     "[CDF_SIZE(SWITCHABLE_FILTERS)]");

  /* Motion vector referencing */
  cts_each_dim[0] = NEWMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_entropy_table(
      &fc.newmv_mode[0][0], probsfile, 2, cts_each_dim, 1,
      "static const aom_prob default_newmv_prob[NEWMV_MODE_CONTEXTS]");
  optimize_cdf_table(&fc.newmv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = GLOBALMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_entropy_table(
      &fc.zeromv_mode[0][0], probsfile, 2, cts_each_dim, 1,
      "static const aom_prob default_zeromv_prob[GLOBALMV_MODE_CONTEXTS]");
  optimize_cdf_table(&fc.zeromv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = REFMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_entropy_table(
      &fc.refmv_mode[0][0], probsfile, 2, cts_each_dim, 1,
      "static const aom_prob default_refmv_prob[REFMV_MODE_CONTEXTS]");
  optimize_cdf_table(&fc.refmv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = DRL_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.drl_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)]");

  /* ext_inter experiment */
  /* New compound mode */
  cts_each_dim[0] = INTER_MODE_CONTEXTS;
  cts_each_dim[1] = INTER_COMPOUND_MODES;
  optimize_cdf_table(&fc.inter_compound_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_"
                     "SIZE(INTER_COMPOUND_MODES)]");

  /* Interintra */
  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.interintra[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(2)]");

  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = INTERINTRA_MODES;
  optimize_cdf_table(&fc.interintra_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE("
                     "INTERINTRA_MODES)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.wedge_interintra[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

  /* Compound type */
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = COMPOUND_TYPES;
  optimize_cdf_table(
      &fc.compound_interinter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)]");

#if WEDGE_IDX_ENTROPY_CODING
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 16;
  optimize_cdf_table(&fc.wedge_idx[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_wedge_idx_cdf[BLOCK_SIZES_ALL][CDF_SIZE(16)]");
#endif

  /* motion_var and warped_motion experiments */
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = MOTION_MODES;
  optimize_cdf_table(
      &fc.motion_mode[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)]");
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.obmc[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

  /* Intra/inter flag */
  cts_each_dim[0] = INTRA_INTER_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.intra_inter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)]");

  /* Single/comp ref flag */
  cts_each_dim[0] = COMP_INTER_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.comp_inter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)]");

  /* ext_comp_refs experiment */
  cts_each_dim[0] = COMP_REF_TYPE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.comp_ref_type[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = UNI_COMP_REF_CONTEXTS;
  cts_each_dim[1] = UNIDIR_COMP_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(&fc.uni_comp_ref[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_"
                     "COMP_REFS - 1][CDF_SIZE(2)]");

  /* Reference frame (single ref) */
  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = SINGLE_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.single_ref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)]");

  /* ext_refs experiment */
  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = FWD_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.comp_ref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)]");

  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = BWD_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.comp_bwdref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)]");

  /* Transform size */
  cts_each_dim[0] = TXFM_PARTITION_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.txfm_partition[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)]");

  /* Skip flag */
  cts_each_dim[0] = SKIP_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.skip[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)]");

  /* intrabc */
  cts_each_dim[0] = 2;
  optimize_cdf_table(
      &fc.intrabc[0], probsfile, 1, cts_each_dim,
      "static const aom_cdf_prob default_intrabc_cdf[CDF_SIZE(2)]");

  /* filter_intra experiment */
  cts_each_dim[0] = FILTER_INTRA_MODES;
  optimize_cdf_table(
      &fc.filter_intra_mode[0], probsfile, 1, cts_each_dim,
      "static const aom_cdf_prob "
      "default_filter_intra_mode_cdf[CDF_SIZE(FILTER_INTRA_MODES)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.filter_intra[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = TXB_SKIP_CONTEXTS;
  cts_each_dim[2] = 2;
  optimize_entropy_table(
      &fc.txb_skip[0][0][0], probsfile, 3, cts_each_dim, 1,
      "static const aom_prob "
      "default_txk_skip[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]");
  optimize_cdf_table(&fc.txb_skip[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_nz_map_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_"
                     "CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = EOB_COEF_CONTEXTS;
  cts_each_dim[3] = 2;
  optimize_entropy_table(
      &fc.eob_flag[0][0][0][0], probsfile, 4, cts_each_dim, 1,
      "static const aom_prob "
      "default_eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = EOB_COEF_CONTEXTS;
  cts_each_dim[3] = 2;
  optimize_cdf_table(
      &fc.eob_extra[0][0][0][0], probsfile, 4, cts_each_dim,
      "static const aom_cdf_prob "
      "default_eob_extra_cdf[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS]"
      "[CDF_SIZE(2)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 5;
  optimize_cdf_table(&fc.eob_multi16[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi16[PLANE_TYPES][2][CDF_SIZE(5)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 6;
  optimize_cdf_table(&fc.eob_multi32[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi32[PLANE_TYPES][2][CDF_SIZE(6)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 7;
  optimize_cdf_table(&fc.eob_multi64[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi64[PLANE_TYPES][2][CDF_SIZE(7)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 8;
  optimize_cdf_table(&fc.eob_multi128[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi128[PLANE_TYPES][2][CDF_SIZE(8)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 9;
  optimize_cdf_table(&fc.eob_multi256[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi256[PLANE_TYPES][2][CDF_SIZE(9)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 10;
  optimize_cdf_table(&fc.eob_multi512[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi512[PLANE_TYPES][2][CDF_SIZE(10)]");

  cts_each_dim[0] = PLANE_TYPES;
  cts_each_dim[1] = 2;
  cts_each_dim[2] = 11;
  optimize_cdf_table(&fc.eob_multi1024[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_eob_multi1024[PLANE_TYPES][2][CDF_SIZE(11)]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = BR_CDF_SIZE - 1;
  cts_each_dim[3] = LEVEL_CONTEXTS;
  cts_each_dim[4] = 2;
  optimize_entropy_table(&fc.coeff_lps[0][0][0][0][0], probsfile, 5,
                         cts_each_dim, 1,
                         "static const aom_prob "
                         "default_coeff_lps[TX_SIZES][PLANE_TYPES][BR_CDF_SIZE-"
                         "1][LEVEL_CONTEXTS]");
  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = LEVEL_CONTEXTS;
  cts_each_dim[3] = BR_CDF_SIZE;
  optimize_cdf_table(&fc.coeff_lps_multi[0][0][0][0], probsfile, 4,
                     cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_coeff_lps_multi[TX_SIZES][PLANE_TYPES][LEVEL_"
                     "CONTEXTS][CDF_SIZE(BR_CDF_SIZE)]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D;
  cts_each_dim[3] = 4;
  optimize_cdf_table(
      &fc.coeff_base_multi[0][0][0][0], probsfile, 4, cts_each_dim,
      "static const aom_cdf_prob "
      "default_coeff_base_multi[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]"
      "[CDF_SIZE(NUM_BASE_LEVELS+2)]");

  cts_each_dim[0] = TX_SIZES;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = SIG_COEF_CONTEXTS_EOB;
  cts_each_dim[3] = 3;
  optimize_cdf_table(
      &fc.coeff_base_eob_multi[0][0][0][0], probsfile, 4, cts_each_dim,
      "static const aom_cdf_prob "
      "default_coeff_base_eob_multi[TX_SIZES][PLANE_TYPES][SIG_COEF_"
      "CONTEXTS_EOB][CDF_SIZE(NUM_BASE_LEVELS+1)]");

  /* Skip mode flag */
  cts_each_dim[0] = SKIP_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_entropy_table(
      &fc.skip_mode[0][0], probsfile, 2, cts_each_dim, 1,
      "static const aom_prob default_skip_mode_probs[SKIP_MODE_CONTEXTS]");
  optimize_cdf_table(&fc.skip_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_skip_mode_cdfs[SKIP_MODE_CONTEXTS][CDF_SIZE(2)]");

  fclose(statsfile);
  fclose(logfile);
  fclose(probsfile);

  return 0;
}
