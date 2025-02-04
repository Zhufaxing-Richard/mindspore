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

#include "include/cuda_runtime.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/float_status_impl.cuh"
#include "include/cuda_fp16.h"

template <typename T>
__global__ void IsNan(const size_t size, const T *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (isnan(input[pos])) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}
template <>
__global__ void IsNan(const size_t size, const half *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (__hisnan(input[pos])) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}

template <typename T>
__global__ void IsInf(const size_t size, const T *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (isinf(input[pos]) != 0) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}
template <>
__global__ void IsInf(const size_t size, const half *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (__hisinf(input[pos]) != 0) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}

template <typename T>
__global__ void IsFinite(const size_t size, const T *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (isinf(input[pos]) == 0 && !isnan(input[pos])) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}
template <>
__global__ void IsFinite(const size_t size, const half *input, bool *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (__hisinf(input[pos]) == 0 && !__hisnan(input[pos])) {
      out[pos] = true;
    } else {
      out[pos] = false;
    }
  }
  return;
}

template <typename T>
__global__ void FloatStatus(const size_t size, const T *input, float *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (isinf(input[pos]) != 0 || isnan(input[pos])) {
      out[0] = 1;
    }
  }
  return;
}
template <>
__global__ void FloatStatus(const size_t size, const half *input, float *out) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < (size); pos += blockDim.x * gridDim.x) {
    if (__hisinf(input[pos]) != 0 || __hisnan(input[pos])) {
      out[0] = 1;
    }
  }
  return;
}

template <typename T>
void CalFloatStatus(const size_t size, const T *input, float *output, cudaStream_t cuda_stream) {
  FloatStatus<<<GET_BLOCKS(size), GET_THREADS, 0, cuda_stream>>>(size, input, output);
  return;
}
template <typename T>
void CalIsNan(const size_t size, const T *input, bool *output, cudaStream_t cuda_stream) {
  IsNan<<<GET_BLOCKS(size), GET_THREADS, 0, cuda_stream>>>(size, input, output);
  return;
}
template <typename T>
void CalIsInf(const size_t size, const T *input, bool *output, cudaStream_t cuda_stream) {
  IsInf<<<GET_BLOCKS(size), GET_THREADS, 0, cuda_stream>>>(size, input, output);
  return;
}
template <typename T>
void CalIsFinite(const size_t size, const T *input, bool *output, cudaStream_t cuda_stream) {
  IsFinite<<<GET_BLOCKS(size), GET_THREADS, 0, cuda_stream>>>(size, input, output);
  return;
}

template CUDA_LIB_EXPORT void CalFloatStatus<float>(const size_t size, const float *input, float *output,
                                                    cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<half>(const size_t size, const half *input, float *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<double>(const size_t size, const double *input, float *output,
                                                     cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<bool>(const size_t size, const bool *input, float *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<int8_t>(const size_t size, const int8_t *input, float *output,
                                                     cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<int16_t>(const size_t size, const int16_t *input, float *output,
                                                      cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<int32_t>(const size_t size, const int32_t *input, float *output,
                                                      cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<int64_t>(const size_t size, const int64_t *input, float *output,
                                                      cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<uint8_t>(const size_t size, const uint8_t *input, float *output,
                                                      cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<uint16_t>(const size_t size, const uint16_t *input, float *output,
                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<uint32_t>(const size_t size, const uint32_t *input, float *output,
                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalFloatStatus<uint64_t>(const size_t size, const uint64_t *input, float *output,
                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<float>(const size_t size, const float *input, bool *output,
                                              cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<half>(const size_t size, const half *input, bool *output,
                                             cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<double>(const size_t size, const double *input, bool *output,
                                               cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<bool>(const size_t size, const bool *input, bool *output,
                                             cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<int8_t>(const size_t size, const int8_t *input, bool *output,
                                               cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<int16_t>(const size_t size, const int16_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<int32_t>(const size_t size, const int32_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<int64_t>(const size_t size, const int64_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<uint8_t>(const size_t size, const uint8_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<uint16_t>(const size_t size, const uint16_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<uint32_t>(const size_t size, const uint32_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsInf<uint64_t>(const size_t size, const uint64_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<float>(const size_t size, const float *input, bool *output,
                                              cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<half>(const size_t size, const half *input, bool *output,
                                             cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<double>(const size_t size, const double *input, bool *output,
                                               cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<bool>(const size_t size, const bool *input, bool *output,
                                             cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<int8_t>(const size_t size, const int8_t *input, bool *output,
                                               cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<int16_t>(const size_t size, const int16_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<int32_t>(const size_t size, const int32_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<int64_t>(const size_t size, const int64_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<uint8_t>(const size_t size, const uint8_t *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<uint16_t>(const size_t size, const uint16_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<uint32_t>(const size_t size, const uint32_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsNan<uint64_t>(const size_t size, const uint64_t *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<float>(const size_t size, const float *input, bool *output,
                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<half>(const size_t size, const half *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<double>(const size_t size, const double *input, bool *output,
                                                  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<bool>(const size_t size, const bool *input, bool *output,
                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<int8_t>(const size_t size, const int8_t *input, bool *output,
                                                  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<int16_t>(const size_t size, const int16_t *input, bool *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<int32_t>(const size_t size, const int32_t *input, bool *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<int64_t>(const size_t size, const int64_t *input, bool *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<uint8_t>(const size_t size, const uint8_t *input, bool *output,
                                                   cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<uint16_t>(const size_t size, const uint16_t *input, bool *output,
                                                    cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<uint32_t>(const size_t size, const uint32_t *input, bool *output,
                                                    cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT void CalIsFinite<uint64_t>(const size_t size, const uint64_t *input, bool *output,
                                                    cudaStream_t cuda_stream);
