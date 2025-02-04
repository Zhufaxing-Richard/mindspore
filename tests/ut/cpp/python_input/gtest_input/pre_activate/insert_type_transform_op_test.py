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
import mindspore
from mindspore.common.tensor import Tensor
from mindspore.ops import Primitive
from mindspore.ops import operations as P
from mindspore.ops import _constants as Constants
tuple_get_item = Primitive(Constants.kTupleGetItem)

make_tuple = Primitive('MakeTuple')
split1 = P.Split(1, 2)
split2 = P.Split(0, 2)
add1 = P.AddN()
add2 = P.AddN()

input_x = Tensor(np.array([[1, 1, 1, 1], [2, 2, 2, 2]]), mindspore.int32)


class FnDict:
    def __init__(self):
        self.fn_dict = {}

    def __call__(self, fn):
        self.fn_dict[fn.__name__] = fn

    def __getitem__(self, name):
        return self.fn_dict.get(name)


def test_tuple_unfold_to_tuple_unfold_transform(tag):
    """
    Feature: Dynamic shape.
    Description: Test TupleUnfold to TupleUnfold transforming pass.
    Expectation: The 'after' graph is identical to the graph after this pass.
    """
    fns = FnDict()

    @fns
    def before(x):
        res = split1(x)
        res = add1(res)
        res = split2(res)
        res = add2((tuple_get_item(res, 0), tuple_get_item(res, 1)))
        return res

    @fns
    def after(x):
        res = split1(x)
        res = add1(tuple_get_item(res, 0), tuple_get_item(res, 1))
        res = split2(res)
        res = add2(tuple_get_item(res, 0), tuple_get_item(res, 1))
        return res

    return fns[tag]
