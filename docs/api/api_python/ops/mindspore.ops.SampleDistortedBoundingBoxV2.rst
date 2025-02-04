mindspore.ops.SampleDistortedBoundingBoxV2
===========================================

.. py:class:: mindspore.ops.SampleDistortedBoundingBoxV2(seed=0, seed2=0, aspect_ratio_range=(0.75, 1.33), area_range=(0.05, 1.0), max_attempts=100, use_image_if_no_bounding_boxes=False)

    为图像生成单个随机扭曲的边界框。

    在图像识别或物品定位中，除了提供真实标签之外，通常还提供边界框注释。用于训练这种系统的常用技术是在保持图像内容的同时随机扭曲图像，即数据增强。
    此算子根据给定的 `image_size` 、 `bounding_boxes` 和一系列约束输出对象的随机扭曲定位，即边界框。输出以3个Tensor的形式返回，分别是： 
    `begin` 、 `size` 和 `bboxes` 。前2个Tensor可以直接输入到mindspore.ops.Slice中，以裁剪图像。第三个Tensor即为生成的扭曲边界框。

    参数：
        - **seed** (int，可选) - 如果 `seed` 或 `seed2` 设置为非零，则随机数生成器将使用这两个给定的的种子。否则，将使用随机数作为种子。默认值：0。
        - **seed2** (int，可选) - 避免种子冲突的第二个种子。默认值：0。
        - **aspect_ratio_range** (Union[list(float), tuple(float)]，可选) - 指定图像的裁剪区域必须具有宽高比范围。区域宽高比 = area_width / area_height。这个属性应为正。默认值：(0.75, 1.33)。
        - **area_range** (Union[list(float), tuple(float)]，可选) - 图像的裁剪区域必须包含在此范围内提供的图像。此属性的值应该在范围(0.0, 1.0]内。默认值：(0.05, 1.0)。
        - **max_attempts** (int，可选) - 尝试生成指定约束的图像的裁剪区域的次数。 `max_attemps` 次失败后，返回整个图像。此属性的值应为正。默认值：100。
        - **use_image_if_no_bounding_boxes** (bool，可选) - 如果没有提供边界框，则此参数控制算子行为。如果没有边界框提供（ `bounding_boxes` 的shape为 :math:`(0, N, 4)` 或者 :math:`(batch, 0, 4)` ），并且此属性设置为True，然后假定一个隐式边界框涵盖整个输入，但如果此属性设置为False，则引发报错。默认值：False。

    输入：
        - **image_size** (Tensor) - 包含[height, width, channels]三个元素的一维Tensor，其中每个值都应大于零。
        - **bounding_boxes** (Tensor) - shape 为 :math:`(batch, N, 4)` 的3维Tensor，用于描述于输入image相关联的N个边框。该输入的值应该在[0.0, 1.0]的范围内。数据类型支持float32。
        - **min_object_covered** (Tensor) - 图像的裁剪区域必须至少包含此比例的任何提供的边界框。此参数的值应该在范围[0.0, 1.0]内。在为0的情况下，裁剪区域不需要重叠任何提供的边界框。数据类型支持float32。

    输出：
        - **begin** (Tensor) - 包含[offset_height, offset_width, 0]的一维Tensor，数据类型与 `image_size` 一致。
        - **size** (Tensor) - 包含[target_height, target_width, -1]的一维Tensor，数据类型与 `image_size` 一致。当 `image_size` 数据类型为uint8的时候， `size` 的最后一个值将由原来的-1强制转变为255。
        - **bboxes** (Tensor) - 3维Tensor，shape为 :math:`(1, 1, 4)` 。包含随机扭曲后的边框。数据类型支持float32。

    异常：
        - **TypeError** - `image_size` 不是Tensor。
        - **TypeError** - `bounding_boxes` 不是Tensor。
        - **TypeError** - `min_object_covered` 不是Tensor。
        - **TypeError** - `seed` 、 `seed2` 或者 `max_attempts` 不是int类型。
        - **TypeError** - `aspect_ratio_range` 不是float类型的list或者tuple。
        - **TypeError** - `area_range` 不是float类型的list或者tuple。
        - **TypeError** - `use_image_if_no_bounding_boxes` 不是bool类型。
        - **ValueError** - `image_size` 、 `min_object_covered` 维度不为1。
        - **ValueError** - `image_size` 含有的元素个数不是3。
        - **ValueError** - `bounding_boxes` 维度不是3。
        - **ValueError** - 每个 `bounding_boxes` 含有的元素个数不是4。
        - **ValueError** - `min_object_covered` 含有的元素个数不是1。
        - **ValueError** - `aspect_ratio_range` 为list或者tuple并且含有的元素个数不是2。
        - **ValueError** - `aspect_ratio_range` 的值不全是正数。
        - **ValueError** - `aspect_ratio_range` 第二个值小于等于第一个值。
        - **ValueError** - `area_range` 为list或者tuple并且含有的元素个数不是2。
        - **ValueError** - `area_range` 不在范围(0.0, 1.0]内。
        - **ValueError** - `area_range` 第二个值小于等于第一个值。
        - **ValueError** - `max_attempts` 不是正整数。
        - **ValueError** - `use_image_if_no_bounding_boxes` 为False并且没有提供任何的边框。
        - **RuntimeError** - `image_size` 的值包含非正数。
        - **RuntimeError** - `bounding_boxes` 不在[0.0, 1.0]范围内。
        - **RuntimeError** - `bounding_boxes` 不能组成一个有效边框。
        - **RuntimeError** - `min_object_covered` 不在[0.0, 1.0]范围内。
