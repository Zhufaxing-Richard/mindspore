mindspore.ops.coo_sinh
=======================

.. py:function:: mindspore.ops.coo_sinh(x: COOTensor)

    逐元素计算输入COOTensor的双曲正弦。

    .. math::
        out_i = \coo_sinh(x_i)

    参数：
        - **x** (COOTensor) - coo_sinh的输入COOTensor，其秩范围必须在[0, 7]。

    返回：
        COOTensor，shape与 `x` 相同。

    异常：
        - **TypeError** - 如果 `x` 不是COOTensor。
