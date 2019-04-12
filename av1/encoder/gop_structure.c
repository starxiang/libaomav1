/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "aom_ports/system_state.h"

#include "av1/common/onyxc_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/gop_structure.h"

// Set parameters for frames between 'start' and 'end' (excluding both).
static void set_multi_layer_params(GF_GROUP *const gf_group, int start, int end,
                                   int *frame_ind, int arf_ind, int level) {
  assert(level >= MIN_PYRAMID_LVL);
  const int num_frames_to_process = end - start - 1;
  assert(num_frames_to_process >= 0);
  if (num_frames_to_process == 0) return;

  // Either we are at the last level of the pyramid, or we don't have enough
  // frames between 'l' and 'r' to create one more level.
  if (level == MIN_PYRAMID_LVL || num_frames_to_process < 3) {
    // Leaf nodes.
    while (++start < end) {
      gf_group->update_type[*frame_ind] = LF_UPDATE;
      gf_group->arf_src_offset[*frame_ind] = 0;
      gf_group->arf_pos_in_gf[*frame_ind] = 0;
      gf_group->arf_update_idx[*frame_ind] = arf_ind;
      gf_group->frame_disp_idx[*frame_ind] = start;
      gf_group->pyramid_level[*frame_ind] = MIN_PYRAMID_LVL;
      ++gf_group->pyramid_lvl_nodes[MIN_PYRAMID_LVL];
      ++(*frame_ind);
    }
  } else {
    const int m = (start + end) / 2;
    const int arf_pos_in_gf = *frame_ind;

    // Internal ARF.
    gf_group->update_type[*frame_ind] = INTNL_ARF_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = m - start - 1;
    gf_group->arf_pos_in_gf[*frame_ind] = 0;
    gf_group->arf_update_idx[*frame_ind] = 1;  // mark all internal ARF 1
    gf_group->frame_disp_idx[*frame_ind] = m;
    gf_group->pyramid_level[*frame_ind] = level;
    ++gf_group->pyramid_lvl_nodes[level];
    ++(*frame_ind);

    // Frames displayed before this internal ARF.
    set_multi_layer_params(gf_group, start, m, frame_ind, 1, level - 1);

    // Overlay for internal ARF.
    gf_group->update_type[*frame_ind] = INTNL_OVERLAY_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = 0;
    gf_group->arf_pos_in_gf[*frame_ind] = arf_pos_in_gf;  // For bit allocation.
    gf_group->arf_update_idx[*frame_ind] = 1;
    gf_group->frame_disp_idx[*frame_ind] = m;
    gf_group->pyramid_level[*frame_ind] = MIN_PYRAMID_LVL;
    ++(*frame_ind);

    // Frames displayed after this internal ARF.
    set_multi_layer_params(gf_group, m, end, frame_ind, arf_ind, level - 1);
  }
}

static int construct_multi_layer_gf_structure(
    GF_GROUP *const gf_group, int gf_interval, int pyr_height,
    FRAME_UPDATE_TYPE first_frame_update_type) {
  gf_group->pyramid_height = pyr_height;
  av1_zero_array(gf_group->pyramid_lvl_nodes, MAX_PYRAMID_LVL);
  int frame_index = 0;

  // Keyframe / Overlay frame / Golden frame.
  assert(gf_interval >= 1);
  assert(first_frame_update_type == KF_UPDATE ||
         first_frame_update_type == OVERLAY_UPDATE ||
         first_frame_update_type == GF_UPDATE);
  gf_group->update_type[frame_index] = first_frame_update_type;
  gf_group->arf_src_offset[frame_index] = 0;
  gf_group->arf_pos_in_gf[frame_index] = 0;
  gf_group->arf_update_idx[frame_index] = 0;
  gf_group->pyramid_level[frame_index] = MIN_PYRAMID_LVL;
  ++frame_index;

  // ALTREF.
  const int use_altref = (gf_group->pyramid_height > 0);
  if (use_altref) {
    gf_group->update_type[frame_index] = ARF_UPDATE;
    gf_group->arf_src_offset[frame_index] = gf_interval - 1;
    gf_group->arf_pos_in_gf[frame_index] = 0;
    gf_group->arf_update_idx[frame_index] = 0;
    gf_group->frame_disp_idx[frame_index] = gf_interval;
    gf_group->pyramid_level[frame_index] = gf_group->pyramid_height;
    ++frame_index;
  }

  // Rest of the frames.
  const int next_height =
      use_altref ? gf_group->pyramid_height - 1 : gf_group->pyramid_height;
  assert(next_height >= MIN_PYRAMID_LVL);
  set_multi_layer_params(gf_group, 0, gf_interval, &frame_index, 0,
                         next_height);
  return frame_index;
}

#define CHECK_GF_PARAMETER 0
#if CHECK_GF_PARAMETER
void check_frame_params(GF_GROUP *const gf_group, int gf_interval) {
  static const char *update_type_strings[FRAME_UPDATE_TYPES] = {
    "KF_UPDATE",       "LF_UPDATE",      "GF_UPDATE",
    "ARF_UPDATE",      "OVERLAY_UPDATE", "INTNL_OVERLAY_UPDATE",
    "INTNL_ARF_UPDATE"
  };
  FILE *fid = fopen("GF_PARAMS.txt", "a");

  fprintf(fid, "\ngf_interval = {%d}\n", gf_interval);
  for (int i = 0; i <= gf_group->size; ++i) {
    fprintf(fid, "#%2d : %s %d %d %d %d\n", i,
            update_type_strings[gf_group->update_type[i]],
            gf_group->arf_src_offset[i], gf_group->arf_pos_in_gf[i],
            gf_group->arf_update_idx[i], gf_group->pyramid_level[i]);
  }

  fprintf(fid, "number of nodes in each level: \n");
  for (int i = 0; i < gf_group->pyramid_height; ++i) {
    fprintf(fid, "lvl %d: %d ", i, gf_group->pyramid_lvl_nodes[i]);
  }
  fprintf(fid, "\n");
  fclose(fid);
}
#endif  // CHECK_GF_PARAMETER

static INLINE int max_pyramid_height_from_width(int pyramid_width) {
  if (pyramid_width > 12) return 4;
  if (pyramid_width > 6) return 3;
  if (pyramid_width > 3) return 2;
  if (pyramid_width > 1) return 1;
  return 0;
}

static int get_pyramid_height(const AV1_COMP *const cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  assert(IMPLIES(cpi->oxcf.gf_max_pyr_height == MIN_PYRAMID_LVL,
                 !rc->source_alt_ref_pending));  // define_gf_group() enforced.
  if (!rc->source_alt_ref_pending) {
    return MIN_PYRAMID_LVL;
  }
  assert(cpi->oxcf.gf_max_pyr_height > MIN_PYRAMID_LVL);
  if (!cpi->internal_altref_allowed) {
    assert(MIN_PYRAMID_LVL + 1 <= cpi->oxcf.gf_max_pyr_height);
    return MIN_PYRAMID_LVL + 1;
  }
  return AOMMIN(max_pyramid_height_from_width(rc->baseline_gf_interval),
                cpi->oxcf.gf_max_pyr_height);
}

// Stack data structure utilities
static INLINE int stack_pop(int *stack, int *stack_size) {
  const int r = stack[0];
  for (int idx = 1; idx < *stack_size; ++idx) stack[idx - 1] = stack[idx];
  *stack_size = *stack_size - 1;

  return r;
}

static INLINE void stack_push(int *stack, int new_item, int *stack_size) {
  for (int idx = *stack_size; idx > 0; --idx) stack[idx] = stack[idx - 1];
  stack[0] = new_item;
  *stack_size = *stack_size + 1;
}

// Queue data structure utility
// Enqueue function, return the dequeued value since the queue has limited
// length.
static INLINE int queue_enqueue(int *queue, int new_item, int *queue_size,
                                int max_queue_size) {
  if (*queue_size < max_queue_size) {
    queue[*queue_size] = new_item;
    *queue_size = *queue_size + 1;

    return -1;
  } else {
    assert(*queue_size == max_queue_size);
    int dequeued_value = queue[0];

    for (int idx = 1; idx < max_queue_size; ++idx) queue[idx - 1] = queue[idx];
    queue[max_queue_size - 1] = new_item;

    return dequeued_value;
  }
}

#define REF_IDX(ref) ((ref)-LAST_FRAME)

static INLINE void reset_ref_frame_idx(int *ref_idx, int reset_value) {
  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) ref_idx[i] = reset_value;
}

static INLINE void set_ref_frame_disp_idx(GF_GROUP *const gf_group) {
  for (int i = 0; i <= gf_group->size; ++i) {
    for (int ref = 0; ref < INTER_REFS_PER_FRAME; ++ref) {
      int ref_gop_idx = gf_group->ref_frame_gop_idx[i][ref];
      if (ref_gop_idx == -1) {
        gf_group->ref_frame_disp_idx[i][ref] = -1;
      } else {
        gf_group->ref_frame_disp_idx[i][ref] =
            gf_group->frame_disp_idx[ref_gop_idx];
      }
    }
  }
}

static void set_gop_ref_frame_map(GF_GROUP *const gf_group) {
  // Initialize the map, if the first frame is a key frame, the slots will be
  // reset by this frame, otherwise the slots are reset as all -1.
  reset_ref_frame_idx(gf_group->ref_frame_gop_idx[0], -1);
  int init_ref_idx;
  if (gf_group->update_type[0] == KF_UPDATE)
    init_ref_idx = 0;
  else
    init_ref_idx = -1;
  for (int frame_idx = 1; frame_idx <= gf_group->size; ++frame_idx)
    reset_ref_frame_idx(gf_group->ref_frame_gop_idx[frame_idx], init_ref_idx);

  // Set the map for frames in the current gop
  int arf_stack[MAX_PYRAMID_LVL] = { -1, -1, -1, -1 };
  int arf_stack_size = 0;
  int lst_queue[3] = { -1, -1, -1 };
  int lst_queue_size = 0;
  int int_arf_idx = -1;
  int ext_ref_idx = -1;

  for (int frame_idx = 1; frame_idx <= gf_group->size; ++frame_idx) {
    gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(GOLDEN_FRAME)] = 0;
    if (arf_stack_size > 0) {
      gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(ALTREF_FRAME)] =
          arf_stack[arf_stack_size - 1];
    }
    if (arf_stack_size > 1) {
      gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(BWDREF_FRAME)] =
          arf_stack[0];
    }
    if (arf_stack_size > 2) {
      gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(ALTREF2_FRAME)] =
          arf_stack[1];
    }
    if (arf_stack_size == 1 && ext_ref_idx != -1) {
      gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(ALTREF2_FRAME)] =
          ext_ref_idx;
    }
    if (lst_queue_size > 0) {
      for (int lst_idx = lst_queue_size - 1; lst_idx >= 0; --lst_idx) {
        int lst_frame_type_idx = LAST_FRAME + (lst_queue_size - 1 - lst_idx);
        gf_group->ref_frame_gop_idx[frame_idx][REF_IDX(lst_frame_type_idx)] =
            lst_queue[lst_idx];
      }
    }

    switch (gf_group->update_type[frame_idx]) {
      case LF_UPDATE:
        queue_enqueue(lst_queue, frame_idx, &lst_queue_size, 3);
        break;
      case INTNL_OVERLAY_UPDATE:
        int_arf_idx = stack_pop(arf_stack, &arf_stack_size);
        ext_ref_idx = queue_enqueue(lst_queue, int_arf_idx, &lst_queue_size, 3);
        break;
      case OVERLAY_UPDATE: stack_pop(arf_stack, &arf_stack_size); break;
      case INTNL_ARF_UPDATE:
      case ARF_UPDATE: stack_push(arf_stack, frame_idx, &arf_stack_size); break;
      default: break;
    }
  }

  // Set the map in display order index by converting from gop indices in the
  // above map
  set_ref_frame_disp_idx(gf_group);
}

void av1_gop_setup_structure(AV1_COMP *cpi,
                             const EncodeFrameParams *const frame_params) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  const int key_frame = (frame_params->frame_type == KEY_FRAME);
  const FRAME_UPDATE_TYPE first_frame_update_type =
      key_frame ? KF_UPDATE
                : rc->source_alt_ref_active ? OVERLAY_UPDATE : GF_UPDATE;
  gf_group->size = construct_multi_layer_gf_structure(
      gf_group, rc->baseline_gf_interval, get_pyramid_height(cpi),
      first_frame_update_type);

  // We need to configure the frame at the end of the sequence + 1 that
  // will be the start frame for the next group. Otherwise prior to the
  // call to av1_get_second_pass_params(), the data will be undefined.
  gf_group->update_type[gf_group->size] =
      (rc->source_alt_ref_pending) ? OVERLAY_UPDATE : GF_UPDATE;
  gf_group->arf_update_idx[gf_group->size] = 0;
  gf_group->arf_pos_in_gf[gf_group->size] = 0;

  set_gop_ref_frame_map(gf_group);

#if CHECK_GF_PARAMETER
  check_frame_params(gf_group, rc->baseline_gf_interval);
#endif
}
