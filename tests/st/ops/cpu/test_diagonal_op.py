# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

import numpy as np
import pytest

from mindspore import Tensor
import mindspore.context as context
from mindspore.ops import functional as F
from mindspore.common import dtype as mstype


def test_diagonal_functional_api():
    """
    Feature: test diagonal functional API.
    Description: test diagonal functional API and compare with expected output.
    Expectation: output should be equal to expected value.
    """
    input_x = Tensor([[0, 1], [2, 3]], mstype.float32)
    output = F.diagonal(input_x)
    expected = np.array([0, 3], np.float32)
    np.testing.assert_array_equal(output.asnumpy(), expected)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_diagonal_functional_api_modes():
    """
    Feature: test diagonal functional API for different modes.
    Description: test diagonal functional API and compare with expected output.
    Expectation: output should be equal to expected value.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    test_diagonal_functional_api()
    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")
    test_diagonal_functional_api()
