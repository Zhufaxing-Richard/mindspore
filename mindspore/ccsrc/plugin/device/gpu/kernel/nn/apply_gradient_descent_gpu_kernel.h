/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_APPLY_GRADIENT_DESCENT_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_APPLY_GRADIENT_DESCENT_GPU_KERNEL_H_

#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <map>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/apply_gradient_descent_impl.cuh"

namespace mindspore {
namespace kernel {
constexpr size_t INPUT_NUM = 3;
class ApplyGradientDescentKernelMod : public NativeGpuKernelMod {
 public:
  ApplyGradientDescentKernelMod() = default;
  ~ApplyGradientDescentKernelMod() override = default;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    VARIABLE_NOT_USED(workspace);
    launch_func_(this, inputs, outputs, stream_ptr);
    return true;
  }

  bool Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
            const std::vector<KernelTensorPtr> &outputs) override {
    MS_EXCEPTION_IF_NULL(base_operator);
    kernel_name_ = base_operator->name();
    if (inputs.size() != INPUT_NUM) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs must be 3, but got " << inputs.size();
    }
    if (outputs.size() != 1) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs must be 1, but got " << outputs.size();
    }

    auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
    auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
    if (!is_match) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does not support this kernel type: " << kernel_attr;
    }
    launch_func_ = func_list_[index].second;
    return true;
  }

  int Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
             const std::vector<KernelTensorPtr> &outputs, const std::map<uint32_t, tensor::TensorPtr> &) override {
    if (int ret = KernelMod::Resize(base_operator, inputs, outputs); ret != KRET_OK) {
      return ret;
    }
    auto input_shape = inputs[kIndex0]->GetShapeVector();
    is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name_, "var");
    if (is_null_input_ || IsDynamic(input_shape)) {
      return true;
    }
    input_size_ = SizeOf(input_shape);
    return static_cast<int>(KRET_OK);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  size_t input_size_{1};
  bool is_null_input_ = {false};
  std::string kernel_name_;

  template <typename T>
  void LaunchKernel(const std::vector<kernel::AddressPtr> &inputs, const std::vector<kernel::AddressPtr> &outputs,
                    void *stream_ptr);
  using LaunchFunc = std::function<void(ApplyGradientDescentKernelMod *, const std::vector<kernel::AddressPtr> &,
                                        const std::vector<kernel::AddressPtr> &, void *stream_ptr)>;
  LaunchFunc launch_func_;

  static std::vector<std::pair<KernelAttr, LaunchFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_APPLY_GRADIENT_DESCENT_GPU_KERNEL_H_
