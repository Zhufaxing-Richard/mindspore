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

#include <set>

#include "ops/op_utils.h"
#include "ops/reverse_v2.h"
#include "utils/check_convert_utils.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
void ReverseV2::Init(const std::vector<int64_t> &axis) { this->set_axis(axis); }
void ReverseV2::set_axis(const std::vector<int64_t> &axis) { (void)this->AddAttr(kAxis, api::MakeValue(axis)); }
std::vector<int64_t> ReverseV2::get_axis() const {
  auto value_ptr = GetAttr(kAxis);
  return GetValue<std::vector<int64_t>>(value_ptr);
}

MIND_API_OPERATOR_IMPL(ReverseV2, BaseOperator);
REGISTER_PRIMITIVE_C(kNameReverseV2, ReverseV2);
}  // namespace ops
}  // namespace mindspore
