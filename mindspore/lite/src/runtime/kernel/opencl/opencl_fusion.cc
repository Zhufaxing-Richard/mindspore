/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <vector>
#include <queue>
#include <set>
#include <ctime>
#include "src/runtime/kernel/opencl/opencl_subgraph.h"
#include "src/runtime/kernel/opencl/opencl_kernel.h"
#include "src/runtime/kernel/opencl/kernel/arithmetic.h"
#include "src/runtime/kernel/opencl/kernel/conv2d.h"
#include "src/runtime/kernel/opencl/kernel/fusion_eltwise.h"
#include "src/runtime/kernel/opencl/utils.h"
#include "src/runtime/opencl/opencl_executor.h"
#include "include/errorcode.h"
#include "schema/ops_generated.h"
#include "src/common/utils.h"
#include "nnacl/conv_parameter.h"
#include "nnacl/pad_parameter.h"
#include "nnacl/pooling_parameter.h"
#include "nnacl/fp32/activation_fp32.h"
#include "nnacl/matmul_parameter.h"
#include "nnacl/scale.h"

using mindspore::schema::ActivationType;
using mindspore::schema::ActivationType_LEAKY_RELU;
using mindspore::schema::ActivationType_NO_ACTIVATION;
using mindspore::schema::ActivationType_RELU;
using mindspore::schema::ActivationType_RELU6;
using mindspore::schema::ActivationType_TANH;
using mindspore::schema::PrimitiveType;
using mindspore::schema::PrimitiveType_Activation;
using mindspore::schema::PrimitiveType_Eltwise;
using mindspore::schema::PrimitiveType_NONE;

namespace mindspore::kernel {

namespace {

template <typename T0, typename T1>
inline bool AIsInB(const T0 *a, const T1 *b) {
  MS_ASSERT(a);
  MS_ASSERT(b);
  return std::find(b->begin(), b->end(), a) != b->end();
}

inline bool PredIs(const LiteKernel *node, PrimitiveType type, std::vector<LiteKernel *> *nodes) {
  MS_ASSERT(node);
  if (node->in_kernels().size() == 1) {
    LiteKernel *pred = node->in_kernels().front();
    MS_ASSERT(pred);
    if (AIsInB(pred, nodes) && pred->Type() == type && pred->out_kernels().size() == 1) {
      MS_ASSERT(pred->out_kernels().front() == node);
      return true;
    }
  }
  return false;
}

inline std::string GetTypeName(const LiteKernel *node) {
  MS_ASSERT(node);
  if (node->Type() == PrimitiveType_FusionEltwise) {
    return "FusionEltwise";
  } else {
    return schema::EnumNamePrimitiveType(node->Type());
  }
}

inline bool NC_N11C(const LiteKernel *node) {
  MS_ASSERT(node);
  if (node->in_tensors().empty() || node->out_tensors().empty()) {
    return false;
  } else {
    MS_ASSERT(node->in_tensors().front());
    MS_ASSERT(node->out_tensors().front());
    auto input_shape = node->in_tensors().front()->shape();
    auto output_shape = node->out_tensors().front()->shape();
    return input_shape.size() == 2 && output_shape.size() == 4 &&
           output_shape == std::vector<int>({input_shape[0], 1, 1, input_shape[1]});
  }
}

inline bool N11C_NC(const LiteKernel *node) {
  MS_ASSERT(node);
  if (node->in_tensors().empty() || node->out_tensors().empty()) {
    return false;
  } else {
    MS_ASSERT(node->in_tensors().front());
    MS_ASSERT(node->out_tensors().front());
    auto input_shape = node->in_tensors().front()->shape();
    auto output_shape = node->out_tensors().front()->shape();
    return input_shape.size() == 4 && output_shape.size() == 2 &&
           input_shape == std::vector<int>({output_shape[0], 1, 1, output_shape[1]});
  }
}

inline bool NC11_NC(const LiteKernel *node) {
  if (node->in_tensors().empty() || node->out_tensors().empty()) {
    return false;
  } else {
    MS_ASSERT(node->in_tensors().front());
    MS_ASSERT(node->out_tensors().front());
    auto input_shape = node->in_tensors().front()->shape();
    auto output_shape = node->out_tensors().front()->shape();
    return input_shape.size() == 4 && output_shape.size() == 2 &&
           input_shape == std::vector<int>({output_shape[0], output_shape[1], 1, 1});
  }
}

template <typename T>
std::vector<T *> RemoveDuplicationsButKeepOrder(const std::vector<T *> &vec) {
  std::vector<T *> ret;
  std::set<T *> s;
  for (auto *x : vec) {
    if (0 == s.count(x)) {
      ret.push_back(x);
      s.insert(x);
    }
  }
  return ret;
}

void Merge(LiteKernel *a, LiteKernel *b, bool remove_a) {
  MS_ASSERT(a);
  MS_ASSERT(b);
  if (remove_a) {  // pred->tensor0->a->tensor1->b: remove a tensor1
    // update pred out_kernels: a.in_kernels.out_kernels.replace(a,b)
    for (auto *pred : a->in_kernels()) {
      MS_ASSERT(pred);
      auto pred_out_kernels = pred->out_kernels();
      std::replace_if(
        pred_out_kernels.begin(), pred_out_kernels.end(), [&](LiteKernel *x) { return x == a; }, b);
      pred->set_out_kernels(RemoveDuplicationsButKeepOrder(pred_out_kernels));
    }

    // update b in_tensors: b.in_tensors.replace(a.out_tensors[0], a.in_tensors)
    auto b_in_tensors = b->in_tensors();
    for (int i = 0; i < b_in_tensors.size(); ++i) {
      if (b_in_tensors[i] == a->out_tensors().front()) {
        // reshape: 2nd input tensor is removed
        if (a->Type() == schema::PrimitiveType_Reshape) {
          b_in_tensors[i] = a->in_tensors().front();
          b->set_in_tensors(b_in_tensors);
        } else {
          b_in_tensors.erase(b_in_tensors.begin() + i);
          b_in_tensors.insert(b_in_tensors.begin() + i, a->in_tensors().begin(), a->in_tensors().end());
          b->set_in_tensors(RemoveDuplicationsButKeepOrder(b_in_tensors));
        }
        break;
      }
    }

    // update b in_kernels: b.in_kernels.replace(a, a.in_kernels)
    auto b_in_kernels = b->in_kernels();
    for (int i = 0; i < b_in_kernels.size(); ++i) {
      if (a == b_in_kernels[i]) {
        b_in_kernels.erase(b_in_kernels.begin() + i);
        b_in_kernels.insert(b_in_kernels.begin() + i, a->in_kernels().begin(), a->in_kernels().end());
        b->set_in_kernels(RemoveDuplicationsButKeepOrder(b_in_kernels));
        break;
      }
    }
  } else {  // a->tensor1->b->tensor2->succ: remove tensor1 b
    // update a.out_tensors
    a->set_out_tensors(b->out_tensors());

    // update a.out_kernels
    a->set_out_kernels(b->out_kernels());

    // update succ in_kernels
    for (auto *succ : b->out_kernels()) {
      MS_ASSERT(succ);
      auto succ_in_kernels = succ->in_kernels();
      std::replace_if(
        succ_in_kernels.begin(), succ_in_kernels.end(), [&](LiteKernel *x) { return x == b; }, a);
      succ->set_in_kernels(RemoveDuplicationsButKeepOrder(succ_in_kernels));
    }
  }
}

inline void MergeRemoveA(LiteKernel *a, LiteKernel *b, std::set<LiteKernel *> *removed_set,
                         bool do_check_specs = true) {
  MS_ASSERT(a);
  MS_ASSERT(b);
  MS_ASSERT(removed_set);
  Merge(a, b, true);
  removed_set->insert(a);
  if (do_check_specs && reinterpret_cast<OpenCLKernel *>(b)->CheckSpecs() != RET_OK) {
    MS_LOG(ERROR) << "fusion kernel CheckSpecs() error: kernel name is " << b->name();
  }
}

inline void MergeRemoveB(LiteKernel *a, LiteKernel *b, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(a);
  MS_ASSERT(b);
  MS_ASSERT(removed_set);
  Merge(a, b, false);
  removed_set->insert(b);
  if (reinterpret_cast<OpenCLKernel *>(a)->CheckSpecs() != RET_OK) {
    MS_LOG(ERROR) << "fusion kernel CheckSpecs() error: kernel name is " << a->name();
  }
}

// Pad + Conv2D
// Pad + DepthwiseConv2D
// Pad + DeConv2D
// Pad + Pooling
template <typename ParamType>
void TryMergePad(LiteKernel *node, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(node);
  MS_ASSERT(removed_set);
  LiteKernel *pad = node->in_kernels().front();
  MS_ASSERT(pad);
  if (pad->in_tensors().front()->shape().size() != 4) {
    return;
  }
  auto *pad_param = reinterpret_cast<PadParameter *>(reinterpret_cast<OpenCLKernel *>(pad)->GetParameter());
  MS_ASSERT(pad_param);
  if (pad_param->pad_mode_ != schema::PaddingMode::PaddingMode_CONSTANT ||
      std::fabs(pad_param->constant_value_) > 1e-5) {
    return;
  }

  auto *conv_param = reinterpret_cast<ParamType *>(reinterpret_cast<OpenCLKernel *>(node)->GetParameter());
  MS_ASSERT(conv_param);
  conv_param->pad_u_ += pad_param->paddings_[2];
  conv_param->pad_d_ += pad_param->paddings_[3];
  conv_param->pad_l_ += pad_param->paddings_[4];
  conv_param->pad_r_ += pad_param->paddings_[5];

  MergeRemoveA(pad, node, removed_set);
  MS_LOG(DEBUG) << "Merge Pad and " + GetTypeName(node) + " success";
}

// Conv2D + Reshape(N11C->NC)
void TryMergeConvReshape(LiteKernel *reshape, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(reshape);
  MS_ASSERT(removed_set);
  if (N11C_NC(reshape)) {
    LiteKernel *conv = reshape->in_kernels().front();
    MS_ASSERT(conv);
    MergeRemoveB(conv, reshape, removed_set);
    MS_LOG(DEBUG) << "Merge Conv2D and Reshape(N11C->NC) success";
  }
}

// FullConnection + Reshape(NC->N11C or N11C->NC)
void TryMergeFcReshape(LiteKernel *reshape, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(reshape);
  MS_ASSERT(removed_set);
  bool NC_N11C_flag = NC_N11C(reshape);
  if (NC_N11C_flag || N11C_NC(reshape)) {
    LiteKernel *fc = reshape->in_kernels().front();
    MS_ASSERT(fc);
    MergeRemoveB(fc, reshape, removed_set);
    MS_LOG(DEBUG) << "Merge FullConnection and Reshape" + (NC_N11C_flag ? std::string("(NC->N11C)") : "(N11C->NC)") +
                       " success";
  }
}

// Reshape(NC11->NC) + FullConnection
// Reshape(NC->N11C) + FullConnection
void TryMergeReshapeFc(LiteKernel *fc, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(fc);
  MS_ASSERT(removed_set);
  LiteKernel *reshape = fc->in_kernels().front();
  MS_ASSERT(reshape);
  bool NC11_NC_flag = NC11_NC(reshape);
  if (NC11_NC_flag || NC_N11C(reshape)) {
    MergeRemoveA(reshape, fc, removed_set);
    MS_LOG(DEBUG) << "Merge Reshape" + (NC11_NC_flag ? std::string("(NC11->NC)") : "(NC->N11C)") +
                       " and FullConnection success";
  }
}

// Arithmetic(NO_ACTIVATION) + Activation(RELU/RELU6)
void TryMergeArithmeticAct(LiteKernel *act, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(act);
  MS_ASSERT(removed_set);
  LiteKernel *arithmetic = act->in_kernels().front();
  MS_ASSERT(arithmetic);
  auto *arithmetic_param =
    reinterpret_cast<ArithmeticParameter *>(reinterpret_cast<OpenCLKernel *>(arithmetic)->GetParameter());
  auto *act_param = reinterpret_cast<ActivationParameter *>(reinterpret_cast<OpenCLKernel *>(act)->GetParameter());
  MS_ASSERT(arithmetic_param);
  MS_ASSERT(act_param);

  if (arithmetic_param->activation_type_ == ActivationType_NO_ACTIVATION &&
      (act_param->type_ == ActivationType_RELU || act_param->type_ == ActivationType_RELU6)) {
    arithmetic_param->activation_type_ = act_param->type_;
    MergeRemoveB(arithmetic, act, removed_set);
    MS_LOG(DEBUG) << "Merge " + GetTypeName(arithmetic) + "(NO_ACTIVATION) and Activation(RELU or RELU6) success";
  }
}

// Conv2D(NO_ACTIVATION)         + Activation(RELU/RELU6/TANH)
// FullConnection(NO_ACTIVATION) + Activation(RELU/RELU6/TANH)
template <typename ParamType>
void TryMergeActivation(LiteKernel *act, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(node);
  MS_ASSERT(removed_set);
  auto *act_param = reinterpret_cast<ActivationParameter *>(reinterpret_cast<OpenCLKernel *>(act)->GetParameter());
  LiteKernel *node = act->in_kernels().front();
  auto *param = reinterpret_cast<ParamType *>(reinterpret_cast<OpenCLKernel *>(node)->GetParameter());
  MS_ASSERT(param);
  if (param->act_type_ == ActType_No) {
    param->act_type_ = static_cast<ActType>(act_param->type_);
    std::string act_name;
    if (act_param->type_ == ActivationType_RELU) {
      act_name = "RELU";
    } else if (act_param->type_ == ActivationType_RELU6) {
      act_name = "RELU6";
    } else if (act_param->type_ == ActivationType_TANH) {
      act_name = "TANH";
    }
    MergeRemoveB(node, act, removed_set);
    MS_LOG(DEBUG) << "Merge " + GetTypeName(node) + "(NO_ACTIVATION) and Activation(" + act_name + ") success";
  }
}

// Conv2D(NO_ACTIVATION) + PReLU(weight is scalar)
void TryMergeConvPReLU(LiteKernel *prelu, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(prelu);
  MS_ASSERT(removed_set);
  if (prelu->in_tensors().size() != 2) {
    return;
  }
  auto *prelu_weight = prelu->in_tensors().at(1);
  bool shape_is_valid =
    prelu_weight->IsScalar() || (prelu_weight->shape().size() == 1 && prelu_weight->shape().front() == 1);
  if (!shape_is_valid) {
    return;
  }
  if (prelu_weight->data_type() != kNumberTypeFloat32) {
    return;
  }

  LiteKernel *conv = prelu->in_kernels().front();
  auto *param = reinterpret_cast<ConvParameter *>(reinterpret_cast<OpenCLKernel *>(conv)->GetParameter());
  MS_ASSERT(param);
  if (param->act_type_ == ActType_No) {
    param->act_type_ = static_cast<ActType>(ActivationType_LEAKY_RELU);
    reinterpret_cast<Conv2DOpenCLKernel *>(conv)->alpha_ = *reinterpret_cast<float *>(prelu_weight->data_c());
    MergeRemoveB(conv, prelu, removed_set);
    MS_LOG(DEBUG) << "Merge Conv2D(NO_ACTIVATION) and PReLU(weight is scalar) success";
  }
}

int TryFusionConvScaleWeight(LiteKernel *conv_kernel, LiteKernel *scale_kernel) {
  MS_ASSERT(conv_kernel);
  MS_ASSERT(scale_kernel);
  auto *scale_param =
    reinterpret_cast<ScaleParameter *>(reinterpret_cast<OpenCLKernel *>(scale_kernel)->GetParameter());
  MS_ASSERT(scale_param);
  MS_ASSERT(conv_kernel->in_tensors().size() >= 2);
  auto *filter = conv_kernel->in_tensors().at(1);
  auto *bias = conv_kernel->in_tensors().size() == 3 ? conv_kernel->in_tensors().at(2) : nullptr;
  auto *scale = scale_kernel->in_tensors().at(1);
  auto *offset = scale_kernel->in_tensors().at(2);
  MS_ASSERT(filter);
  MS_ASSERT(bias);
  MS_ASSERT(scale);
  MS_ASSERT(offset);

  if (scale_kernel->in_tensors().size() != 3) {
    return RET_ERROR;
  }
  if (scale->shape().size() != 1 || scale->shape().at(0) != filter->shape().back() ||
      scale->shape() != offset->shape()) {
    return RET_ERROR;
  }
  if (!(scale_param->axis_ == -1 || scale_param->axis_ == 3)) {
    return RET_ERROR;
  }
  if (filter->data_type() != kNumberTypeFloat32 || (bias && bias->data_type() != kNumberTypeFloat32) ||
      scale->data_type() != kNumberTypeFloat32 || offset->data_type() != kNumberTypeFloat32) {
    return RET_ERROR;
  }

  // update filter: filter*=scale
  MS_ASSERT(filter->shape().size() == 4);
  int CI = filter->shape()[0];
  int KH = filter->shape()[1];
  int KW = filter->shape()[2];
  int CO = filter->shape()[3];
  auto *filter_data = reinterpret_cast<float *>(filter->data_c());
  auto *scale_data = reinterpret_cast<float *>(scale->data_c());
  for (int i = 0; i < CI * KH * KW * CO; ++i) {
    filter_data[i] *= scale_data[i % CO];
  }

  // update bias: bias=bias*scale+offset
  if (bias != nullptr) {
    auto *bias_data = reinterpret_cast<float *>(bias->data_c());
    auto *offset_data = reinterpret_cast<float *>(offset->data_c());
    for (int co = 0; co < CO; ++co) {
      bias_data[co] *= scale_data[co];
      bias_data[co] += offset_data[co];
    }
  } else {  // if deconv dont't have bias, let scale's offset be deconv's bias
    auto tmp = conv_kernel->in_tensors();
    tmp.push_back(offset);
    conv_kernel->set_in_tensors(tmp);
  }
  return RET_OK;
}

// DeConv2D + Scale (can't both has activation)
void TryMergeDeconvScale(LiteKernel *scale, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(scale);
  MS_ASSERT(removed_set);
  LiteKernel *deconv = scale->in_kernels().front();
  MS_ASSERT(deconv);

  // check act_type_
  auto *deconv_param = reinterpret_cast<ConvParameter *>(reinterpret_cast<OpenCLKernel *>(deconv)->GetParameter());
  auto *scale_param = reinterpret_cast<ScaleParameter *>(reinterpret_cast<OpenCLKernel *>(scale)->GetParameter());
  MS_ASSERT(deconv_param);
  MS_ASSERT(scale_param);
  if (deconv_param->act_type_ == ActType_No) {
    if (!(scale_param->activation_type_ == ActivationType_NO_ACTIVATION ||
          scale_param->activation_type_ == ActivationType_RELU ||
          scale_param->activation_type_ == ActivationType_RELU6)) {
      return;
    }
  } else if (deconv_param->act_type_ == ActType_Relu || deconv_param->act_type_ == ActType_Relu6) {
    if (deconv_param->act_type_ != ActType_No) {
      return;
    }
  } else {
    return;
  }

  // fusion weight
  if (TryFusionConvScaleWeight(deconv, scale) == RET_ERROR) {
    return;
  }

  // update act_type_
  if (deconv_param->act_type_ == ActType_No) {
    deconv_param->act_type_ = static_cast<ActType>(scale_param->activation_type_);
  }

  MergeRemoveB(deconv, scale, removed_set);
  MS_LOG(DEBUG) << "Merge DeConv2D and Scale success";
}

void CreateEltwiseKernelReplaceOld(FusionEltwiseParameter *param, LiteKernel *old, std::vector<LiteKernel *> *nodes,
                                   std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(param);
  MS_ASSERT(old);
  MS_ASSERT(nodes);
  MS_ASSERT(removed_set);
  auto *eltwise = new (std::nothrow) FusionEltwiseOpenCLKernel(reinterpret_cast<OpParameter *>(param),
                                                               old->in_tensors(), old->out_tensors(), nullptr, nullptr);
  if (eltwise == nullptr) {
    MS_LOG(ERROR) << "create FusionEltwiseOpenCLKernel error.";
    return;
  }
  eltwise->set_name("FusionEltwise: " + param->name_);
  eltwise->set_in_kernels(old->in_kernels());
  eltwise->set_out_kernels(old->out_kernels());

  for (auto *pred : old->in_kernels()) {
    MS_ASSERT(pred);
    auto tmp = pred->out_kernels();
    std::replace_if(
      tmp.begin(), tmp.end(), [&](LiteKernel *x) { return x == old; }, eltwise);
    pred->set_out_kernels(tmp);
  }

  for (auto *succ : old->out_kernels()) {
    MS_ASSERT(succ);
    auto tmp = succ->in_kernels();
    std::replace_if(
      tmp.begin(), tmp.end(), [&](LiteKernel *x) { return x == old; }, eltwise);
    succ->set_in_kernels(tmp);
  }
  std::replace(nodes->begin(), nodes->end(), old, reinterpret_cast<LiteKernel *>(eltwise));
  removed_set->insert(old);
}

// Eltwise + Eltwise
int TryMergeEltwiseEltwise(LiteKernel *node, std::vector<LiteKernel *> *nodes, std::set<LiteKernel *> *removed_set) {
  MS_ASSERT(node);
  MS_ASSERT(nodes);
  MS_ASSERT(removed_set);
  // node must be eltwise-like op
  if (!IsEltwiseAndOperatorSupported(node)) {
    return RET_ERROR;
  }

  // preds must contain eltwise-like op
  const std::vector<LiteKernel *> preds = node->in_kernels();
  std::set<LiteKernel *> pred_eltwises;
  std::map<lite::Tensor *, FusionEltwiseParameter *> pred_params;
  for (LiteKernel *pred : preds) {
    MS_ASSERT(pred);
    if (AIsInB(pred, nodes) && IsEltwiseAndOperatorSupported(pred) && pred->out_kernels().size() == 1) {
      auto *tensor = pred->out_tensors().front();
      MS_ASSERT(pred->out_kernels().front() == node);
      MS_ASSERT(AIsInB(tensor, node.in_tensors()));
      pred_eltwises.insert(pred);
      // create FusionEltwiseParameter for this pred eltwise
      auto param = CreateFusionEltwiseParameter(pred);
      pred_params.emplace(tensor, param);
    }
  }
  if (pred_eltwises.empty()) {
    return RET_ERROR;
  }

  // 1. create FusionEltwiseParameter for this node
  FusionEltwiseParameter *param = CreateFusionEltwiseParameter(node, pred_params);
  MS_ASSERT(param);
  // 2. merge pred eltwise op
  for (LiteKernel *pred_eltwise : pred_eltwises) {
    MergeRemoveA(pred_eltwise, node, removed_set, false);
  }
  // 3. create FusionFusionEltwiseOpenCLKernel and replace old kernel by new
  CreateEltwiseKernelReplaceOld(param, node, nodes, removed_set);

  MS_LOG(DEBUG) << "Merge Eltwise and Eltwise success: " << param->name_;
  return RET_OK;
}

}  // namespace

int OpenCLSubGraph::FusionPass() {
  if (!this->IsSubGraphInferShapeDone()) {
    return RET_OK;
  }
  MS_LOG(DEBUG) << "start Fusion";

  std::vector<LiteKernel *> input_nodes;
  for (auto *node : nodes_) {
    if (std::any_of(node->in_tensors().begin(), node->in_tensors().end(),
                    [&](lite::Tensor *tensor) { return AIsInB(tensor, &in_tensors_); })) {
      input_nodes.push_back(node);
    }
  }

  auto cmp = [&](LiteKernel *a, LiteKernel *b) {
    return std::find(nodes_.begin(), nodes_.end(), a) > std::find(nodes_.begin(), nodes_.end(), b);
  };
  std::priority_queue<LiteKernel *, std::vector<LiteKernel *>, decltype(cmp)> q(cmp, input_nodes);
  std::set<LiteKernel *> qset(input_nodes.begin(), input_nodes.end());
  std::set<LiteKernel *> removed_set;
  while (!q.empty()) {
    LiteKernel *node = q.top();
    MS_ASSERT(node);
    q.pop();
    qset.erase(node);
    if (AIsInB(node, &removed_set)) {
      continue;
    }

    // insert node->out_kernels to q only if succ
    // 1. not in q
    // 2. not be removed
    // 3. in nodes_
    for (auto *succ : node->out_kernels()) {
      if (!AIsInB(succ, &qset) && !AIsInB(succ, &removed_set) && AIsInB(succ, &nodes_)) {
        q.push(succ);
        qset.insert(succ);
      }
    }

    // do element-wise fusion, like mul+add, mul+add+relu
    if (TryMergeEltwiseEltwise(node, &nodes_, &removed_set) == RET_OK) {
      continue;
    }

    // do special fusion, like pad+conv2d, fc+reshape
    switch (node->Type()) {
      case schema::PrimitiveType_Conv2D:
      case schema::PrimitiveType_DepthwiseConv2D:
      case schema::PrimitiveType_DeConv2D: {
        if (PredIs(node, schema::PrimitiveType_Pad, &nodes_)) {
          TryMergePad<ConvParameter>(node, &removed_set);
        }
        break;
      }
      case schema::PrimitiveType_Pooling: {
        if (PredIs(node, schema::PrimitiveType_Pad, &nodes_)) {
          TryMergePad<PoolingParameter>(node, &removed_set);
        }
        break;
      }
      case schema::PrimitiveType_Reshape: {
        if (PredIs(node, schema::PrimitiveType_FullConnection, &nodes_)) {
          TryMergeFcReshape(node, &removed_set);
        } else if (PredIs(node, schema::PrimitiveType_Conv2D, &nodes_)) {
          TryMergeConvReshape(node, &removed_set);
        }
        break;
      }
      case schema::PrimitiveType_FullConnection: {
        if (PredIs(node, schema::PrimitiveType_Reshape, &nodes_)) {
          TryMergeReshapeFc(node, &removed_set);
        }
        break;
      }
      case schema::PrimitiveType_Activation: {
        // try merge Conv2D/FC(without act) + RELU/RELU6/TANH
        auto *param = reinterpret_cast<ActivationParameter *>(reinterpret_cast<OpenCLKernel *>(node)->GetParameter());
        MS_ASSERT(param);
        if (param->type_ == ActivationType_RELU || param->type_ == ActivationType_RELU6 ||
            param->type_ == ActivationType_TANH) {
          if (PredIs(node, schema::PrimitiveType_Conv2D, &nodes_)) {
            TryMergeActivation<ConvParameter>(node, &removed_set);
            break;
          } else if (PredIs(node, schema::PrimitiveType_FullConnection, &nodes_)) {
            TryMergeActivation<MatMulParameter>(node, &removed_set);
            break;
          }
        }
        if (std::any_of(ArithmeticPrimitives.begin(), ArithmeticPrimitives.end(),
                        [&](schema::PrimitiveType type) { return PredIs(node, type, &nodes_); })) {
          TryMergeArithmeticAct(node, &removed_set);
        }
        break;
      }
      case schema::PrimitiveType_PReLU: {
        if (PredIs(node, schema::PrimitiveType_Conv2D, &nodes_)) {
          TryMergeConvPReLU(node, &removed_set);
          break;
        }
        break;
      }
      case schema::PrimitiveType_Scale: {
        if (PredIs(node, schema::PrimitiveType_DeConv2D, &nodes_)) {
          TryMergeDeconvScale(node, &removed_set);
          break;
        }
        break;
      }
      default:
        break;
    }
  }

  for (auto kernel : removed_set) {
    delete kernel;
  }
  MS_LOG(DEBUG) << "number of kernels(before fusion): " << nodes_.size();
  nodes_.erase(
    std::remove_if(nodes_.begin(), nodes_.end(), [&](LiteKernel *node) { return AIsInB(node, &removed_set); }),
    nodes_.end());
  MS_LOG(DEBUG) << "number of kernels(after fusion) : " << nodes_.size();
  return RET_OK;
}

}  // namespace mindspore::kernel
