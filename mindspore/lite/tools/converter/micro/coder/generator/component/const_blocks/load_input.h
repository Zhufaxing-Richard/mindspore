/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_GENERATOR_COMPONENT_CONST_BLOCKS_LOAD_INPUT_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_GENERATOR_COMPONENT_CONST_BLOCKS_LOAD_INPUT_H_

namespace mindspore::lite::micro {
extern const char load_input_h[];
extern const char load_input_c[];
extern const char load_input_h_cortex[];
extern const char load_input_c_cortex[];
extern const char data_h_cortex[];
extern const char cortex_m7_toolchain[];
extern const char cortex_build_sh[];
}  // namespace mindspore::lite::micro
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_GENERATOR_COMPONENT_CONST_BLOCKS_LOAD_INPUT_H_
