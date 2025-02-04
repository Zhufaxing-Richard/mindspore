/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/quant/fake_quant_perchannel_grad_gpu_kernel.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/fake_quant_perchannel_impl.cuh"

namespace mindspore {
namespace kernel {
FakeQuantPerChannelGradGpuKernelMod::FakeQuantPerChannelGradGpuKernelMod()
    : input_size_(0),
      num_bits_(0),
      quant_min_(0),
      quant_max_(0),
      num_channels_(0),
      quant_delay_(0),
      global_step_(0),
      narrow_range_(false),
      is_null_input_(false),
      symmetric_(false) {}

bool FakeQuantPerChannelGradGpuKernelMod::Init(const CNodePtr &kernel_node) {
  auto kernel_name = common::AnfAlgo::GetCNodeName(kernel_node);
  kernel_node_ = kernel_node;
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  if (input_num != 4) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name << "', the number of inputs should be 4, but got " << input_num;
  }

  size_t output_num = AnfAlgo::GetOutputTensorNum(kernel_node);
  if (output_num != 1) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name << "', the number of outputs should be 1, but got " << output_num;
  }

  auto prim = common::AnfAlgo::GetCNodePrimitive(kernel_node);
  MS_EXCEPTION_IF_NULL(prim);
  num_bits_ = static_cast<unsigned int>(GetValue<int64_t>(prim->GetAttr("num_bits")));
  if (num_bits_ <= 2 || num_bits_ >= 16) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name << "', the value of num_bits should be in (2, 16), but got "
                      << num_bits_;
  }

  quant_delay_ = static_cast<int>(GetValue<int64_t>(prim->GetAttr("quant_delay")));
  if (quant_delay_ < 0) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name << "', the value of quant_delay_ cannot be less than 0, but got "
                      << quant_delay_;
  }

  symmetric_ = GetValue<bool>(prim->GetAttr("symmetric"));
  narrow_range_ = GetValue<bool>(prim->GetAttr("narrow_range"));

  // quant min and max value
  quant_min_ = 0;
  quant_max_ = (1 << num_bits_) - 1;
  if (narrow_range_) {
    quant_min_++;
  }

  auto input_shape = common::AnfAlgo::GetPrevNodeOutputInferShape(kernel_node, 0);
  is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name, "input");
  if (is_null_input_) {
    InitSizeLists();
    return true;
  }
  if (input_shape.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name << "', input cannot be empty, but got empty";
  }
  num_channels_ = LongToInt(input_shape[0]);
  input_size_ = sizeof(float) * SizeOf(input_shape);
  InitSizeLists();
  return true;
}

void FakeQuantPerChannelGradGpuKernelMod::InitSizeLists() {
  input_size_list_.push_back(input_size_);                        // gradient
  input_size_list_.push_back(input_size_);                        // input
  input_size_list_.push_back(sizeof(float) * num_channels_);      // min
  input_size_list_.push_back(sizeof(float) * num_channels_);      // max
  output_size_list_.push_back(input_size_);                       // output
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // scale in channel
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // min in channel
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // max in channel
}

bool FakeQuantPerChannelGradGpuKernelMod::Launch(const std::vector<AddressPtr> &inputs,
                                                 const std::vector<AddressPtr> &workspace,
                                                 const std::vector<AddressPtr> &outputs, void *stream_ptr) {
  if (is_null_input_) {
    return true;
  }
  (void)workspace;
  float *output = GetDeviceAddress<float>(outputs, 0);
  float *gradient = GetDeviceAddress<float>(inputs, 0);
  float *input = GetDeviceAddress<float>(inputs, 1);
  float *input_min = GetDeviceAddress<float>(inputs, 2);
  float *input_max = GetDeviceAddress<float>(inputs, 3);
  float *scale = GetDeviceAddress<float>(workspace, 0);
  float *nudge_min = GetDeviceAddress<float>(workspace, 1);
  float *nudge_max = GetDeviceAddress<float>(workspace, 2);

  int total_size = input_size_ / sizeof(float);
  if (global_step_ >= quant_delay_) {
    CalNudgePerChannel(input_min, input_max, quant_min_, quant_max_, nudge_min, nudge_max, scale, num_channels_,
                       symmetric_, reinterpret_cast<cudaStream_t>(stream_ptr));
    CalFakeQuantPerChannelGrad(input, gradient, output, total_size, num_channels_, nudge_min, nudge_max,
                               reinterpret_cast<cudaStream_t>(stream_ptr));
  } else {
    CHECK_CUDA_RET_WITH_ERROR(kernel_node_,
                              cudaMemcpyAsync(output, gradient, input_size_, cudaMemcpyDeviceToDevice,
                                              reinterpret_cast<cudaStream_t>(stream_ptr)),
                              "Copy gpu memory failed.");
  }
  global_step_++;
  return true;
}

MS_REG_GPU_KERNEL(FakeQuantPerChannelGrad, FakeQuantPerChannelGradGpuKernelMod)
}  // namespace kernel
}  // namespace mindspore
