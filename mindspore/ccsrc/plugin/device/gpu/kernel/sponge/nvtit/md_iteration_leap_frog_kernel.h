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
/**
 * Note:
 *  MDIterationLeapFrog. This is an experimental interface that is subject to change and/or deletion.
 */

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MD_ITERATION_LEAP_FROG_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MD_ITERATION_LEAP_FROG_KERNEL_H_

#include <cuda_runtime_api.h>
#include <map>
#include <string>
#include <vector>

#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_common.h"
#include "plugin/device/gpu/kernel/cuda_impl/sponge/nvtit/md_iteration_leap_frog_impl.cuh"

namespace mindspore {
namespace kernel {
template <typename T>
class MDIterationLeapFrogGpuKernelMod : public NativeGpuKernelMod {
 public:
  MDIterationLeapFrogGpuKernelMod() {}
  ~MDIterationLeapFrogGpuKernelMod() override = default;

  bool Init(const CNodePtr &kernel_node) override {
    kernel_node_ = kernel_node;
    atom_numbers = static_cast<int>(GetAttr<int64_t>(kernel_node, "atom_numbers"));
    dt = static_cast<float>(GetAttr<float>(kernel_node, "dt"));
    InitSizeLists();
    return true;
  }

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    auto vel = GetDeviceAddress<float>(inputs, 0);
    auto crd = GetDeviceAddress<float>(inputs, 1);
    auto frc = GetDeviceAddress<float>(inputs, 2);
    auto acc = GetDeviceAddress<float>(inputs, 3);
    auto inverse_mass = GetDeviceAddress<float>(inputs, 4);

    MDIterationLeapFrog(atom_numbers, vel, crd, frc, acc, inverse_mass, dt, reinterpret_cast<cudaStream_t>(stream_ptr));
    return true;
  }

 protected:
  void InitSizeLists() override {
    input_size_list_.push_back(atom_numbers * 3 * sizeof(T));
    input_size_list_.push_back(atom_numbers * 3 * sizeof(T));
    input_size_list_.push_back(atom_numbers * 3 * sizeof(T));
    input_size_list_.push_back(atom_numbers * 3 * sizeof(T));
    input_size_list_.push_back(atom_numbers * sizeof(T));

    output_size_list_.push_back(sizeof(T));
  }

 private:
  int atom_numbers;
  float dt;
};
}  // namespace kernel
}  // namespace mindspore
#endif
