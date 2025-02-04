mindspore.ops.csr_floor
========================

.. py:function:: mindspore.ops.csr_floor(x: CSRTensor)

    CSRTensor逐元素向下取整函数。

    .. math::
        out_i = \lcsr_floor x_i \rcsr_floor

    参数：
        - **x** (CSRTensor) - Floor的输入，任意维度的CSRTensor，秩应小于8。其数据类型必须为float16、float32。

    返回：
        CSRTensor，shape与 `x` 相同。

    异常：
        - **TypeError** - `x` 的数据类型不是CSRTensor。
        - **TypeError** - `x` 的数据类型不是float16、float32、float64。