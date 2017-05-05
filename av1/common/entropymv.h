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

#ifndef AV1_COMMON_ENTROPYMV_H_
#define AV1_COMMON_ENTROPYMV_H_

#include "./aom_config.h"

#include "aom_dsp/prob.h"

#include "av1/common/mv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;

void av1_init_mv_probs(struct AV1Common *cm);

void av1_adapt_mv_probs(struct AV1Common *cm, int usehp);

#define MV_UPDATE_PROB 252

/* Symbols for coding which components are zero jointly */
#define MV_JOINTS 4
typedef enum {
  MV_JOINT_ZERO = 0,   /* Zero vector */
  MV_JOINT_HNZVZ = 1,  /* Vert zero, hor nonzero */
  MV_JOINT_HZVNZ = 2,  /* Hor zero, vert nonzero */
  MV_JOINT_HNZVNZ = 3, /* Both components nonzero */
} MvJointType;

static INLINE int mv_joint_vertical(MvJointType type) {
  return type == MV_JOINT_HZVNZ || type == MV_JOINT_HNZVNZ;
}

static INLINE int mv_joint_horizontal(MvJointType type) {
  return type == MV_JOINT_HNZVZ || type == MV_JOINT_HNZVNZ;
}

/* Symbols for coding magnitude class of nonzero components */
#define MV_CLASSES 11
typedef enum {
  MV_CLASS_0 = 0,   /* (0, 2]     integer pel */
  MV_CLASS_1 = 1,   /* (2, 4]     integer pel */
  MV_CLASS_2 = 2,   /* (4, 8]     integer pel */
  MV_CLASS_3 = 3,   /* (8, 16]    integer pel */
  MV_CLASS_4 = 4,   /* (16, 32]   integer pel */
  MV_CLASS_5 = 5,   /* (32, 64]   integer pel */
  MV_CLASS_6 = 6,   /* (64, 128]  integer pel */
  MV_CLASS_7 = 7,   /* (128, 256] integer pel */
  MV_CLASS_8 = 8,   /* (256, 512] integer pel */
  MV_CLASS_9 = 9,   /* (512, 1024] integer pel */
  MV_CLASS_10 = 10, /* (1024,2048] integer pel */
} MvClassType;

#define CLASS0_BITS 1 /* bits at integer precision for class 0 */
#define CLASS0_SIZE (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)
#define MV_FP_SIZE 4

#define MV_MAX_BITS (MV_CLASSES + CLASS0_BITS + 2)
#define MV_MAX ((1 << MV_MAX_BITS) - 1)
#define MV_VALS ((MV_MAX << 1) + 1)

#define MV_IN_USE_BITS 14
#define MV_UPP ((1 << MV_IN_USE_BITS) - 1)
#define MV_LOW (-(1 << MV_IN_USE_BITS))

extern const AomTreeIndex av1_mv_joint_tree[];
extern const AomTreeIndex av1_mv_class_tree[];
extern const AomTreeIndex av1_mv_class0_tree[];
extern const AomTreeIndex av1_mv_fp_tree[];

typedef struct {
  AomProb sign;
  AomProb classes[MV_CLASSES - 1];
#if CONFIG_EC_MULTISYMBOL
  AomCdfProb class_cdf[CDF_SIZE(MV_CLASSES)];
#endif
  AomProb class0[CLASS0_SIZE - 1];
  AomProb bits[MV_OFFSET_BITS];
  AomProb class0_fp[CLASS0_SIZE][MV_FP_SIZE - 1];
  AomProb fp[MV_FP_SIZE - 1];
#if CONFIG_EC_MULTISYMBOL
  AomCdfProb class0_fp_cdf[CLASS0_SIZE][CDF_SIZE(MV_FP_SIZE)];
  AomCdfProb fp_cdf[CDF_SIZE(MV_FP_SIZE)];
#endif
  AomProb class0_hp;
  AomProb hp;
} NmvComponent;

typedef struct {
  AomProb joints[MV_JOINTS - 1];
#if CONFIG_EC_MULTISYMBOL
  AomCdfProb joint_cdf[CDF_SIZE(MV_JOINTS)];
#endif
  NmvComponent comps[2];
} NmvContext;

static INLINE MvJointType av1_get_mv_joint(const MV *mv) {
  if (mv->row == 0) {
    return mv->col == 0 ? MV_JOINT_ZERO : MV_JOINT_HNZVZ;
  } else {
    return mv->col == 0 ? MV_JOINT_HZVNZ : MV_JOINT_HNZVNZ;
  }
}

MvClassType av1_get_mv_class(int z, int *offset);

typedef struct {
  unsigned int sign[2];
  unsigned int classes[MV_CLASSES];
  unsigned int class0[CLASS0_SIZE];
  unsigned int bits[MV_OFFSET_BITS][2];
  unsigned int class0_fp[CLASS0_SIZE][MV_FP_SIZE];
  unsigned int fp[MV_FP_SIZE];
  unsigned int class0_hp[2];
  unsigned int hp[2];
} NmvComponentCounts;

typedef struct {
  unsigned int joints[MV_JOINTS];
  NmvComponentCounts comps[2];
} NmvContextCounts;

void av1_inc_mv(const MV *mv, NmvContextCounts *mvctx, const int usehp);
#if CONFIG_GLOBAL_MOTION
extern const AomTreeIndex
    av1_global_motion_types_tree[TREE_SIZE(GLOBAL_TRANS_TYPES)];
#endif  // CONFIG_GLOBAL_MOTION
#if CONFIG_EC_MULTISYMBOL
void av1_set_mv_cdfs(NmvContext *ctx);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENTROPYMV_H_
