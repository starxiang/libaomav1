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

#include "config/aom_config.h"

#include "av1/common/intra_entropy_models.h"
#include "av1/common/nn_em.h"
#include "av1/common/reconinter.h"
#include "av1/common/scan.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/seg_common.h"
#include "av1/common/txb_common.h"
#include "config/av1_rtcd.h"

#if CONFIG_INTRA_ENTROPY
// Applies the ReLu activation to one fc layer
// output[i] = Max(input[i],0.0f)
static void nn_relu(float *input, int num_outputs) {
  for (int i = 0; i < num_outputs; ++i) {
    input[i] = AOMMAX(input[i], 0.0f);
  }
}

// Applies the Sigmoid activation to one fc layer
// output[i] = 1/(1+exp(input[i]))
static void nn_sigmoid(float *input, int num_outputs) {
  for (int i = 0; i < num_outputs; ++i) {
    const float tmp = AOMMIN(AOMMAX(input[i], -10.0f), 10.0f);
    input[i] = 1.0f / (1.0f + expf(-tmp));
  }
}

// Forward prediction in one fc layer, used in function av1_nn_predict_V2
void av1_nn_fc_forward_c(FC_LAYER_EM *layer, const float *input,
                         float *output) {
  const float *weights = layer->weights;
  const float *bias = layer->bias;
  assert(layer->num_outputs < EM_MAX_NODES);
  // fc
  for (int node = 0; node < layer->num_outputs; ++node) {
    float val = bias[node];
    for (int i = 0; i < layer->num_inputs; ++i) val += weights[i] * input[i];
    output[node] = val;
    weights += layer->num_inputs;
  }

  // activation
  switch (layer->activation) {
    case ACTN_NONE:  // Do Nothing;
      break;
    case ACTN_RELU: nn_relu(output, layer->num_outputs); break;
    case ACTN_SIGMOID: nn_sigmoid(output, layer->num_outputs); break;
    default: assert(0 && "Unknown activation");  // Unknown activation
  }
}

void av1_nn_input_forward(FC_INPUT_LAYER_EM *layer, const int *sparse_features,
                          const float *dense_features) {
  float *output = layer->output;
  const int output_size = layer->num_outputs;
  int has_sparse = layer->num_sparse_inputs > 0;
  int has_dense = layer->num_dense_inputs > 0;
  assert(output_size < EM_MAX_NODES);

  const float *bias = layer->bias;
  for (int out_idx = 0; out_idx < output_size; out_idx++) {
    output[out_idx] = bias[out_idx];
  }

  // Handle sparse layer
  if (has_sparse) {
    const float(*sparse_weights)[EM_MAX_SPARSE_WEIGHT_SIZE] =
        layer->sparse_weights;
    for (int sparse_idx = 0; sparse_idx < layer->num_sparse_inputs;
         sparse_idx++) {
      const float *weight_ptr = sparse_weights[sparse_idx] +
                                sparse_features[sparse_idx] * output_size;
      for (int out_idx = 0; out_idx < output_size; out_idx++) {
        output[out_idx] += weight_ptr[out_idx];
      }
    }
  }

  // fc
  if (has_dense) {
    const float *dense_weights = layer->dense_weights;
    for (int node = 0; node < layer->num_outputs; ++node) {
      float val = 0.0f;
      for (int i = 0; i < layer->num_dense_inputs; ++i)
        val += dense_weights[i] * dense_features[i];
      output[node] = val;
      dense_weights += layer->num_dense_inputs;
    }
  }

  // activation
  switch (layer->activation) {
    case ACTN_NONE:  // Do Nothing;
      break;
    case ACTN_RELU: nn_relu(output, layer->num_outputs); break;
    case ACTN_SIGMOID: nn_sigmoid(output, layer->num_outputs); break;
    default: assert(0 && "Unknown activation");  // Unknown activation
  }
}

void av1_nn_predict_em(NN_CONFIG_EM *nn_config) {
  const int *sparse_features = nn_config->sparse_features;
  const float *dense_features = nn_config->dense_features;
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= EM_MAX_HLAYERS);

  // Propagate input layers
  av1_nn_input_forward(&nn_config->input_layer, sparse_features,
                       dense_features);
  float *input_nodes = nn_config->input_layer.output;

  // Propagate the layers.
  int num_inputs = nn_config->layer[0].num_inputs;
  for (int i = 0; i < num_layers; ++i) {
    assert(num_inputs == nn_config->layer[i].num_inputs);
    av1_nn_fc_forward(nn_config->layer + i, input_nodes,
                      nn_config->layer[i].output);
    input_nodes = nn_config->layer[i].output;
    num_inputs = nn_config->layer[i].num_outputs;
  }

  // Final layer
  assert(num_inputs == nn_config->num_logits);
  (void)num_inputs;
  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      if (nn_config->num_logits == 1) {
        // sigmoid
        const float tmp = AOMMIN(AOMMAX(input_nodes[0], -10.0f), 10.0f);
        nn_config->output[0] = 1.0f / (1.0f + expf(-tmp));
      } else {
        // softmax
        av1_nn_softmax_em(input_nodes, nn_config->output,
                          nn_config->num_logits);
      }
      break;
    default:
      av1_copy_array(nn_config->output, input_nodes, nn_config->num_logits);
  }
}

/***************************Backprop for gradient******************************/
// Backprop for ReLU activation
static void nn_relu_back(float *dX_out, const float *dY, const float *output,
                         int num_outputs) {
  for (int i = 0; i < num_outputs; ++i)
    dX_out[i] = output[i] > 0.0f ? dY[i] : 0.0f;
}

// Backprop for sigmoid activation
static void nn_sigmoid_back(float *dX_out, const float *dY, const float *output,
                            int num_outputs) {
  for (int i = 0; i < num_outputs; ++i)
    dX_out[i] = dY[i] * output[i] * (1 - output[i]);  // dX=dY*sigmoid(X)
}

// Backprop for softmax cross entropy loss
static void nn_softmax_cross_entropy_loss_back(float *dX_out,
                                               const float *output,
                                               const int num_outputs,
                                               const int label) {
  if (num_outputs == 1) {
    // sigmoid
    assert(label < 2);  // label [0,1]
    dX_out[0] = output[0] - (float)label;
  } else {
    // softmax
    assert(num_outputs > label);  // label [0,1,... num_logits-1]
    av1_copy_array(dX_out, output, num_outputs);
    dX_out[label] -= 1;
  }
}

// Backprop in one fc layer, used in function av1_nn_backprop
static void nn_fc_backward(const float *X, float *dX_out, FC_LAYER_EM *layer) {
  // backprop on activation
  float dY_fc[EM_MAX_NODES] = { 0.0f };  // dY for fc
  switch (layer->activation) {
    case ACTN_NONE:  // no activation, dY_fc <-- dY
      av1_copy_array(dY_fc, layer->dY, layer->num_outputs);
      break;
    case ACTN_RELU:
      nn_relu_back(dY_fc, layer->dY, layer->output, layer->num_outputs);
      break;
    case ACTN_SIGMOID:
      nn_sigmoid_back(dY_fc, layer->dY, layer->output, layer->num_outputs);
      break;
    default: assert(0 && "Unknown activation");  // Unknown activation
  }

  // backprop on fc
  // gradient of W, b
  float *dW = layer->dW;
  float *db = layer->db;
  for (int j = 0; j < layer->num_outputs; ++j) {
    for (int i = 0; i < layer->num_inputs; ++i) dW[i] += dY_fc[j] * X[i];
    db[j] += dY_fc[j];
    dW += layer->num_inputs;
  }

  // gradient of the input, i.e., the output of last layer
  if (dX_out) {
    for (int i = 0; i < layer->num_inputs; ++i) {
      float *w = layer->weights + i;
      float val = 0.0f;
      for (int j = 0; j < layer->num_outputs; ++j) {
        val += dY_fc[j] * w[j * layer->num_inputs];
      }
      dX_out[i] = val;
    }
  }
}

static void nn_fc_input_backward(const int *sparse_features,
                                 const float *dense_features,
                                 FC_INPUT_LAYER_EM *layer) {
  const int num_sparse = layer->num_sparse_inputs;
  const int num_dense = layer->num_dense_inputs;
  const int num_out = layer->num_outputs;
  const int has_sparse = num_sparse > 0;
  const int has_dense = num_dense > 0;

  // backprop on activation
  const float *dY_fc = NULL;
  float dY_buffer[EM_MAX_NODES] = { 0.0f };  // dY for fc
  switch (layer->activation) {
    case ACTN_NONE:  // no activation, dY_fc <-- dY
      dY_fc = layer->dY;
      break;
    case ACTN_RELU:
      nn_relu_back(dY_buffer, layer->dY, layer->output, layer->num_outputs);
      dY_fc = dY_buffer;
      break;
    case ACTN_SIGMOID:
      nn_sigmoid_back(dY_buffer, layer->dY, layer->output, layer->num_outputs);
      dY_fc = dY_buffer;
      break;
    default: assert(0 && "Unknown activation");  // Unknown activation
  }

  // Handle bias
  float *db = layer->db;
  for (int j = 0; j < num_out; ++j) {
    db[j] = dY_fc[j];
  }

  // Handle sparse
  float(*dW_sparse)[EM_MAX_SPARSE_WEIGHT_SIZE] = layer->dW_sparse;
  if (has_sparse) {
    for (int s_idx = 0; s_idx < num_sparse; s_idx++) {
      const int non_zero_idx = sparse_features[s_idx];
      for (int j = 0; j < num_out; ++j) {
        dW_sparse[s_idx][non_zero_idx * num_out + j] = dY_fc[j];
      }
    }
  }

  // Handle dense
  float *dW_dense = layer->dW_dense;
  if (has_dense) {
    for (int j = 0; j < num_out; ++j) {
      for (int i = 0; i < num_dense; ++i) {
        dW_dense[i] = dY_fc[j] * dense_features[i];
      }
      dW_dense += num_dense;
    }
  }
}

void av1_nn_backprop_em(NN_CONFIG_EM *nn_config, const int label) {
  // loss layer
  const int num_layers = nn_config->num_hidden_layers;
  float *prev_dY = num_layers > 0 ? nn_config->layer[num_layers - 1].dY
                                  : nn_config->input_layer.dY;

  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      nn_softmax_cross_entropy_loss_back(prev_dY, nn_config->output,
                                         nn_config->num_logits, label);
      break;
    default: assert(0 && "Unknown loss");  // Unknown loss
  }

  // hidden fc layer
  float *prev_Y = nn_config->input_layer.output;
  for (int layer_idx = num_layers - 1; layer_idx >= 0; layer_idx++) {
    if (layer_idx == 0) {
      prev_dY = nn_config->input_layer.dY;
      prev_Y = nn_config->input_layer.dY;
    } else {
      FC_LAYER_EM *last_layer = &nn_config->layer[layer_idx - 1];
      prev_dY = last_layer->dY;
      prev_Y = last_layer->output;
    }
    nn_fc_backward(prev_Y, prev_dY, &nn_config->layer[layer_idx]);
  }

  nn_fc_input_backward(nn_config->sparse_features, nn_config->dense_features,
                       &nn_config->input_layer);
}

void av1_nn_update_em(NN_CONFIG_EM *nn_config, float mu) {
  const int num_layers = nn_config->num_hidden_layers;

  // Update the weights
  for (int i = 0; i < num_layers; ++i) {
    FC_LAYER_EM *layer = nn_config->layer + i;
    for (int j = 0; j < layer->num_inputs * layer->num_outputs; ++j) {
      layer->weights[j] -= mu * layer->dW[j];
      layer->dW[j] = 0.f;
    }
    for (int j = 0; j < layer->num_outputs; ++j) {
      layer->bias[j] -= mu * layer->db[j];
      layer->db[j] = 0.f;
    }
  }

  // Input layer
  FC_INPUT_LAYER_EM *input_layer = &nn_config->input_layer;
  const int num_sparse = input_layer->num_sparse_inputs;
  const int num_dense = input_layer->num_dense_inputs;
  const int num_out = input_layer->num_outputs;
  const int has_sparse = num_sparse > 0;
  const int has_dense = num_dense > 0;

  float *b = input_layer->bias;
  float *db = input_layer->db;
  for (int j = 0; j < num_out; ++j) {
    b[j] -= mu * db[j];
  }

  // Handle sparse
  if (has_sparse) {
    float(*dW_sparse)[EM_MAX_SPARSE_WEIGHT_SIZE] = input_layer->dW_sparse;
    float(*W_sparse)[EM_MAX_SPARSE_WEIGHT_SIZE] = input_layer->sparse_weights;

    for (int s_idx = 0; s_idx < num_sparse; s_idx++) {
      const int non_zero_idx = nn_config->sparse_features[s_idx];
      const int sparse_size = input_layer->sparse_input_size[s_idx];
      if (non_zero_idx == sparse_size - 1) {
        continue;
      }
      for (int j = 0; j < num_out; j++) {
        W_sparse[s_idx][non_zero_idx * num_out + j] -=
            mu * dW_sparse[s_idx][non_zero_idx * num_out + j];
      }
    }
  }

  if (has_dense) {
    float *dW_dense = input_layer->dW_dense;
    float *W_dense = input_layer->dense_weights;
    for (int j = 0; j < num_dense * num_out; ++j) {
      W_dense[j] -= mu * dW_dense[j];
    }
  }
}

void av1_nn_softmax_em_c(const float *input, float *output, int n) {
  // Softmax function is invariant to adding the same constant
  // to all input values, so we subtract the maximum input to avoid
  // possible overflow.
  float max_inp = input[0];
  for (int i = 1; i < n; i++) max_inp = AOMMAX(max_inp, input[i]);
  float sum_out = 0.0f;
  for (int i = 0; i < n; i++) {
    // Clamp to range [-10.0, 0.0] to prevent FE_UNDERFLOW errors.
    const float normalized_input = AOMMAX(input[i] - max_inp, -10.0f);
    output[i] = (float)exp(normalized_input);
    sum_out += output[i];
  }
  for (int i = 0; i < n; i++) output[i] /= sum_out;
}
#endif  // CONFIG_INTRA_ENTROPY

static const aom_cdf_prob
    default_kf_y_mode_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][CDF_SIZE(
        INTRA_MODES)] = {
      { { AOM_CDF13(15588, 17027, 19338, 20218, 20682, 21110, 21825, 23244,
                    24189, 28165, 29093, 30466) },
        { AOM_CDF13(12016, 18066, 19516, 20303, 20719, 21444, 21888, 23032,
                    24434, 28658, 30172, 31409) },
        { AOM_CDF13(10052, 10771, 22296, 22788, 23055, 23239, 24133, 25620,
                    26160, 29336, 29929, 31567) },
        { AOM_CDF13(14091, 15406, 16442, 18808, 19136, 19546, 19998, 22096,
                    24746, 29585, 30958, 32462) },
        { AOM_CDF13(12122, 13265, 15603, 16501, 18609, 20033, 22391, 25583,
                    26437, 30261, 31073, 32475) } },
      { { AOM_CDF13(10023, 19585, 20848, 21440, 21832, 22760, 23089, 24023,
                    25381, 29014, 30482, 31436) },
        { AOM_CDF13(5983, 24099, 24560, 24886, 25066, 25795, 25913, 26423,
                    27610, 29905, 31276, 31794) },
        { AOM_CDF13(7444, 12781, 20177, 20728, 21077, 21607, 22170, 23405,
                    24469, 27915, 29090, 30492) },
        { AOM_CDF13(8537, 14689, 15432, 17087, 17408, 18172, 18408, 19825,
                    24649, 29153, 31096, 32210) },
        { AOM_CDF13(7543, 14231, 15496, 16195, 17905, 20717, 21984, 24516,
                    26001, 29675, 30981, 31994) } },
      { { AOM_CDF13(12613, 13591, 21383, 22004, 22312, 22577, 23401, 25055,
                    25729, 29538, 30305, 32077) },
        { AOM_CDF13(9687, 13470, 18506, 19230, 19604, 20147, 20695, 22062,
                    23219, 27743, 29211, 30907) },
        { AOM_CDF13(6183, 6505, 26024, 26252, 26366, 26434, 27082, 28354, 28555,
                    30467, 30794, 32086) },
        { AOM_CDF13(10718, 11734, 14954, 17224, 17565, 17924, 18561, 21523,
                    23878, 28975, 30287, 32252) },
        { AOM_CDF13(9194, 9858, 16501, 17263, 18424, 19171, 21563, 25961, 26561,
                    30072, 30737, 32463) } },
      { { AOM_CDF13(12602, 14399, 15488, 18381, 18778, 19315, 19724, 21419,
                    25060, 29696, 30917, 32409) },
        { AOM_CDF13(8203, 13821, 14524, 17105, 17439, 18131, 18404, 19468,
                    25225, 29485, 31158, 32342) },
        { AOM_CDF13(8451, 9731, 15004, 17643, 18012, 18425, 19070, 21538, 24605,
                    29118, 30078, 32018) },
        { AOM_CDF13(7714, 9048, 9516, 16667, 16817, 16994, 17153, 18767, 26743,
                    30389, 31536, 32528) },
        { AOM_CDF13(8843, 10280, 11496, 15317, 16652, 17943, 19108, 22718,
                    25769, 29953, 30983, 32485) } },
      { { AOM_CDF13(12578, 13671, 15979, 16834, 19075, 20913, 22989, 25449,
                    26219, 30214, 31150, 32477) },
        { AOM_CDF13(9563, 13626, 15080, 15892, 17756, 20863, 22207, 24236,
                    25380, 29653, 31143, 32277) },
        { AOM_CDF13(8356, 8901, 17616, 18256, 19350, 20106, 22598, 25947, 26466,
                    29900, 30523, 32261) },
        { AOM_CDF13(10835, 11815, 13124, 16042, 17018, 18039, 18947, 22753,
                    24615, 29489, 30883, 32482) },
        { AOM_CDF13(7618, 8288, 9859, 10509, 15386, 18657, 22903, 28776, 29180,
                    31355, 31802, 32593) } }
    };

static const aom_cdf_prob default_angle_delta_cdf[DIRECTIONAL_MODES][CDF_SIZE(
    2 * MAX_ANGLE_DELTA + 1)] = {
  { AOM_CDF7(2180, 5032, 7567, 22776, 26989, 30217) },
  { AOM_CDF7(2301, 5608, 8801, 23487, 26974, 30330) },
  { AOM_CDF7(3780, 11018, 13699, 19354, 23083, 31286) },
  { AOM_CDF7(4581, 11226, 15147, 17138, 21834, 28397) },
  { AOM_CDF7(1737, 10927, 14509, 19588, 22745, 28823) },
  { AOM_CDF7(2664, 10176, 12485, 17650, 21600, 30495) },
  { AOM_CDF7(2240, 11096, 15453, 20341, 22561, 28917) },
  { AOM_CDF7(3605, 10428, 12459, 17676, 21244, 30655) }
};

static const aom_cdf_prob default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    INTRA_MODES)] = { { AOM_CDF13(22801, 23489, 24293, 24756, 25601, 26123,
                                  26606, 27418, 27945, 29228, 29685, 30349) },
                      { AOM_CDF13(18673, 19845, 22631, 23318, 23950, 24649,
                                  25527, 27364, 28152, 29701, 29984, 30852) },
                      { AOM_CDF13(19770, 20979, 23396, 23939, 24241, 24654,
                                  25136, 27073, 27830, 29360, 29730, 30659) },
                      { AOM_CDF13(20155, 21301, 22838, 23178, 23261, 23533,
                                  23703, 24804, 25352, 26575, 27016, 28049) } };

static const aom_cdf_prob
    default_uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES][CDF_SIZE(
        UV_INTRA_MODES)] = {
      { { AOM_CDF13(22631, 24152, 25378, 25661, 25986, 26520, 27055, 27923,
                    28244, 30059, 30941, 31961) },
        { AOM_CDF13(9513, 26881, 26973, 27046, 27118, 27664, 27739, 27824,
                    28359, 29505, 29800, 31796) },
        { AOM_CDF13(9845, 9915, 28663, 28704, 28757, 28780, 29198, 29822, 29854,
                    30764, 31777, 32029) },
        { AOM_CDF13(13639, 13897, 14171, 25331, 25606, 25727, 25953, 27148,
                    28577, 30612, 31355, 32493) },
        { AOM_CDF13(9764, 9835, 9930, 9954, 25386, 27053, 27958, 28148, 28243,
                    31101, 31744, 32363) },
        { AOM_CDF13(11825, 13589, 13677, 13720, 15048, 29213, 29301, 29458,
                    29711, 31161, 31441, 32550) },
        { AOM_CDF13(14175, 14399, 16608, 16821, 17718, 17775, 28551, 30200,
                    30245, 31837, 32342, 32667) },
        { AOM_CDF13(12885, 13038, 14978, 15590, 15673, 15748, 16176, 29128,
                    29267, 30643, 31961, 32461) },
        { AOM_CDF13(12026, 13661, 13874, 15305, 15490, 15726, 15995, 16273,
                    28443, 30388, 30767, 32416) },
        { AOM_CDF13(19052, 19840, 20579, 20916, 21150, 21467, 21885, 22719,
                    23174, 28861, 30379, 32175) },
        { AOM_CDF13(18627, 19649, 20974, 21219, 21492, 21816, 22199, 23119,
                    23527, 27053, 31397, 32148) },
        { AOM_CDF13(17026, 19004, 19997, 20339, 20586, 21103, 21349, 21907,
                    22482, 25896, 26541, 31819) },
        { AOM_CDF13(12124, 13759, 14959, 14992, 15007, 15051, 15078, 15166,
                    15255, 15753, 16039, 16606) } },
      { { AOM_CDF14(10407, 11208, 12900, 13181, 13823, 14175, 14899, 15656,
                    15986, 20086, 20995, 22455, 24212) },
        { AOM_CDF14(4532, 19780, 20057, 20215, 20428, 21071, 21199, 21451,
                    22099, 24228, 24693, 27032, 29472) },
        { AOM_CDF14(5273, 5379, 20177, 20270, 20385, 20439, 20949, 21695, 21774,
                    23138, 24256, 24703, 26679) },
        { AOM_CDF14(6740, 7167, 7662, 14152, 14536, 14785, 15034, 16741, 18371,
                    21520, 22206, 23389, 24182) },
        { AOM_CDF14(4987, 5368, 5928, 6068, 19114, 20315, 21857, 22253, 22411,
                    24911, 25380, 26027, 26376) },
        { AOM_CDF14(5370, 6889, 7247, 7393, 9498, 21114, 21402, 21753, 21981,
                    24780, 25386, 26517, 27176) },
        { AOM_CDF14(4816, 4961, 7204, 7326, 8765, 8930, 20169, 20682, 20803,
                    23188, 23763, 24455, 24940) },
        { AOM_CDF14(6608, 6740, 8529, 9049, 9257, 9356, 9735, 18827, 19059,
                    22336, 23204, 23964, 24793) },
        { AOM_CDF14(5998, 7419, 7781, 8933, 9255, 9549, 9753, 10417, 18898,
                    22494, 23139, 24764, 25989) },
        { AOM_CDF14(10660, 11298, 12550, 12957, 13322, 13624, 14040, 15004,
                    15534, 20714, 21789, 23443, 24861) },
        { AOM_CDF14(10522, 11530, 12552, 12963, 13378, 13779, 14245, 15235,
                    15902, 20102, 22696, 23774, 25838) },
        { AOM_CDF14(10099, 10691, 12639, 13049, 13386, 13665, 14125, 15163,
                    15636, 19676, 20474, 23519, 25208) },
        { AOM_CDF14(3144, 5087, 7382, 7504, 7593, 7690, 7801, 8064, 8232, 9248,
                    9875, 10521, 29048) } }
    };

static const aom_cdf_prob default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(
    EXT_PARTITION_TYPES)] = {
  { AOM_CDF4(19132, 25510, 30392) },
  { AOM_CDF4(13928, 19855, 28540) },
  { AOM_CDF4(12522, 23679, 28629) },
  { AOM_CDF4(9896, 18783, 25853) },
  { AOM_CDF10(15597, 20929, 24571, 26706, 27664, 28821, 29601, 30571, 31902) },
  { AOM_CDF10(7925, 11043, 16785, 22470, 23971, 25043, 26651, 28701, 29834) },
  { AOM_CDF10(5414, 13269, 15111, 20488, 22360, 24500, 25537, 26336, 32117) },
  { AOM_CDF10(2662, 6362, 8614, 20860, 23053, 24778, 26436, 27829, 31171) },
  { AOM_CDF10(18462, 20920, 23124, 27647, 28227, 29049, 29519, 30178, 31544) },
  { AOM_CDF10(7689, 9060, 12056, 24992, 25660, 26182, 26951, 28041, 29052) },
  { AOM_CDF10(6015, 9009, 10062, 24544, 25409, 26545, 27071, 27526, 32047) },
  { AOM_CDF10(1394, 2208, 2796, 28614, 29061, 29466, 29840, 30185, 31899) },
  { AOM_CDF10(20137, 21547, 23078, 29566, 29837, 30261, 30524, 30892, 31724) },
  { AOM_CDF10(6732, 7490, 9497, 27944, 28250, 28515, 28969, 29630, 30104) },
  { AOM_CDF10(5945, 7663, 8348, 28683, 29117, 29749, 30064, 30298, 32238) },
  { AOM_CDF10(870, 1212, 1487, 31198, 31394, 31574, 31743, 31881, 32332) },
  { AOM_CDF8(27899, 28219, 28529, 32484, 32539, 32619, 32639) },
  { AOM_CDF8(6607, 6990, 8268, 32060, 32219, 32338, 32371) },
  { AOM_CDF8(5429, 6676, 7122, 32027, 32227, 32531, 32582) },
  { AOM_CDF8(711, 966, 1172, 32448, 32538, 32617, 32664) },
};

#if CONFIG_MODE_DEP_TX
static const aom_cdf_prob default_intra_ext_tx_cdf
    [EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
    [CDF_SIZE(TX_TYPES_NOMDTX)] = {
#else
static const aom_cdf_prob default_intra_ext_tx_cdf
    [EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES][CDF_SIZE(TX_TYPES)] = {
#endif
      {
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
      },
      {
          {
              { AOM_CDF7(1535, 8035, 9461, 12751, 23467, 27825) },
              { AOM_CDF7(564, 3335, 9709, 10870, 18143, 28094) },
              { AOM_CDF7(672, 3247, 3676, 11982, 19415, 23127) },
              { AOM_CDF7(5279, 13885, 15487, 18044, 23527, 30252) },
              { AOM_CDF7(4423, 6074, 7985, 10416, 25693, 29298) },
              { AOM_CDF7(1486, 4241, 9460, 10662, 16456, 27694) },
              { AOM_CDF7(439, 2838, 3522, 6737, 18058, 23754) },
              { AOM_CDF7(1190, 4233, 4855, 11670, 20281, 24377) },
              { AOM_CDF7(1045, 4312, 8647, 10159, 18644, 29335) },
              { AOM_CDF7(202, 3734, 4747, 7298, 17127, 24016) },
              { AOM_CDF7(447, 4312, 6819, 8884, 16010, 23858) },
              { AOM_CDF7(277, 4369, 5255, 8905, 16465, 22271) },
              { AOM_CDF7(3409, 5436, 10599, 15599, 19687, 24040) },
          },
          {
              { AOM_CDF7(1870, 13742, 14530, 16498, 23770, 27698) },
              { AOM_CDF7(326, 8796, 14632, 15079, 19272, 27486) },
              { AOM_CDF7(484, 7576, 7712, 14443, 19159, 22591) },
              { AOM_CDF7(1126, 15340, 15895, 17023, 20896, 30279) },
              { AOM_CDF7(655, 4854, 5249, 5913, 22099, 27138) },
              { AOM_CDF7(1299, 6458, 8885, 9290, 14851, 25497) },
              { AOM_CDF7(311, 5295, 5552, 6885, 16107, 22672) },
              { AOM_CDF7(883, 8059, 8270, 11258, 17289, 21549) },
              { AOM_CDF7(741, 7580, 9318, 10345, 16688, 29046) },
              { AOM_CDF7(110, 7406, 7915, 9195, 16041, 23329) },
              { AOM_CDF7(363, 7974, 9357, 10673, 15629, 24474) },
              { AOM_CDF7(153, 7647, 8112, 9936, 15307, 19996) },
              { AOM_CDF7(3511, 6332, 11165, 15335, 19323, 23594) },
          },
          {
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
          },
          {
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
          },
      },
      {
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
          {
              { AOM_CDF5(1127, 12814, 22772, 27483) },
              { AOM_CDF5(145, 6761, 11980, 26667) },
              { AOM_CDF5(362, 5887, 11678, 16725) },
              { AOM_CDF5(385, 15213, 18587, 30693) },
              { AOM_CDF5(25, 2914, 23134, 27903) },
              { AOM_CDF5(60, 4470, 11749, 23991) },
              { AOM_CDF5(37, 3332, 14511, 21448) },
              { AOM_CDF5(157, 6320, 13036, 17439) },
              { AOM_CDF5(119, 6719, 12906, 29396) },
              { AOM_CDF5(47, 5537, 12576, 21499) },
              { AOM_CDF5(269, 6076, 11258, 23115) },
              { AOM_CDF5(83, 5615, 12001, 17228) },
              { AOM_CDF5(1968, 5556, 12023, 18547) },
          },
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
      },
    };

#if CONFIG_MODE_DEP_TX
static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(
        TX_TYPES_NOMDTX)] = {
#else
static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(
        TX_TYPES)] = {
#endif
      {
          { 0 },
          { 0 },
          { 0 },
          { 0 },
      },
      {
          { AOM_CDF16(4458, 5560, 7695, 9709, 13330, 14789, 17537, 20266, 21504,
                      22848, 23934, 25474, 27727, 28915, 30631) },
          { AOM_CDF16(1645, 2573, 4778, 5711, 7807, 8622, 10522, 15357, 17674,
                      20408, 22517, 25010, 27116, 28856, 30749) },
          { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                      20480, 22528, 24576, 26624, 28672, 30720) },
          { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                      20480, 22528, 24576, 26624, 28672, 30720) },
      },
      {
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
          { AOM_CDF12(770, 2421, 5225, 12907, 15819, 18927, 21561, 24089, 26595,
                      28526, 30529) },
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
      },
      {
          { AOM_CDF2(16384) },
          { AOM_CDF2(4167) },
          { AOM_CDF2(1998) },
          { AOM_CDF2(748) },
      },
    };

#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTER
static const aom_cdf_prob
    default_mdtx_type_inter_cdf[EXT_TX_SIZES][CDF_SIZE(MDTX_TYPES_INTER)] = {
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
    };

static const aom_cdf_prob default_use_mdtx_inter_cdf[EXT_TX_SIZES]
                                                    [CDF_SIZE(2)] = {
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                    };
#endif  // USE_MDTX_INTER
#if USE_MDTX_INTRA
static const aom_cdf_prob
    default_mdtx_type_intra_cdf[EXT_TX_SIZES][INTRA_MODES]
                               [CDF_SIZE(MDTX_TYPES_INTRA)] = {
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                               };

static const aom_cdf_prob default_use_mdtx_intra_cdf[EXT_TX_SIZES][INTRA_MODES]
                                                    [CDF_SIZE(2)] = {
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      }
                                                    };
#endif  // USE_MDTX_INTRA
#endif

static const aom_cdf_prob default_cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)] = {
  AOM_CDF8(1418, 2123, 13340, 18405, 26972, 28343, 32294)
};

static const aom_cdf_prob
    default_cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)] = {
      { AOM_CDF16(7637, 20719, 31401, 32481, 32657, 32688, 32692, 32696, 32700,
                  32704, 32708, 32712, 32716, 32720, 32724) },
      { AOM_CDF16(14365, 23603, 28135, 31168, 32167, 32395, 32487, 32573, 32620,
                  32647, 32668, 32672, 32676, 32680, 32684) },
      { AOM_CDF16(11532, 22380, 28445, 31360, 32349, 32523, 32584, 32649, 32673,
                  32677, 32681, 32685, 32689, 32693, 32697) },
      { AOM_CDF16(26990, 31402, 32282, 32571, 32692, 32696, 32700, 32704, 32708,
                  32712, 32716, 32720, 32724, 32728, 32732) },
      { AOM_CDF16(17248, 26058, 28904, 30608, 31305, 31877, 32126, 32321, 32394,
                  32464, 32516, 32560, 32576, 32593, 32622) },
      { AOM_CDF16(14738, 21678, 25779, 27901, 29024, 30302, 30980, 31843, 32144,
                  32413, 32520, 32594, 32622, 32656, 32660) }
    };

static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(
        SWITCHABLE_FILTERS)] = {
      { AOM_CDF3(31935, 32720) }, { AOM_CDF3(5568, 32719) },
      { AOM_CDF3(422, 2938) },    { AOM_CDF3(28244, 32608) },
      { AOM_CDF3(31206, 31953) }, { AOM_CDF3(4862, 32121) },
      { AOM_CDF3(770, 1152) },    { AOM_CDF3(20889, 25637) },
      { AOM_CDF3(31910, 32724) }, { AOM_CDF3(4120, 32712) },
      { AOM_CDF3(305, 2247) },    { AOM_CDF3(27403, 32636) },
      { AOM_CDF3(31022, 32009) }, { AOM_CDF3(2963, 32093) },
      { AOM_CDF3(601, 943) },     { AOM_CDF3(14969, 21398) }
    };

static const aom_cdf_prob default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(24035) }, { AOM_CDF2(16630) }, { AOM_CDF2(15339) },
            { AOM_CDF2(8386) },  { AOM_CDF2(12222) }, { AOM_CDF2(4676) } };

static const aom_cdf_prob default_zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(2175) }, { AOM_CDF2(1054) } };

static const aom_cdf_prob default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(23974) }, { AOM_CDF2(24188) }, { AOM_CDF2(17848) },
            { AOM_CDF2(28622) }, { AOM_CDF2(24312) }, { AOM_CDF2(19923) } };

static const aom_cdf_prob default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_CDF2(13104) }, { AOM_CDF2(24560) }, { AOM_CDF2(18945) }
};

static const aom_cdf_prob
    default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(
        INTER_COMPOUND_MODES)] = {
      { AOM_CDF8(7760, 13823, 15808, 17641, 19156, 20666, 26891) },
      { AOM_CDF8(10730, 19452, 21145, 22749, 24039, 25131, 28724) },
      { AOM_CDF8(10664, 20221, 21588, 22906, 24295, 25387, 28436) },
      { AOM_CDF8(13298, 16984, 20471, 24182, 25067, 25736, 26422) },
      { AOM_CDF8(18904, 23325, 25242, 27432, 27898, 28258, 30758) },
      { AOM_CDF8(10725, 17454, 20124, 22820, 24195, 25168, 26046) },
      { AOM_CDF8(17125, 24273, 25814, 27492, 28214, 28704, 30592) },
      { AOM_CDF8(13046, 23214, 24505, 25942, 27435, 28442, 29330) }
    };

static const aom_cdf_prob default_interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    2)] = { { AOM_CDF2(16384) },
            { AOM_CDF2(26887) },
            { AOM_CDF2(27597) },
            { AOM_CDF2(30237) } };

static const aom_cdf_prob
    default_interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
        INTERINTRA_MODES)] = { { AOM_CDF4(8192, 16384, 24576) },
                               { AOM_CDF4(1875, 11082, 27332) },
                               { AOM_CDF4(2473, 9996, 26388) },
                               { AOM_CDF4(4238, 11537, 25926) } };

static const aom_cdf_prob
    default_wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(20036) }, { AOM_CDF2(24957) }, { AOM_CDF2(26704) },
      { AOM_CDF2(27530) }, { AOM_CDF2(29564) }, { AOM_CDF2(29444) },
      { AOM_CDF2(26872) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(
    MASKED_COMPOUND_TYPES)] = {
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(23431) }, { AOM_CDF2(13171) }, { AOM_CDF2(11470) },
  { AOM_CDF2(9770) },  { AOM_CDF2(9100) },  { AOM_CDF2(8233) },
  { AOM_CDF2(6172) },  { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(11820) }, { AOM_CDF2(7701) },  { AOM_CDF2(16384) },
  { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
};

static const aom_cdf_prob
    default_wedge_idx_cdf[BLOCK_SIZES_ALL][CDF_SIZE(16)] = {
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2438, 4440, 6599, 8663, 11005, 12874, 15751, 18094, 20359,
                  22362, 24127, 25702, 27752, 29450, 31171) },
      { AOM_CDF16(806, 3266, 6005, 6738, 7218, 7367, 7771, 14588, 16323, 17367,
                  18452, 19422, 22839, 26127, 29629) },
      { AOM_CDF16(2779, 3738, 4683, 7213, 7775, 8017, 8655, 14357, 17939, 21332,
                  24520, 27470, 29456, 30529, 31656) },
      { AOM_CDF16(1684, 3625, 5675, 7108, 9302, 11274, 14429, 17144, 19163,
                  20961, 22884, 24471, 26719, 28714, 30877) },
      { AOM_CDF16(1142, 3491, 6277, 7314, 8089, 8355, 9023, 13624, 15369, 16730,
                  18114, 19313, 22521, 26012, 29550) },
      { AOM_CDF16(2742, 4195, 5727, 8035, 8980, 9336, 10146, 14124, 17270,
                  20533, 23434, 25972, 27944, 29570, 31416) },
      { AOM_CDF16(1727, 3948, 6101, 7796, 9841, 12344, 15766, 18944, 20638,
                  22038, 23963, 25311, 26988, 28766, 31012) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(154, 987, 1925, 2051, 2088, 2111, 2151, 23033, 23703, 24284,
                  24985, 25684, 27259, 28883, 30911) },
      { AOM_CDF16(1135, 1322, 1493, 2635, 2696, 2737, 2770, 21016, 22935, 25057,
                  27251, 29173, 30089, 30960, 31933) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(10923, 21845) },
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(7651, 24760) },
      { AOM_CDF3(4738, 24765) },  { AOM_CDF3(5391, 25528) },
      { AOM_CDF3(19419, 26810) }, { AOM_CDF3(5123, 23606) },
      { AOM_CDF3(11606, 24308) }, { AOM_CDF3(26260, 29116) },
      { AOM_CDF3(20360, 28062) }, { AOM_CDF3(21679, 26830) },
      { AOM_CDF3(29516, 30701) }, { AOM_CDF3(28898, 30397) },
      { AOM_CDF3(30878, 31335) }, { AOM_CDF3(32507, 32558) },
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(10923, 21845) },
      { AOM_CDF3(28799, 31390) }, { AOM_CDF3(26431, 30774) },
      { AOM_CDF3(28973, 31594) }, { AOM_CDF3(29742, 31203) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF3(16384, 24576) }, { AOM_CDF3(16384, 24576) },
      { AOM_CDF3(16384, 27000) }, { AOM_CDF3(16384, 27000) },
      { AOM_CDF3(16384, 27000) }, { AOM_CDF3(16384, 27000) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(10437) }, { AOM_CDF2(9371) },  { AOM_CDF2(9301) },
  { AOM_CDF2(17432) }, { AOM_CDF2(14423) }, { AOM_CDF2(15142) },
  { AOM_CDF2(25817) }, { AOM_CDF2(22823) }, { AOM_CDF2(22083) },
  { AOM_CDF2(30128) }, { AOM_CDF2(31014) }, { AOM_CDF2(31560) },
  { AOM_CDF2(32638) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(23664) }, { AOM_CDF2(20901) }, { AOM_CDF2(24008) },
  { AOM_CDF2(26879) },
#if CONFIG_FLEX_PARTITION
  { AOM_CDF2(24000) }, { AOM_CDF2(24000) }, { AOM_CDF2(24000) },
  { AOM_CDF2(24000) }, { AOM_CDF2(24000) }, { AOM_CDF2(24000) },
#endif  // CONFIG_FLEX_PARTITION
};

static const aom_cdf_prob default_intra_inter_cdf[INTRA_INTER_CONTEXTS]
                                                 [CDF_SIZE(2)] = {
                                                   { AOM_CDF2(806) },
                                                   { AOM_CDF2(16662) },
                                                   { AOM_CDF2(20186) },
                                                   { AOM_CDF2(26538) }
                                                 };

static const aom_cdf_prob default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(26828) },
            { AOM_CDF2(24035) },
            { AOM_CDF2(12031) },
            { AOM_CDF2(10640) },
            { AOM_CDF2(2901) } };

static const aom_cdf_prob default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS]
                                                   [CDF_SIZE(2)] = {
                                                     { AOM_CDF2(1198) },
                                                     { AOM_CDF2(2070) },
                                                     { AOM_CDF2(9166) },
                                                     { AOM_CDF2(7499) },
                                                     { AOM_CDF2(22475) }
                                                   };

static const aom_cdf_prob
    default_uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS -
                                                    1][CDF_SIZE(2)] = {
      { { AOM_CDF2(5284) }, { AOM_CDF2(3865) }, { AOM_CDF2(3128) } },
      { { AOM_CDF2(23152) }, { AOM_CDF2(14173) }, { AOM_CDF2(15270) } },
      { { AOM_CDF2(31774) }, { AOM_CDF2(25120) }, { AOM_CDF2(26710) } }
    };

static const aom_cdf_prob default_single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1]
                                                [CDF_SIZE(2)] = {
                                                  { { AOM_CDF2(4897) },
                                                    { AOM_CDF2(1555) },
                                                    { AOM_CDF2(4236) },
                                                    { AOM_CDF2(8650) },
                                                    { AOM_CDF2(904) },
                                                    { AOM_CDF2(1444) } },
                                                  { { AOM_CDF2(16973) },
                                                    { AOM_CDF2(16751) },
                                                    { AOM_CDF2(19647) },
                                                    { AOM_CDF2(24773) },
                                                    { AOM_CDF2(11014) },
                                                    { AOM_CDF2(15087) } },
                                                  { { AOM_CDF2(29744) },
                                                    { AOM_CDF2(30279) },
                                                    { AOM_CDF2(31194) },
                                                    { AOM_CDF2(31895) },
                                                    { AOM_CDF2(26875) },
                                                    { AOM_CDF2(30304) } }
                                                };

static const aom_cdf_prob
    default_comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)] = {
      { { AOM_CDF2(4946) }, { AOM_CDF2(9468) }, { AOM_CDF2(1503) } },
      { { AOM_CDF2(19891) }, { AOM_CDF2(22441) }, { AOM_CDF2(15160) } },
      { { AOM_CDF2(30731) }, { AOM_CDF2(31059) }, { AOM_CDF2(27544) } }
    };

static const aom_cdf_prob
    default_comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)] = {
      { { AOM_CDF2(2235) }, { AOM_CDF2(1423) } },
      { { AOM_CDF2(17182) }, { AOM_CDF2(15175) } },
      { { AOM_CDF2(30606) }, { AOM_CDF2(30489) } }
    };

static const aom_cdf_prob
    default_palette_y_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)] = {
      { AOM_CDF7(7952, 13000, 18149, 21478, 25527, 29241) },
      { AOM_CDF7(7139, 11421, 16195, 19544, 23666, 28073) },
      { AOM_CDF7(7788, 12741, 17325, 20500, 24315, 28530) },
      { AOM_CDF7(8271, 14064, 18246, 21564, 25071, 28533) },
      { AOM_CDF7(12725, 19180, 21863, 24839, 27535, 30120) },
      { AOM_CDF7(9711, 14888, 16923, 21052, 25661, 27875) },
      { AOM_CDF7(14940, 20797, 21678, 24186, 27033, 28999) }
    };

static const aom_cdf_prob
    default_palette_uv_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)] = {
      { AOM_CDF7(8713, 19979, 27128, 29609, 31331, 32272) },
      { AOM_CDF7(5839, 15573, 23581, 26947, 29848, 31700) },
      { AOM_CDF7(4426, 11260, 17999, 21483, 25863, 29430) },
      { AOM_CDF7(3228, 9464, 14993, 18089, 22523, 27420) },
      { AOM_CDF7(3768, 8886, 13091, 17852, 22495, 27207) },
      { AOM_CDF7(2464, 8451, 12861, 21632, 25525, 28555) },
      { AOM_CDF7(1269, 5435, 10433, 18963, 21700, 25865) }
    };

static const aom_cdf_prob default_palette_y_mode_cdf
    [PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][CDF_SIZE(2)] = {
      { { AOM_CDF2(31676) }, { AOM_CDF2(3419) }, { AOM_CDF2(1261) } },
      { { AOM_CDF2(31912) }, { AOM_CDF2(2859) }, { AOM_CDF2(980) } },
      { { AOM_CDF2(31823) }, { AOM_CDF2(3400) }, { AOM_CDF2(781) } },
      { { AOM_CDF2(32030) }, { AOM_CDF2(3561) }, { AOM_CDF2(904) } },
      { { AOM_CDF2(32309) }, { AOM_CDF2(7337) }, { AOM_CDF2(1462) } },
      { { AOM_CDF2(32265) }, { AOM_CDF2(4015) }, { AOM_CDF2(1521) } },
      { { AOM_CDF2(32450) }, { AOM_CDF2(7946) }, { AOM_CDF2(129) } }
    };

static const aom_cdf_prob
    default_palette_uv_mode_cdf[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(32461) }, { AOM_CDF2(21488) }
    };

static const aom_cdf_prob default_palette_y_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_CDF2(28710) },
          { AOM_CDF2(16384) },
          { AOM_CDF2(10553) },
          { AOM_CDF2(27036) },
          { AOM_CDF2(31603) },
      },
      {
          { AOM_CDF3(27877, 30490) },
          { AOM_CDF3(11532, 25697) },
          { AOM_CDF3(6544, 30234) },
          { AOM_CDF3(23018, 28072) },
          { AOM_CDF3(31915, 32385) },
      },
      {
          { AOM_CDF4(25572, 28046, 30045) },
          { AOM_CDF4(9478, 21590, 27256) },
          { AOM_CDF4(7248, 26837, 29824) },
          { AOM_CDF4(19167, 24486, 28349) },
          { AOM_CDF4(31400, 31825, 32250) },
      },
      {
          { AOM_CDF5(24779, 26955, 28576, 30282) },
          { AOM_CDF5(8669, 20364, 24073, 28093) },
          { AOM_CDF5(4255, 27565, 29377, 31067) },
          { AOM_CDF5(19864, 23674, 26716, 29530) },
          { AOM_CDF5(31646, 31893, 32147, 32426) },
      },
      {
          { AOM_CDF6(23132, 25407, 26970, 28435, 30073) },
          { AOM_CDF6(7443, 17242, 20717, 24762, 27982) },
          { AOM_CDF6(6300, 24862, 26944, 28784, 30671) },
          { AOM_CDF6(18916, 22895, 25267, 27435, 29652) },
          { AOM_CDF6(31270, 31550, 31808, 32059, 32353) },
      },
      {
          { AOM_CDF7(23105, 25199, 26464, 27684, 28931, 30318) },
          { AOM_CDF7(6950, 15447, 18952, 22681, 25567, 28563) },
          { AOM_CDF7(7560, 23474, 25490, 27203, 28921, 30708) },
          { AOM_CDF7(18544, 22373, 24457, 26195, 28119, 30045) },
          { AOM_CDF7(31198, 31451, 31670, 31882, 32123, 32391) },
      },
      {
          { AOM_CDF8(21689, 23883, 25163, 26352, 27506, 28827, 30195) },
          { AOM_CDF8(6892, 15385, 17840, 21606, 24287, 26753, 29204) },
          { AOM_CDF8(5651, 23182, 25042, 26518, 27982, 29392, 30900) },
          { AOM_CDF8(19349, 22578, 24418, 25994, 27524, 29031, 30448) },
          { AOM_CDF8(31028, 31270, 31504, 31705, 31927, 32153, 32392) },
      },
    };

static const aom_cdf_prob default_palette_uv_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_CDF2(29089) },
          { AOM_CDF2(16384) },
          { AOM_CDF2(8713) },
          { AOM_CDF2(29257) },
          { AOM_CDF2(31610) },
      },
      {
          { AOM_CDF3(25257, 29145) },
          { AOM_CDF3(12287, 27293) },
          { AOM_CDF3(7033, 27960) },
          { AOM_CDF3(20145, 25405) },
          { AOM_CDF3(30608, 31639) },
      },
      {
          { AOM_CDF4(24210, 27175, 29903) },
          { AOM_CDF4(9888, 22386, 27214) },
          { AOM_CDF4(5901, 26053, 29293) },
          { AOM_CDF4(18318, 22152, 28333) },
          { AOM_CDF4(30459, 31136, 31926) },
      },
      {
          { AOM_CDF5(22980, 25479, 27781, 29986) },
          { AOM_CDF5(8413, 21408, 24859, 28874) },
          { AOM_CDF5(2257, 29449, 30594, 31598) },
          { AOM_CDF5(19189, 21202, 25915, 28620) },
          { AOM_CDF5(31844, 32044, 32281, 32518) },
      },
      {
          { AOM_CDF6(22217, 24567, 26637, 28683, 30548) },
          { AOM_CDF6(7307, 16406, 19636, 24632, 28424) },
          { AOM_CDF6(4441, 25064, 26879, 28942, 30919) },
          { AOM_CDF6(17210, 20528, 23319, 26750, 29582) },
          { AOM_CDF6(30674, 30953, 31396, 31735, 32207) },
      },
      {
          { AOM_CDF7(21239, 23168, 25044, 26962, 28705, 30506) },
          { AOM_CDF7(6545, 15012, 18004, 21817, 25503, 28701) },
          { AOM_CDF7(3448, 26295, 27437, 28704, 30126, 31442) },
          { AOM_CDF7(15889, 18323, 21704, 24698, 26976, 29690) },
          { AOM_CDF7(30988, 31204, 31479, 31734, 31983, 32325) },
      },
      {
          { AOM_CDF8(21442, 23288, 24758, 26246, 27649, 28980, 30563) },
          { AOM_CDF8(5863, 14933, 17552, 20668, 23683, 26411, 29273) },
          { AOM_CDF8(3415, 25810, 26877, 27990, 29223, 30394, 31618) },
          { AOM_CDF8(17965, 20084, 22232, 23974, 26274, 28402, 30390) },
          { AOM_CDF8(31190, 31329, 31516, 31679, 31825, 32026, 32322) },
      },
    };

#if CONFIG_NEW_TX_PARTITION
static const aom_cdf_prob
    default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(
        TX_PARTITION_TYPES)] = {
      { AOM_CDF2(28581) }, { AOM_CDF2(23846) }, { AOM_CDF2(20847) },
      { AOM_CDF2(24315) }, { AOM_CDF2(18196) }, { AOM_CDF2(12133) },
      { AOM_CDF2(18791) }, { AOM_CDF2(10887) }, { AOM_CDF2(11005) },
      { AOM_CDF2(27179) }, { AOM_CDF2(20004) }, { AOM_CDF2(11281) },
      { AOM_CDF2(26549) }, { AOM_CDF2(19308) }, { AOM_CDF2(14224) },
      { AOM_CDF2(28015) }, { AOM_CDF2(21546) }, { AOM_CDF2(14400) },
      { AOM_CDF2(28165) }, { AOM_CDF2(22401) }, { AOM_CDF2(16088) }
    };
#else
static const aom_cdf_prob
    default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(28581) }, { AOM_CDF2(23846) }, { AOM_CDF2(20847) },
      { AOM_CDF2(24315) }, { AOM_CDF2(18196) }, { AOM_CDF2(12133) },
      { AOM_CDF2(18791) }, { AOM_CDF2(10887) }, { AOM_CDF2(11005) },
      { AOM_CDF2(27179) }, { AOM_CDF2(20004) }, { AOM_CDF2(11281) },
      { AOM_CDF2(26549) }, { AOM_CDF2(19308) }, { AOM_CDF2(14224) },
      { AOM_CDF2(28015) }, { AOM_CDF2(21546) }, { AOM_CDF2(14400) },
      { AOM_CDF2(28165) }, { AOM_CDF2(22401) }, { AOM_CDF2(16088) }
    };
#endif  // CONFIG_NEW_TX_PARTITION

static const aom_cdf_prob default_skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_CDF2(31671) }, { AOM_CDF2(16515) }, { AOM_CDF2(4576) }
};

static const aom_cdf_prob default_skip_mode_cdfs[SKIP_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(32621) }, { AOM_CDF2(20708) }, { AOM_CDF2(8127) } };

static const aom_cdf_prob
    default_compound_idx_cdfs[COMP_INDEX_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(18244) }, { AOM_CDF2(12865) }, { AOM_CDF2(7053) },
      { AOM_CDF2(13259) }, { AOM_CDF2(9334) },  { AOM_CDF2(4644) }
    };

static const aom_cdf_prob
    default_comp_group_idx_cdfs[COMP_GROUP_IDX_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(26607) }, { AOM_CDF2(22891) }, { AOM_CDF2(18840) },
      { AOM_CDF2(24594) }, { AOM_CDF2(19934) }, { AOM_CDF2(22674) }
    };

static const aom_cdf_prob default_intrabc_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    30531) };

static const aom_cdf_prob default_filter_intra_mode_cdf[CDF_SIZE(
    FILTER_INTRA_MODES)] = { AOM_CDF5(8949, 12776, 17211, 29558) };

static const aom_cdf_prob
    default_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(4621) },  { AOM_CDF2(6743) },  { AOM_CDF2(5893) },
      { AOM_CDF2(7866) },  { AOM_CDF2(12551) }, { AOM_CDF2(9394) },
      { AOM_CDF2(12408) }, { AOM_CDF2(14301) }, { AOM_CDF2(12756) },
      { AOM_CDF2(22343) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(12770) }, { AOM_CDF2(10368) },
      { AOM_CDF2(20229) }, { AOM_CDF2(18101) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };

#if CONFIG_ADAPT_FILTER_INTRA
static const aom_cdf_prob default_adapt_filter_intra_mode_cdf[CDF_SIZE(
    USED_ADAPT_FILTER_INTRA_MODES)] =
#if USED_ADAPT_FILTER_INTRA_MODES == 3
    { AOM_CDF3(10922, 10922) };
#elif USED_ADAPT_FILTER_INTRA_MODES == 5
    { AOM_CDF5(6553, 13106, 19659, 26212) };
#elif USED_ADAPT_FILTER_INTRA_MODES == 7
    { AOM_CDF7(4681, 9362, 14043, 18724, 23405, 28086) };
#endif

static const aom_cdf_prob
    default_adapt_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };
#endif  // CONFIG_ADAPT_FILTER_INTRA

#if CONFIG_LOOP_RESTORE_CNN
static const aom_cdf_prob default_switchable_restore_cdf[CDF_SIZE(
    RESTORE_SWITCHABLE_TYPES)] = { AOM_CDF4(6000, 14000, 22500) };
#else
static const aom_cdf_prob default_switchable_restore_cdf[CDF_SIZE(
    RESTORE_SWITCHABLE_TYPES)] = { AOM_CDF3(9413, 22581) };
#endif  // CONFIG_LOOP_RESTORE_CNN

static const aom_cdf_prob default_wiener_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    11570) };

static const aom_cdf_prob default_sgrproj_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    16855) };

#if CONFIG_LOOP_RESTORE_CNN
static const aom_cdf_prob default_cnn_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    20000) };
#endif  // CONFIG_LOOP_RESTORE_CNN

static const aom_cdf_prob default_delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)] = {
  AOM_CDF4(28160, 32120, 32677)
};

static const aom_cdf_prob default_delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(
    DELTA_LF_PROBS + 1)] = { { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) } };
static const aom_cdf_prob default_delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)] = {
  AOM_CDF4(28160, 32120, 32677)
};

// FIXME(someone) need real defaults here
static const aom_cdf_prob default_seg_tree_cdf[CDF_SIZE(MAX_SEGMENTS)] = {
  AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672)
};

static const aom_cdf_prob
    default_segment_pred_cdf[SEG_TEMPORAL_PRED_CTXS][CDF_SIZE(2)] = {
      { AOM_CDF2(128 * 128) }, { AOM_CDF2(128 * 128) }, { AOM_CDF2(128 * 128) }
    };

static const aom_cdf_prob
    default_spatial_pred_seg_tree_cdf[SPATIAL_PREDICTION_PROBS][CDF_SIZE(
        MAX_SEGMENTS)] = {
      {
          AOM_CDF8(5622, 7893, 16093, 18233, 27809, 28373, 32533),
      },
      {
          AOM_CDF8(14274, 18230, 22557, 24935, 29980, 30851, 32344),
      },
      {
          AOM_CDF8(27527, 28487, 28723, 28890, 32397, 32647, 32679),
      },
    };

static const aom_cdf_prob default_tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS]
                                             [CDF_SIZE(MAX_TX_DEPTH + 1)] = {
                                               { { AOM_CDF2(19968) },
                                                 { AOM_CDF2(19968) },
                                                 { AOM_CDF2(24320) } },
                                               { { AOM_CDF3(12272, 30172) },
                                                 { AOM_CDF3(12272, 30172) },
                                                 { AOM_CDF3(18677, 30848) } },
                                               { { AOM_CDF3(12986, 15180) },
                                                 { AOM_CDF3(12986, 15180) },
                                                 { AOM_CDF3(24302, 25602) } },
                                               { { AOM_CDF3(5782, 11475) },
                                                 { AOM_CDF3(5782, 11475) },
                                                 { AOM_CDF3(16803, 22759) } },
                                             };

#define MAX_COLOR_CONTEXT_HASH 8
// Negative values are invalid
static const int palette_color_index_context_lookup[MAX_COLOR_CONTEXT_HASH +
                                                    1] = { -1, -1, 0, -1, -1,
                                                           4,  3,  2, 1 };

#define NUM_PALETTE_NEIGHBORS 3  // left, top-left and top.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx) {
  assert(palette_size <= PALETTE_MAX_SIZE);
  assert(r > 0 || c > 0);

  // Get color indices of neighbors.
  int color_neighbors[NUM_PALETTE_NEIGHBORS];
  color_neighbors[0] = (c - 1 >= 0) ? color_map[r * stride + c - 1] : -1;
  color_neighbors[1] =
      (c - 1 >= 0 && r - 1 >= 0) ? color_map[(r - 1) * stride + c - 1] : -1;
  color_neighbors[2] = (r - 1 >= 0) ? color_map[(r - 1) * stride + c] : -1;

  // The +10 below should not be needed. But we get a warning "array subscript
  // is above array bounds [-Werror=array-bounds]" without it, possibly due to
  // this (or similar) bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
  int scores[PALETTE_MAX_SIZE + 10] = { 0 };
  int i;
  static const int weights[NUM_PALETTE_NEIGHBORS] = { 2, 1, 2 };
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    if (color_neighbors[i] >= 0) {
      scores[color_neighbors[i]] += weights[i];
    }
  }

  int inverse_color_order[PALETTE_MAX_SIZE];
  for (i = 0; i < PALETTE_MAX_SIZE; ++i) {
    color_order[i] = i;
    inverse_color_order[i] = i;
  }

  // Get the top NUM_PALETTE_NEIGHBORS scores (sorted from large to small).
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    int max = scores[i];
    int max_idx = i;
    for (int j = i + 1; j < palette_size; ++j) {
      if (scores[j] > max) {
        max = scores[j];
        max_idx = j;
      }
    }
    if (max_idx != i) {
      // Move the score at index 'max_idx' to index 'i', and shift the scores
      // from 'i' to 'max_idx - 1' by 1.
      const int max_score = scores[max_idx];
      const uint8_t max_color_order = color_order[max_idx];
      for (int k = max_idx; k > i; --k) {
        scores[k] = scores[k - 1];
        color_order[k] = color_order[k - 1];
        inverse_color_order[color_order[k]] = k;
      }
      scores[i] = max_score;
      color_order[i] = max_color_order;
      inverse_color_order[color_order[i]] = i;
    }
  }

  if (color_idx != NULL)
    *color_idx = inverse_color_order[color_map[r * stride + c]];

  // Get hash value of context.
  int color_index_ctx_hash = 0;
  static const int hash_multipliers[NUM_PALETTE_NEIGHBORS] = { 1, 2, 2 };
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    color_index_ctx_hash += scores[i] * hash_multipliers[i];
  }
  assert(color_index_ctx_hash > 0);
  assert(color_index_ctx_hash <= MAX_COLOR_CONTEXT_HASH);

  // Lookup context from hash.
  const int color_index_ctx =
      palette_color_index_context_lookup[color_index_ctx_hash];
  assert(color_index_ctx >= 0);
  assert(color_index_ctx < PALETTE_COLOR_INDEX_CONTEXTS);
  return color_index_ctx;
}
#undef NUM_PALETTE_NEIGHBORS
#undef MAX_COLOR_CONTEXT_HASH

static void init_mode_probs(FRAME_CONTEXT *fc) {
  av1_copy(fc->palette_y_size_cdf, default_palette_y_size_cdf);
  av1_copy(fc->palette_uv_size_cdf, default_palette_uv_size_cdf);
  av1_copy(fc->palette_y_color_index_cdf, default_palette_y_color_index_cdf);
  av1_copy(fc->palette_uv_color_index_cdf, default_palette_uv_color_index_cdf);
  av1_copy(fc->kf_y_cdf, default_kf_y_mode_cdf);
#if CONFIG_INTRA_ENTROPY
  NN_CONFIG_EM *intra_y_model = &fc->av1_intra_y_mode;
  av1_zero(*intra_y_model);
  intra_y_model->lr = intra_y_mode_lr;
  intra_y_model->num_hidden_layers = 0;
  intra_y_model->input_layer.num_sparse_inputs = EM_NUM_Y_SPARSE_FEATURES;
  intra_y_model->input_layer.num_dense_inputs = EM_NUM_Y_DENSE_FEATURES;
  intra_y_model->input_layer.sparse_input_size[0] = EM_Y_SPARSE_FEAT_SIZE_0;
  intra_y_model->input_layer.sparse_input_size[1] = EM_Y_SPARSE_FEAT_SIZE_1;
  intra_y_model->input_layer.num_outputs = INTRA_MODES;
  intra_y_model->input_layer.activation = ACTN_NONE;
  intra_y_model->num_logits = INTRA_MODES;
  intra_y_model->loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  for (int sparse_idx = 0;
       sparse_idx < intra_y_model->input_layer.num_sparse_inputs;
       sparse_idx++) {
    const int arr_size =
        intra_y_model->input_layer.sparse_input_size[sparse_idx] *
        intra_y_model->input_layer.num_outputs;
    av1_copy_array(intra_y_model->input_layer.sparse_weights[sparse_idx],
                   intra_y_mode_layer0_sparse_weights[sparse_idx], arr_size);
  }
  av1_copy(intra_y_model->input_layer.dense_weights,
           intra_y_mode_layer0_dense_weights);
  av1_copy(intra_y_model->input_layer.bias, intra_y_mode_layer0_bias);

  NN_CONFIG_EM *intra_uv_model = &fc->av1_intra_uv_mode;
  av1_zero(*intra_uv_model);
  intra_uv_model->lr = intra_uv_mode_lr;
  intra_uv_model->num_hidden_layers = 0;
  intra_uv_model->input_layer.num_sparse_inputs = EM_NUM_UV_SPARSE_FEATURES;
  intra_uv_model->input_layer.num_dense_inputs = EM_NUM_Y_DENSE_FEATURES;
  intra_uv_model->input_layer.sparse_input_size[0] = EM_UV_SPARSE_FEAT_SIZE_0;
  intra_uv_model->input_layer.sparse_input_size[1] = EM_UV_SPARSE_FEAT_SIZE_1;
  intra_uv_model->input_layer.num_outputs = UV_INTRA_MODES;
  intra_uv_model->input_layer.activation = ACTN_NONE;
  intra_uv_model->num_logits = UV_INTRA_MODES;
  intra_uv_model->loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  for (int sparse_idx = 0;
       sparse_idx < intra_uv_model->input_layer.num_sparse_inputs;
       sparse_idx++) {
    const int arr_size =
        intra_uv_model->input_layer.sparse_input_size[sparse_idx] *
        intra_uv_model->input_layer.num_outputs;
    av1_copy_array(intra_uv_model->input_layer.sparse_weights[sparse_idx],
                   intra_uv_mode_layer0_sparse_weights[sparse_idx], arr_size);
  }
  av1_copy(intra_uv_model->input_layer.dense_weights,
           intra_uv_mode_layer0_dense_weights);
  av1_copy(intra_uv_model->input_layer.bias, intra_uv_mode_layer0_bias);
#endif  // CONFIG_INTRA_ENTROPY
  av1_copy(fc->angle_delta_cdf, default_angle_delta_cdf);
  av1_copy(fc->comp_inter_cdf, default_comp_inter_cdf);
  av1_copy(fc->comp_ref_type_cdf, default_comp_ref_type_cdf);
  av1_copy(fc->uni_comp_ref_cdf, default_uni_comp_ref_cdf);
  av1_copy(fc->palette_y_mode_cdf, default_palette_y_mode_cdf);
  av1_copy(fc->palette_uv_mode_cdf, default_palette_uv_mode_cdf);
  av1_copy(fc->comp_ref_cdf, default_comp_ref_cdf);
  av1_copy(fc->comp_bwdref_cdf, default_comp_bwdref_cdf);
  av1_copy(fc->single_ref_cdf, default_single_ref_cdf);
  av1_copy(fc->txfm_partition_cdf, default_txfm_partition_cdf);
  av1_copy(fc->compound_index_cdf, default_compound_idx_cdfs);
  av1_copy(fc->comp_group_idx_cdf, default_comp_group_idx_cdfs);
  av1_copy(fc->newmv_cdf, default_newmv_cdf);
  av1_copy(fc->zeromv_cdf, default_zeromv_cdf);
  av1_copy(fc->refmv_cdf, default_refmv_cdf);
  av1_copy(fc->drl_cdf, default_drl_cdf);
  av1_copy(fc->motion_mode_cdf, default_motion_mode_cdf);
  av1_copy(fc->obmc_cdf, default_obmc_cdf);
  av1_copy(fc->inter_compound_mode_cdf, default_inter_compound_mode_cdf);
  av1_copy(fc->compound_type_cdf, default_compound_type_cdf);
  av1_copy(fc->wedge_idx_cdf, default_wedge_idx_cdf);
  av1_copy(fc->interintra_cdf, default_interintra_cdf);
  av1_copy(fc->wedge_interintra_cdf, default_wedge_interintra_cdf);
  av1_copy(fc->interintra_mode_cdf, default_interintra_mode_cdf);
  av1_copy(fc->seg.pred_cdf, default_segment_pred_cdf);
  av1_copy(fc->seg.tree_cdf, default_seg_tree_cdf);
  av1_copy(fc->filter_intra_cdfs, default_filter_intra_cdfs);
  av1_copy(fc->filter_intra_mode_cdf, default_filter_intra_mode_cdf);
#if CONFIG_ADAPT_FILTER_INTRA
  av1_copy(fc->adapt_filter_intra_cdfs, default_adapt_filter_intra_cdfs);
  av1_copy(fc->adapt_filter_intra_mode_cdf,
           default_adapt_filter_intra_mode_cdf);
#endif
  av1_copy(fc->switchable_restore_cdf, default_switchable_restore_cdf);
  av1_copy(fc->wiener_restore_cdf, default_wiener_restore_cdf);
  av1_copy(fc->sgrproj_restore_cdf, default_sgrproj_restore_cdf);
#if CONFIG_LOOP_RESTORE_CNN
  av1_copy(fc->cnn_restore_cdf, default_cnn_restore_cdf);
#endif  // CONFIG_LOOP_RESTORE_CNN
  av1_copy(fc->y_mode_cdf, default_if_y_mode_cdf);
  av1_copy(fc->uv_mode_cdf, default_uv_mode_cdf);
  av1_copy(fc->switchable_interp_cdf, default_switchable_interp_cdf);
  av1_copy(fc->partition_cdf, default_partition_cdf);
  av1_copy(fc->intra_ext_tx_cdf, default_intra_ext_tx_cdf);
  av1_copy(fc->inter_ext_tx_cdf, default_inter_ext_tx_cdf);
#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTER
  av1_copy(fc->mdtx_type_inter_cdf, default_mdtx_type_inter_cdf);
  av1_copy(fc->use_mdtx_inter_cdf, default_use_mdtx_inter_cdf);
#endif
#if USE_MDTX_INTRA
  av1_copy(fc->mdtx_type_intra_cdf, default_mdtx_type_intra_cdf);
  av1_copy(fc->use_mdtx_intra_cdf, default_use_mdtx_intra_cdf);
#endif
#endif
  av1_copy(fc->skip_mode_cdfs, default_skip_mode_cdfs);
  av1_copy(fc->skip_cdfs, default_skip_cdfs);
  av1_copy(fc->intra_inter_cdf, default_intra_inter_cdf);
  for (int i = 0; i < SPATIAL_PREDICTION_PROBS; i++)
    av1_copy(fc->seg.spatial_pred_seg_cdf[i],
             default_spatial_pred_seg_tree_cdf[i]);
  av1_copy(fc->tx_size_cdf, default_tx_size_cdf);
  av1_copy(fc->delta_q_cdf, default_delta_q_cdf);
  av1_copy(fc->delta_lf_cdf, default_delta_lf_cdf);
  av1_copy(fc->delta_lf_multi_cdf, default_delta_lf_multi_cdf);
  av1_copy(fc->cfl_sign_cdf, default_cfl_sign_cdf);
  av1_copy(fc->cfl_alpha_cdf, default_cfl_alpha_cdf);
  av1_copy(fc->intrabc_cdf, default_intrabc_cdf);
}

void av1_set_default_ref_deltas(int8_t *ref_deltas) {
  assert(ref_deltas != NULL);

  ref_deltas[INTRA_FRAME] = 1;
  ref_deltas[LAST_FRAME] = 0;
  ref_deltas[LAST2_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[LAST3_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[BWDREF_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[GOLDEN_FRAME] = -1;
  ref_deltas[ALTREF2_FRAME] = -1;
  ref_deltas[ALTREF_FRAME] = -1;
}

void av1_set_default_mode_deltas(int8_t *mode_deltas) {
  assert(mode_deltas != NULL);

  mode_deltas[0] = 0;
  mode_deltas[1] = 0;
}

static void set_default_lf_deltas(struct loopfilter *lf) {
  lf->mode_ref_delta_enabled = 1;
  lf->mode_ref_delta_update = 1;

  av1_set_default_ref_deltas(lf->ref_deltas);
  av1_set_default_mode_deltas(lf->mode_deltas);
}

void av1_setup_frame_contexts(AV1_COMMON *cm) {
  // Store the frame context into a special slot (not associated with any
  // reference buffer), so that we can set up cm->pre_fc correctly later
  // This function must ONLY be called when cm->fc has been initialized with
  // default probs, either by av1_setup_past_independence or after manually
  // initializing them
  *cm->default_frame_context = *cm->fc;
  // TODO(jack.haughton@argondesign.com): don't think this should be necessary,
  // but could do with fuller testing
  if (cm->large_scale_tile) {
    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
      RefCntBuffer *const buf = get_ref_frame_buf(cm, i);
      if (buf != NULL) buf->frame_context = *cm->fc;
    }
    for (int i = 0; i < FRAME_BUFFERS; ++i)
      cm->buffer_pool->frame_bufs[i].frame_context = *cm->fc;
  }
}

void av1_setup_past_independence(AV1_COMMON *cm) {
  // Reset the segment feature data to the default stats:
  // Features disabled, 0, with delta coding (Default state).
  av1_clearall_segfeatures(&cm->seg);

  if (cm->cur_frame->seg_map)
    memset(cm->cur_frame->seg_map, 0, (cm->mi_rows * cm->mi_cols));

  // reset mode ref deltas
  av1_set_default_ref_deltas(cm->cur_frame->ref_deltas);
  av1_set_default_mode_deltas(cm->cur_frame->mode_deltas);
  set_default_lf_deltas(&cm->lf);

  av1_default_coef_probs(cm);
  init_mode_probs(cm->fc);
  av1_init_mv_probs(cm);
  cm->fc->initialized = 1;
  av1_setup_frame_contexts(cm);

  // prev_mi will only be allocated in encoder.
  if (frame_is_intra_only(cm) && cm->prev_mi)
    memset(cm->prev_mi, 0, cm->mi_stride * cm->mi_rows * sizeof(*cm->prev_mi));
}
