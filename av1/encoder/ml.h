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

#ifndef AV1_ENCODER_ML_H_
#define AV1_ENCODER_ML_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int num_inputs;                // Number of input nodes, i.e. features.
  int num_outputs;               // Number of output nodes.
  int num_hidden_layers;         // Number of hidden layers, maximum 10.
  int num_hidden_nodes[10];      // Number of nodes for each hidden layer.
  const float *weights[10 + 1];  // Weight parameters, indexed by layer.
  const float *bias[10 + 1];     // Bias parameters, indexed by layer.
} NN_CONFIG;

// Calculate prediction based on the given input features and neural net config.
// Assume there are no more than 128 nodes in each hidden layer.
void av1_nn_predict(const float *features, const NN_CONFIG *nn_config,
                    float *output);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RD_H_
