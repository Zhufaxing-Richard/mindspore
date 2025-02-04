/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "momentum_impl.cuh"
#include "include/cuda_fp16.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"

template <typename T, typename S, typename G>
__global__ void MomentumUpdateVariableKernel(const size_t size, T *variable, T *accumulation, const S *learning_rate,
                                             const G *gradient, const S *momentum, bool use_nesterov) {
  if (use_nesterov) {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = momentum[0] * accumulation[i] + gradient[i];
      variable[i] -= gradient[i] * learning_rate[0] + accumulation[i] * momentum[0] * learning_rate[0];
    }
  } else {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = momentum[0] * accumulation[i] + gradient[i];
      variable[i] -= learning_rate[0] * accumulation[i];
    }
  }
}
template <>
__global__ void MomentumUpdateVariableKernel(const size_t size, half *variable, half *accumulation,
                                             const float *learning_rate, const half *gradient, const float *momentum,
                                             bool use_nesterov) {
  if (use_nesterov) {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = __float2half(momentum[0]) * accumulation[i] + gradient[i];
      variable[i] -= gradient[i] * __float2half(learning_rate[0]) +
                     accumulation[i] * __float2half(momentum[0]) * __float2half(learning_rate[0]);
    }
  } else {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = __float2half(momentum[0]) * accumulation[i] + gradient[i];
      variable[i] -= __float2half(learning_rate[0]) * accumulation[i];
    }
  }
}
template <>
__global__ void MomentumUpdateVariableKernel(const size_t size, float *variable, float *accumulation,
                                             const float *learning_rate, const half *gradient, const float *momentum,
                                             bool use_nesterov) {
  if (use_nesterov) {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = momentum[0] * accumulation[i] + __half2float(gradient[i]);
      variable[i] -= __half2float(gradient[i]) * learning_rate[0] + accumulation[i] * momentum[0] * learning_rate[0];
    }
  } else {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (size); i += blockDim.x * gridDim.x) {
      accumulation[i] = momentum[0] * accumulation[i] + __half2float(gradient[i]);
      variable[i] -= learning_rate[0] * accumulation[i];
    }
  }
}
template <typename T, typename S, typename G>
void MomentumUpdateVariable(const size_t size, T *variable, T *accumulation, const S *learning_rate, const G *gradient,
                            const S *momentum, bool use_nesterov, cudaStream_t cuda_stream) {
  MomentumUpdateVariableKernel<<<GET_BLOCKS(size), GET_THREADS, 0, cuda_stream>>>(
    size, variable, accumulation, learning_rate, gradient, momentum, use_nesterov);
}

template <typename T, typename S>
__global__ void FusedMomentumWeightDecayScaleKernel(const size_t element_num, T *weight_decay, T *scale, T *variable,
                                                    T *accumulation, const T *learning_rate, const S *gradient,
                                                    const T *momentum) {
  for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (element_num); i += blockDim.x * gridDim.x) {
    T grad = (variable[i] * weight_decay[0] + static_cast<T>(gradient[i])) * scale[0];
    accumulation[i] = momentum[0] * accumulation[i] + grad;
    variable[i] -= learning_rate[0] * accumulation[i];
  }
}

template <typename T, typename S>
void FusedWeightDecayScaleMomentum(const size_t element_num, T *weight_decay, T *scale, T *variable, T *accumulation,
                                   const T *learning_rate, const S *gradient, const T *momentum,
                                   cudaStream_t cuda_stream) {
  size_t thread_per_block = 256;
  size_t block_per_grid = (element_num + thread_per_block - 1) / thread_per_block;
  FusedMomentumWeightDecayScaleKernel<<<block_per_grid, thread_per_block, 0, cuda_stream>>>(
    element_num, weight_decay, scale, variable, accumulation, learning_rate, gradient, momentum);
}

template <typename T, typename S>
__global__ void FusedMomentumScaleKernel(const size_t element_num, T *scale, T *variable, T *accumulation,
                                         const T *learning_rate, const S *gradient, const T *momentum) {
  for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (element_num); i += blockDim.x * gridDim.x) {
    accumulation[i] = momentum[0] * accumulation[i] + static_cast<T>(gradient[i]) * scale[0];
    variable[i] -= learning_rate[0] * accumulation[i];
  }
}

template <typename T, typename S>
void FusedScaleMomentum(const size_t element_num, T *scale, T *variable, T *accumulation, const T *learning_rate,
                        const S *gradient, const T *momentum, cudaStream_t cuda_stream) {
  size_t thread_per_block = 256;
  size_t block_per_grid = (element_num + thread_per_block - 1) / thread_per_block;
  FusedMomentumScaleKernel<<<block_per_grid, thread_per_block, 0, cuda_stream>>>(
    element_num, scale, variable, accumulation, learning_rate, gradient, momentum);
}

template <typename T, typename S>
__global__ void FusedWeightDecayMomentumKernel(const size_t element_num, T *weight_decay, T *variable, T *accumulation,
                                               const T *learning_rate, const S *gradient, const T *momentum) {
  for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (element_num); i += blockDim.x * gridDim.x) {
    T grad = variable[i] * weight_decay[0] + static_cast<T>(gradient[i]);
    accumulation[i] = momentum[0] * accumulation[i] + grad;
    variable[i] -= learning_rate[0] * accumulation[i];
  }
}

template <typename T, typename S>
void FusedWeightDecayMomentum(const size_t element_num, T *weight_decay, T *variable, T *accumulation,
                              const T *learning_rate, const S *gradient, const T *momentum, cudaStream_t cuda_stream) {
  size_t thread_per_block = 256;
  size_t block_per_grid = (element_num + thread_per_block - 1) / thread_per_block;
  FusedWeightDecayMomentumKernel<<<block_per_grid, thread_per_block, 0, cuda_stream>>>(
    element_num, weight_decay, variable, accumulation, learning_rate, gradient, momentum);
}

// CombineFusedScaleMomentum
template <typename T, typename S>
__global__ void CombineFusedMomentumScaleKernel(const size_t num, const size_t *element_num, T **scale, T **variable,
                                                T **accumulation, T **learning_rate, S **gradient, T **momentum) {
  for (size_t idx = 0; idx < num; idx++) {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (element_num[idx]); i += blockDim.x * gridDim.x) {
      accumulation[idx][i] = momentum[idx][0] * accumulation[idx][i] + static_cast<T>(gradient[idx][i]) * scale[idx][0];
      variable[idx][i] -= learning_rate[idx][0] * accumulation[idx][i];
    }
  }
}

template <typename T, typename S>
void CombineFusedScaleMomentum(const size_t max, const size_t num, const size_t *elements, T **scale, T **variable,
                               T **accumulation, T **learning_rate, S **gradient, T **momentum,
                               cudaStream_t cuda_stream) {
  size_t thread_per_block = 256;
  size_t block_per_grid = (max + thread_per_block - 1) / thread_per_block;
  CombineFusedMomentumScaleKernel<<<block_per_grid, thread_per_block, 0, cuda_stream>>>(
    num, elements, scale, variable, accumulation, learning_rate, gradient, momentum);
}
// end CombineFusedScaleMomentum

// CombineFusedWeightDecayScaleMomentum
template <typename T, typename S>
__global__ void CombineFusedMomentumWeightDecayScaleKernel(const size_t num, const size_t *element_num,
                                                           T **weight_decay, T **scale, T **variable, T **accumulation,
                                                           T **learning_rate, S **gradient, T **momentum) {
  for (size_t idx = 0; idx < num; idx++) {
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < (element_num[idx]); i += blockDim.x * gridDim.x) {
      T grad = (variable[idx][i] * weight_decay[idx][0] + static_cast<T>(gradient[idx][i])) * scale[idx][0];
      accumulation[idx][i] = momentum[idx][0] * accumulation[idx][i] + grad;
      variable[idx][i] -= learning_rate[idx][0] * accumulation[idx][i];
    }
  }
}

template <typename T, typename S>
void CombineFusedWeightDecayScaleMomentum(const size_t max, const size_t num, const size_t *element_num,
                                          T **weight_decay, T **scale, T **variable, T **accumulation,
                                          T **learning_rate, S **gradient, T **momentum, cudaStream_t cuda_stream) {
  size_t thread_per_block = 256;
  size_t block_per_grid = (max + thread_per_block - 1) / thread_per_block;
  CombineFusedMomentumWeightDecayScaleKernel<<<block_per_grid, thread_per_block, 0, cuda_stream>>>(
    num, element_num, weight_decay, scale, variable, accumulation, learning_rate, gradient, momentum);
}
// end CombineFusedWeightDecayScaleMomentum
template CUDA_LIB_EXPORT void MomentumUpdateVariable<float, float, float>(const size_t size, float *variable,
                                                                          float *accumulation,
                                                                          const float *learning_rate,
                                                                          const float *gradient, const float *momentum,
                                                                          bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<half, half, half>(const size_t size, half *variable,
                                                                       half *accumulation, const half *learning_rate,
                                                                       const half *gradient, const half *momentum,
                                                                       bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<half, float, half>(const size_t size, half *variable,
                                                                        half *accumulation, const float *learning_rate,
                                                                        const half *gradient, const float *momentum,
                                                                        bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<float, float, half>(const size_t size, float *variable,
                                                                         float *accumulation,
                                                                         const float *learning_rate,
                                                                         const half *gradient, const float *momentum,
                                                                         bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<int8_t, int8_t, int8_t>(
  const size_t size, int8_t *variable, int8_t *accumulation, const int8_t *learning_rate, const int8_t *gradient,
  const int8_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<uint8_t, uint8_t, uint8_t>(
  const size_t size, uint8_t *variable, uint8_t *accumulation, const uint8_t *learning_rate, const uint8_t *gradient,
  const uint8_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<int16_t, int16_t, int16_t>(
  const size_t size, int16_t *variable, int16_t *accumulation, const int16_t *learning_rate, const int16_t *gradient,
  const int16_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<uint16_t, uint16_t, uint16_t>(
  const size_t size, uint16_t *variable, uint16_t *accumulation, const uint16_t *learning_rate,
  const uint16_t *gradient, const uint16_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<uint32_t, uint32_t, uint32_t>(
  const size_t size, uint32_t *variable, uint32_t *accumulation, const uint32_t *learning_rate,
  const uint32_t *gradient, const uint32_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<int64_t, int64_t, int64_t>(
  const size_t size, int64_t *variable, int64_t *accumulation, const int64_t *learning_rate, const int64_t *gradient,
  const int64_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<uint64_t, uint64_t, uint64_t>(
  const size_t size, uint64_t *variable, uint64_t *accumulation, const uint64_t *learning_rate,
  const uint64_t *gradient, const uint64_t *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<double, double, double>(
  const size_t size, double *variable, double *accumulation, const double *learning_rate, const double *gradient,
  const double *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<Complex<float>, Complex<float>, Complex<float>>(
  const size_t size, Complex<float> *variable, Complex<float> *accumulation, const Complex<float> *learning_rate,
  const Complex<float> *gradient, const Complex<float> *momentum, bool use_nesterov, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void MomentumUpdateVariable<Complex<double>, Complex<double>, Complex<double>>(
  const size_t size, Complex<double> *variable, Complex<double> *accumulation, const Complex<double> *learning_rate,
  const Complex<double> *gradient, const Complex<double> *momentum, bool use_nesterov, cudaStream_t cuda_stream);

template CUDA_LIB_EXPORT void FusedWeightDecayScaleMomentum(const size_t element_num, float *weight_decay, float *scale,
                                                            float *variable, float *accumulation,
                                                            const float *learning_rate, const float *gradient,
                                                            const float *momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void FusedWeightDecayScaleMomentum(const size_t element_num, float *weight_decay, float *scale,
                                                            float *variable, float *accumulation,
                                                            const float *learning_rate, const half *gradient,
                                                            const float *momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void FusedWeightDecayMomentum(const size_t element_num, float *weight_decay, float *variable,
                                                       float *accumulation, const float *learning_rate,
                                                       const float *gradient, const float *momentum,
                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void FusedWeightDecayMomentum(const size_t element_num, float *weight_decay, float *variable,
                                                       float *accumulation, const float *learning_rate,
                                                       const half *gradient, const float *momentum,
                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void FusedScaleMomentum(const size_t element_num, float *scale, float *variable,
                                                 float *accumulation, const float *learning_rate, const float *gradient,
                                                 const float *momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void FusedScaleMomentum(const size_t element_num, float *scale, float *variable,
                                                 float *accumulation, const float *learning_rate, const half *gradient,
                                                 const float *momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CombineFusedWeightDecayScaleMomentum(
  const size_t max, const size_t num, const size_t *elements, float **weight_decay, float **scale, float **variable,
  float **accumulation, float **learning_rate, float **gradient, float **momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CombineFusedWeightDecayScaleMomentum(
  const size_t max, const size_t num, const size_t *elements, float **weight_decay, float **scale, float **variable,
  float **accumulation, float **learning_rate, half **gradient, float **momentum, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CombineFusedScaleMomentum(const size_t max, const size_t num, const size_t *elements,
                                                        float **scale, float **variable, float **accumulation,
                                                        float **learning_rate, float **gradient, float **momentum,
                                                        cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CombineFusedScaleMomentum(const size_t max, const size_t num, const size_t *elements,
                                                        float **scale, float **variable, float **accumulation,
                                                        float **learning_rate, half **gradient, float **momentum,
                                                        cudaStream_t cuda_stream);
