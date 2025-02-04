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

"""Spectral operators."""
from mindspore._checkparam import Validator as validator
from mindspore.common import dtype as mstype
from mindspore.ops.primitive import Primitive, prim_attr_register


class BartlettWindow(Primitive):
    r"""
    Bartlett window function.

    Refer to :func:`mindspore.ops.bartlett_window` for more details.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> window_length = Tensor(5, mstype.int32)
        >>> bartlett_window = ops.BartlettWindow(periodic=True, dtype=mstype.float32)
        >>> output = bartlett_window(window_length)
        >>> print(output)
        [0.  0.4 0.8 0.8 0.4]
    """

    @prim_attr_register
    def __init__(self, periodic=True, dtype=mstype.float32):
        """Initialize BartlettWindow"""
        self.add_prim_attr("max_length", 1000000)
        validator.check_value_type("periodic", periodic, [bool], self.name)
        validator.check_value_type("dtype", dtype, [mstype.Type], self.name)
        valid_values = (mstype.float16, mstype.float32, mstype.float64)
        validator.check_type_name("dtype", dtype, valid_values, self.name)


class BlackmanWindow(Primitive):
    r"""
    Blackman window function.

    Refer to :func:`mindspore.ops.blackman_window` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> window_length = Tensor(10, mindspore.int32)
        >>> blackman_window = ops.BlackmanWindow(periodic = True, dtype = mindspore.float32)
        >>> output = blackman_window(window_length)
        >>> print(output)
        [-2.9802322e-08  4.0212840e-02  2.0077014e-01  5.0978714e-01
          8.4922993e-01  1.0000000e+00  8.4922981e-01  5.0978690e-01
          2.0077008e-01  4.0212870e-02]
    """

    @prim_attr_register
    def __init__(self, periodic=True, dtype=mstype.float32):
        """Initialize BlackmanWindow"""
        self.add_prim_attr("max_length", 1000000)
        validator.check_value_type("periodic", periodic, [bool], self.name)
        validator.check_value_type("dtype", dtype, [mstype.Type], self.name)
        valid_values = (mstype.float16, mstype.float32, mstype.float64)
        validator.check_type_name("dtype", dtype, valid_values, self.name)
