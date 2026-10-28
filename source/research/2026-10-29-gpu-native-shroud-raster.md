# GPU 原生 SHROUD/FOG 栅格与有界遮罩缓存

## 范围与已有依据

用户要求尽量把更适合 GPU 的工作迁移。已有 CPU 主视口迷雾仍会对每个源格、目标像素和面积样本执行循环，随后再遍历完整 RGB 图像。迁移前检索研究索引、既有原生黑幕与本次性能笔记，以及持久 notebook v79 中的 `2026-10-26-native-minimap-shroud-and-underlay`。源 TMP 与目标迷雾分离的修复继续保留，不能用遮罩平滑代替它。

本项是生产宿主算法的 GPU 实现，不新增游戏地址、游戏函数调用、Hook、IPC 字段或写入。既有 SHROUD/FOG 的 native ABI/映像证据见 [原生黑幕](2026-10-26-native-minimap-shroud-and-underlay.md)；此处使用合成 60×30 SHP/地图数据进行可公开复现的算法比较，不携带私有 EXE、素材或原表。受控软件 Vulkan 不证明真实游戏画面、显卡性能或多人兼容性。

## 保留的语义

`gpu_shroud.h` 的 Builder 接收当前 Geometry/cell selector 状态及原始 SHP loader。仅做源发现、原生稳定排序、每个 stamp 的一维 X/Y 源索引和 32×32 目标分桶。源坐标沿用 CPU 原本的 double 表达式，避免 GPU float 边界舍入导致半格相差一像素；GPU 本身只使用整数，不要求 shaderFloat64。

- 保持 SHP 原始 0..127 alpha；0xFE 及其他大于127的值跳过。
- 未知目标样本初值仍为2；全部 shroud stamps 先按原生稳定 YSort 顺序执行，然后全部 fog stamps。
- fog 对已有127的样本直接取新值，其余使用 `max(0,old+incoming-127)`；没有换成普通 alpha-over。
- 保持原生 1..4×1..4 面积样本、8M样本预算及最终整数四舍五入。格子高度不移动这层平面遮罩。
- 每个输出像素由一个 invocation 负责，避免跨格写入竞争。四个连续像素的结果打包进一个 uint，和场景 `byteAt` 的0..127输入一致。

没有使用双线性模糊、独立格子纹理缩放或先按雷达 RGB 平面格可见性丢弃遮罩源。这些做法无法保留现有 skip、半格及目标像素语义。

## 同一 GPU 批次与生命周期

`vulkan_shroud_backend.h::Stage` 共享 FrameResolve 的设备和命令缓冲。它只 prepare/record，不自行提交队列、不等待 fence、不读取每格或每层结果。输出进入场景 material atlas，最后和颜色一起进行一次统一回读；CPU 如需 `feedbackVisibility` 可从同一回读里的紧凑 alpha 字节取得。

Stage 保留1MiB有界原始 alpha 源区、至多64MiB metadata buffer和至多16MiB紧凑 mask buffer，另有驱动实现开销；这些是 Stage 自身的逻辑容量，不等于整个 GPU 后端的总内存。源缓存按实际共享所有权和 vector 地址识别；同地址新所有权不能命中旧素材。Builder 比较原始 SHP crop/indices 内容，内容变化产生新的不可变 owner。

相同不可变 Plan 重用已经完成的 GPU mask，避免稳定迷雾每帧重新执行数百万次 stamp。场景 material slot 未变化时协调器可完全跳过 mask record；slot 重排时只作设备内拷贝。改变相机、selector、crop或原始 SHP必须建立新 Plan。

命令记录不代表 GPU 完成。Stage 只在协调器 fence 成功后的 `commit()` 发布 mask cache 身份，丢弃未提交的 command buffer不会产生“有效”缓存；mask buffer增容也会使先前结果失效。Stage 沿用协调器的有界超时隔离，未完成的资源不能提前释放。非inflight的分配失败可经 `release()` 在关闭共享设备前释放所有Stage映射/缓存，为32位CPU回退腾出地址空间；重复release及随后析构均清空句柄，设备已关闭后不会再调用旧Vk入口。此清理不释放仍在运行的逾时资源。

## 验证

证据：[gpu-shroud-raster.json](evidence/gpu-shroud-raster.json)。

- `gpu_shroud.cpp`：13组原始生产 `Raster::build` 对照，10,755,133个 alpha像素完全一致；覆盖4K、1.5×、分数crop、强缩小的面积样本、裁剪SHP、shroud/fog顺序、相同YSort及重复stamp、透明skip、未知边界、原始源内容替换和坏输入。优化版与 ASan/UBSan 通过；受环境限制关闭 LSan。
- 在一组稠密合成4K case中，CPU原始stamp约311.4ms，CPU生成GPU坐标/分桶约7.98ms；4331个stamp、635322个坐标、44016个bin references。它说明迁移目标的 CPU 工作量，不是 GPU 计时、实机延迟或 FPS。
- shader bootstrap `40742252357` 使用 glslang/SPIR-V Tools 编译并验证 Vulkan1.0；整数 kernel 的源 SHA-256 `331d054b1b5fce4de1b20c540a2a18431812dc54f747af3c43f6c3f73de39289`，SPIR-V `2de6bf0402be4e5c0404f6ebf2c22995d8c8d33b60796958249fbc8dc307ecb1`，6572bytes。
- 实际 Vulkan bootstrap `40742855018` / job `123008773039`：llvmpipe(LLVM20.1.2)执行28帧，38,099,066个 alpha像素与生产 CPU完全一致；稳定 Plan 的 GPU mask dispatch及素材上传均为0；每帧一次回读。前后canary、未提交命令丢弃后重试、原始源所有权替换均通过。ASan/UBSan 与 Vulkan validation layer无错误，运行日志由 Python检查。
- Zig0.13.0 Win32 x86 `-O2 -UNDEBUG` 交叉编译通过。Windows硬件执行尚未验证；主场景 FrameResolve 的颜色+mask atlas整合另由 `vulkan_shroud_scene.cpp` 和最终主 CI 检查。

shader以一次写四像素的方式输出，并在最大16M目标像素时使用二维 workgroup dispatch，避免超过 Vulkan1.0 最小65535个 X workgroup上限。独立运行后补充了显式 local-size/device-limit拒绝条件，以及与主视口相同的有限crop/32768跨度限制，避免对异常巨大double做整数转换；未改变有效输入的 kernel 算法，最终主 CI 应对最终源码重新执行。
