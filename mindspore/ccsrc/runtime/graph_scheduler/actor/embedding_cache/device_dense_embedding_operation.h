/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_DENSE_EMBEDDING_OPERATION_H_
#define MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_DENSE_EMBEDDING_OPERATION_H_

#include <memory>
#include <utility>
#include "runtime/graph_scheduler/actor/embedding_cache/device_embedding_operation.h"

namespace mindspore {
namespace runtime {
class DeviceDenseEmbeddingOperation : public DeviceEmbeddingOperation {
 public:
  DeviceDenseEmbeddingOperation(EmbeddingCachePrefetchActor *actor, device::DeviceContext *device_context,
                                std::shared_ptr<EmbeddingDeviceCache> emb_dev_cache,
                                std::shared_ptr<EmbeddingHostCache> emb_host_cache,
                                const std::pair<int, int> &local_embedding_slice_bounds,
                                const std::pair<int, int> &local_device_cache_bounds,
                                EmbeddingCacheStatisticsInfo *statistics_info, const size_t &stream_id)
      : DeviceEmbeddingOperation(actor, device_context, emb_dev_cache, emb_host_cache, local_embedding_slice_bounds,
                                 local_device_cache_bounds, statistics_info, stream_id) {}

  ~DeviceDenseEmbeddingOperation() override = default;

  bool CountCacheMissIds(int *batch_ids, const size_t batch_ids_num, size_t data_step, size_t graph_running_step,
                         bool *device_cache_need_wait_graph, bool *host_cache_need_wait_graph) override;

 protected:
  void BuildEmbeddingCacheLookupKernel() override;
  void BuildEmbeddingCacheUpdateKernel() override;

 private:
  bool LookupDeviceCache(void *indices, void *embedding_cache, size_t indices_num, size_t cache_size,
                         size_t embedding_size, void *outputs) override;

  // Batch preprocess the current batch ids information of cache hitting or exceeding the range of the embedding table
  // slice corresponding to the process.
  bool CheckCacheHitOrOutRange(const int *batch_ids, const size_t batch_ids_len, int *hash_index, bool *in_device,
                               bool *out_range, size_t data_step);

  // Thread execution function of method 'CheckCacheHitOrOutRange'.
  bool CheckCacheHitOrOutRangeFunc(const int *batch_ids, const size_t batch_ids_len, int *hash_index, bool *in_device,
                                   bool *out_range, size_t *hash_hit_count, size_t data_step);

  // Parse the hit and swap information of the currently preprocessed id in the device cache.
  bool ParseDeviceData(int id, bool *need_swap_device_to_host, bool *need_swap_host_to_device, int *hash_index,
                       size_t data_step, size_t graph_running_step, bool *device_cache_need_wait_graph);

  DISABLE_COPY_AND_ASSIGN(DeviceDenseEmbeddingOperation);
};
}  // namespace runtime
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_DENSE_EMBEDDING_OPERATION_H_
