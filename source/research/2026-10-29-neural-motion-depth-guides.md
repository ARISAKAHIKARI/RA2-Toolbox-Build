# 神经渲染的实例运动、正交深度与保守历史重置

## 问题与证据范围

DLSS 5 神经渲染需要当前画面对应的运动、深度和控制遮罩。已有工具侧 Vulkan 场景只返回最终 RGBA，不能把全零运动、共享素材身份或任意三维距离冒充这些输入。本次只修改工具侧渲染记录、shader 和测试，沿用已采样的 Marker 实例、姿态与相机数据；没有新增游戏地址、游戏函数调用、Hook 或写入。因此没有新的游戏 PE SHA/ABI/偏移结论。

证据是生产 C++/GLSL、独立 CPU Surface 像素对照、软件 Vulkan 实际 dispatch 和受控故障。没有取得 NVIDIA 实机 DLSS 推理或真实游戏画质、帧率结论。

## 当前像素到上一画面的对应

`vulkan_scene_types.h` 为每个 draw 保存独立实例键：object、Abstract ID、类型、kind，以及附件的精确索引路径。共享同一 SHP/VXL 姿态的两个单位不能共用运动身份。附件路径不使用可能碰撞的 hash；改变阴影绘制先后也不会改变附件索引含义。

`gpu_motion.h::History` 只在相同上下文、地形版本、尺寸、采样倍率和深度区间内继续历史。原素材/几何还须保持同一个不可变 shared ownership 和对象地址，布局、深度规则及倍率一致。旧几何仅以弱引用保存，不为历史锁住上一画面的素材。VXL 使用缓存的 Prepared 包身份；每轮新分配的外层 Image 不会使相同 Prepared 误判为新姿态。

静止地形的运动为 `(当前原生相机位置 - 之前位置) × 输出倍率`。同一刚性实例为 `之前输出锚点 - 当前输出锚点`，已经包含实际相机移动与单位移动；单位不需要速度预测。数值是输出像素单位的“当前像素→上一画面像素”，不是反号方向。

新增/消失/重复或未知实例、SHP 动画或 VXL 姿态包变化、上下文/地形/投影变化、倒退或过旧采样、瞬移等均触发整幅神经历史重置。重置画面明确输出零对应量，不能声称它保持了可靠的跨帧对应。已知相同姿态平移才复用历史。

## 深度、遮罩与提交边界

场景 shader 在原有每像素 Z 测试与实际覆盖顺序中生成 guides，不另外在 CPU 猜测最前面的物件。深度是已有 2.5D 场景 Z 的稳定反向正交归一化，区间为工具世界坐标的 `[-4096,32768]`；较大 Z 更近。它不是透视距离、原生三维网格或新增的物理深度。超区间、NaN、无穷和未知深度关闭该像素的神经作用。

透明混合、阴影、未知实例/无 Z 图案、迷雾、画布裁剪等像素输出 confidence=0。消费端必须在推理后将这些像素恢复为原输入，不能只依赖网络本身尊重遮罩。完整不透明、经过有效 Z 测试且有可靠实例身份的物件才可输出 confidence=1；被 Z 拒绝的 draw 不会抢占地形运动或深度。

guides 与 RGBA 放在同一 SSBO、同一命令批次和一次最终回读内：RGBA 后附每像素三个 uint，分别是两个 float 运动分量，以及 float 深度。非负深度的符号位保存 confidence=0 标志，不降低浮点深度精度。CPU 解包成 motion(float2)、depth(float)、confidence(float)，供独立神经进程使用。这里增加了回读字节数；不能称为 GPU 直接呈现或零拷贝。

只有请求 guides 时才分配附加空间、规划历史和回读这些平面。关闭时恢复普通 RGBA 分配，并丢弃旧 guides。设备 storage range 不足或 guides 分配失败时，本帧重放普通 GPU 合成；失败尺寸做有界缓存，避免每帧重复申请，关闭请求或改变尺寸后可重试。普通场景失败仍沿用原 CPU 回退，不发布部分图像。

`SceneRenderer::guideEnabled()` 控制收集；只有完整 GPU 场景成功且没有随后补画的缺素材标记，`Scene.guides` 才返回当前结果。消费端还必须比较 guides.sequence 与上一次成功神经处理的序号：跳帧、缺 guides、失败或上下文变化后的下一幅都要重置神经历史。GPU 收集成功不等于神经推理成功。

## 验证

- `assist/tests/gpu_motion.cpp`：同姿态多实例的独立平移、相机移动、重排、精确附件路径、相同地址不同 ownership、姿态/类型/地形/上下文/尺寸变化、时间回绕及弱引用释放；O2 和 ASan/UBSan 通过。
- `assist/tests/vulkan_guides.cpp`：实际 shader 输出运动、深度和控制；与生产 CPU Surface 对照 RGBA；覆盖 Z 拒绝、透明/阴影/迷雾/裁剪、未知实例重置、关闭和无效输入，以及普通非场景 resolve 不泄漏旧 guides。
- `assist/tests/gpu_scene_renderer.cpp`：真实 SceneRenderer 返回 guides；GPU 回退时不附旧 guides。原有 27 幅、7,516,800 个通道比较最大误差 0；遮罩比较 1,044,000 像素。
- `assist/tests/vulkan_allocation_fallback.cpp`：64KiB 映射预算允许普通 RGBA、拒绝附加 guides；验证同帧继续 GPU、失败尺寸不反复申请、显式关闭/尺寸改变后恢复；同时保留原有主场景和面积滤波资源释放回归。

隔离的 shader bootstrap 分支只用于生成 SPIR-V 和实际软件 GPU 回归，不能替代最终产品同提交 CI。最终验证记录由发布时的构建记录关联。

本次隔离 bootstrap 为 run `40822553166`、commit `d34647ba45c01532e3f5971c451449c6cc9e5970`、job `123278089662`。六项夹具在 llvmpipe、ASan/UBSan 和 Khronos validation 下通过，未发现 validation error/VUID。`vulkan_scene` 的 301,712 个通道比较最大误差 1；`gpu_scene_record` 的 20,736 个通道比较最大误差 0。上列 SceneRenderer 比较同样实际执行 GPU。64KiB 可选 guides 故障和原有全场景/面积滤波故障均有独立 PASS 标记。

当前 GLSL SHA256 为 `310424232a10f3249a3096a4db09f796f47192752b781758bd434e486d956d3d`，生成头为 `036f4422dd87c4e0eb4a56aa2703c747d6d7eb2f642e8b9be6b77b9ea9ced556`。下载的 evidence ZIP 为29,849字节，SHA256 `b5e332d0809e223f55abf6677b016d217f4ff36ffa4019233a9aca3d90b4965e`；CRC及内含生产 shader、FrameResolve、types、motion planner 和生成头的哈希均已与本地冻结文件核对。
