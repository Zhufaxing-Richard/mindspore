mindspore.ops.coo_sin
======================

.. py:function:: mindspore.ops.coo_sin(x: COOTensor)

    逐元素计算输入COOTensor的正弦。

    .. math::
        out_i = coo_sin(x_i)

    参数：
        - **x** (COOTensor) - COOTensor的shape为 :math:`(N,*)` 其中 :math:`*` 表示任意数量的附加维度。

    返回：
        COOTensor，shape与 `x` 相同。

    异常：
        - **TypeError** - 如果 `x` 不是COOTensor。
        - **TypeError** - 如果 `x` 的数据类型不是float16、float32或者float64、complex64、complex128。
