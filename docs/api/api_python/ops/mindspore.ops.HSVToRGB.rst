mindspore.ops.HSVToRGB
======================

.. py:class:: mindspore.ops.HSVToRGB

    将一个或多个图像从HSV转换为RGB。图像的格式应为：NHWC。

    输入：
        - **x** (Tensor) - 输入的图像必须是shape为 :math:`[batch, image_height, image_width, channel]` 的4维Tensor。
          channel 值必须为3。
          支持的类型：float16、float32、float64。

    输出：
        一个4-D Tensor，shape为 :math:`[batch, image_height, image_width, channel]` ，且数据类型同输入一致。

    异常：
        - **TypeError** - 如果 `x` 不是一个Tensor。
        - **TypeError** - 如果 `x` 的数据类型不是float16、float32或float64。
        - **ValueError** - 如果 `x` 的维度不等于4。
        - **ValueError** - 如果 `x` 的最后一维不等于3。
