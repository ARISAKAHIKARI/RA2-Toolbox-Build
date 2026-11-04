# Vulkan GPU 场景管线：xBRZ、VXL、合成与原生迷雾

## 问题、来源与范围

用户先要求尝试 GPU，随后明确“尽量把能做到 GPU 上性能更好的都做到 GPU 上”。因此实现已从分数倍率解析扩展到整帧管线：原生 1x 的动态像素合成也可以使用 GPU，不以大于 1x 为前提。

已检索 INDEX 和持久笔记中的既有 VERA20K 研究。固定 VERA20K commit `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f` 是 Rust 引擎，render 使用 wgpu，依赖 wgpu 29.0.4，不能无适配直接插入本 C++ 外部采样器。官方 wgpu-native `v29.0.1.1` 有 i686 Windows MSVC 包，不能声称 32 位 Windows 不受支持。本次选择 Vulkan 1.0 动态入口和小型独立 compute kernels，保留现有游戏数据采集与 Win32 分层窗口。

依赖为未修改的 [Khronos Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/b379292b2ab6df5771ba9870d53cf8b2c9295daf)，tag `v1.3.290`、commit `b379292b2ab6df5771ba9870d53cf8b2c9295daf`。`third_party/vulkan/UPSTREAM.json` 逐文件记录官方路径、字节数、Git blob 和 SHA-256，保留 Apache-2.0/MIT 文本。Windows 仅从系统目录动态载入 `vulkan-1.dll`；不增加静态 Vulkan 导入，不附送 loader，不搜索游戏目录同名库。官方 PFN 保留 Win32 `__stdcall` ABI。

## GPU 执行的工作

`vulkan_scene_types.h` 定义不可变共享素材、原始覆盖/Z/索引、现场 light gains、目标几何、绘图顺序、地形和遮罩；`FrameResolve` 统一准备、提交和回读，其他 Stage 只能在同一 command buffer 中 record，不能各自 submit/readback。

1. **地形与 SHP xBRZ**：上传原始 native PARGB，GPU corner preprocess 与 2/3/4x scale 写入常驻色图集，然后解析实际输出倍率。实现对应未修改上游 xBRZ 1.9 的 argbUnbuffered、颜色距离、旋转和整数混色规则，不替换为自定义滤波。双精度 shader 禁止相关 FMA 收缩；设备必须支持 `shaderFloat64`。原始 tile/已现场着色的 SHP native source 保持不可变，稳定素材不重复上传或重建。
2. **VXL 光栅化**：CPU 保留现有 pose、坡面、VPL 和投影准备；GPU 执行逐三角形覆盖、深度赢家、最早面 tie-break 与 RGBA/index resolve。结果直接留在色图集/索引图集，再参与场景合成，无逐单位回读。不能声称原始 VXL 模型的全部顶点变换和 VPL 计算已经移到 GPU。
3. **动态场景**：32x32 分桶，每个输出像素由一个 invocation 独占，并按原 painter 顺序执行桶内指令；包含现场光照、emissive 索引、原始 coverage、原始最近邻 Z、NaN/no-Z、阴影、alpha 与 depth-write 规则。这样避免每像素扫描所有单位，或逐 sprite framebuffer/barrier。原生 1x 的 nearest 分支不要求 FP64。
4. **采样精度**：bilinear/adaptive/reconstructed 使用独立 FP64 shader，保留 SmoothRGB 在插值前未裁剪的光照贡献，以及原始 coverage/Z 的准入；不支持相应精度则整帧回退原 CPU 路径，不静默更换算法。FP64 scene 管线按需创建。
5. **原生地形 Z 映射**：CPU保留NativeCanvas.depthRegion的原尺寸浮点平面，仅构造O(w+h)最近邻轴索引；GPU按输出像素查原始Z，避免相机移动时CPU重新展开4K完整Z平面。CPU回退时才展开原先的输出Z，保留原始深度语义。
6. **原生迷雾**：CPU 发现实际 SHROUD/FOG source、顺序与每行/列采样坐标；GPU 用整数构造原生 0..127 mask、执行 fog/shroud 覆盖和最终变暗。mask 按四字节打包，稳定 Plan 不重新 raster；需要 CPU 原生反馈可将 packed mask 与最终颜色放到同一次最终回读批次。

NativeCanvas 静态 TMP/覆盖图的原尺寸合成仍是 CPU 缓存；VXL 的三角形准备、采样/资源发现和游戏数据校验仍在 CPU。最终 RGBA 回读给现有 Win32 layered-window 呈现，尚不是 GPU 直接扫描输出，也不是整个引擎重写。

## 缓存、预算与故障

缓存比较 shared ownership 身份、对象地址、尺寸/重建倍率签名；单纯地址复用不会命中旧素材。常驻项目只保留弱引用。上传、GPU 生成和最终缓存身份均在完整 fence 成功后才被视为有效；放弃录制不能把尚未生成的 mask/xBRZ 标记为已完成。

色图集上限由第一阶段 96 MiB 提至 **128 MiB**，容纳 4K 带 overscan 的约 23M reconstructed 地形像素与 actors。材料图集单独限制 **96 MiB**，xBRZ native source 图集 **96 MiB**。最终颜色至多 16M 像素/64 MiB；颜色和反馈 mask 的同批回读逻辑数据至多约 80 MiB，当前容量增长可保留 128 MiB readback buffer。还有有界坐标、上传、命令、xBRZ scratch、VXL triangles/scratch 和 mask 缓存；不能把某一个图集预算描述成整个 GPU 后端的内存用量。显式 Vulkan buffer 映射按实际 VkMemoryRequirements allocationSize 统一计数，全部 Stage 的 host-mapped 总上限为 **512 MiB**；诊断记录 allocated/mapped/peak-mapped 字节。这不包含驱动私有 pipeline/JIT 或进程其他 CPU cache。材料 staging 增长限制在96 MiB，不向128 MiB取整；VXL triangles独立96 MiB精确容量、单actor scratch至多128 MiB；无baseTiles的1x分支释放不用的坐标缓存。超出限制安全回退，不降图像质量，不无限增长。

普通 exact integer copy 保留拷贝路径；Vulkan CPU 软件设备不在产品中使用，避免 GPU 选项实际转到另一套 CPU renderer。显式 `RA2_VULKAN_TEST_SOFTWARE` 构建才允许 llvmpipe，并将软件 shader JIT 的测试 fence 等待预算独立设为 10s。硬件 fence 安全上限仍为 100ms；这是故障门限，既不是承诺帧时间，也不包含 driver/pipeline 初始化时间。

初始化、设备或运行时分配失败保持 CPU 回退；不逐帧重试 OOM 分配。尚未提交且没有in-flight工作的失败，会先依次释放所有Stage/Scene/Core资源，再执行CPU replay，让出32位虚拟地址空间；有已提交未完成工作的超时仍严格隔离，不能套用此释放流程。超时后不释放或卸载可能仍在执行的资源；进程级 sticky breaker 阻止切换开关/重启 worker 再创建另一套超时后端。析构仅在确认 fence 完成时释放，避免无限 DeviceWaitIdle。异常驱动挂死后所有平台的退出行为尚未实机验证。

GPU xBRZ 首次使用还会在当前设备执行固定 7x7 输入的上游 2/3/4x golden 自检；不一致即禁用该 raw reconstruction 路径。它是有限设备自检，不能证明所有输入或所有硬件。

## 实际验证与限制

Bootstrap run **40743255379**，head `deb54f6c8c649fb26dff3e8c791b7f267e3c6b7b`：官方 glslang 编译、SPIR-V Tools 校验四组 shader；ASan/UBSan 和 Khronos validation 下实际 llvmpipe LLVM 20.1.2 执行通过：

| 实际执行测试 | 结果 |
| --- | --- |
| `vulkan_scene.cpp` 原生 1x + bilinear 场景 | 8 帧，301,712 channel checks，最大差 1，稳定上传 0/最终回读 1 |
| `gpu_xbrz.cpp` 与未修改上游逐位比较 | 190 输入、2/3/4x、589 帧、8,167,384 像素完全一致；570 fresh builds、16 warm frames 零上传/零重建 |
| `gpu_voxel_vulkan.cpp` 实际 VXL raster | 96 poses，261,025 RGBA/index 像素完全一致，含输出偏移 canary/深度 tie |
| `gpu_voxel_scene.cpp` VXL 常驻结果到场景 | 96 compositions，13,762,560 channel checks，最大差 1，稳定上传 0/回读 1 |
| `vulkan_shroud.cpp` 原生 mask Stage | 28 帧，38,099,066 alpha 像素完全一致；稳定 dispatch 0/source upload 0，放弃录制/地址变化/canary 通过 |

实际整合 CPU oracle 之外，原 xBRZ GLSL 逻辑的 C++ 模型、GPU mask plan 模型和 VXL 准备也分别有独立 CPU/sanitizer 回归。各 Stage 的软件执行证明算法与协调器可以工作，**不构成显卡或游戏内 FPS 提升证据**。最终组合 bootstrap **40746255697**，head `fb80648cbff8668ad38d1465d6cba19362c2286e`、scene job `123020732795`，全部3个jobs成功，重复上述全部Stage并增加以下实际GPU测试，均在ASan/UBSan与Khronosvalidation下通过：

| 生产组合与故障回归 | 结果 |
| --- | --- |
| `gpu_scene_renderer.cpp` 真实生产SceneRenderer | GPU27/27帧，7,516,800channelchecks最大差0；1,044,000反馈mask像素exact；涵盖1x/1.5x/2x、rawterrain/SHPxBRZ、deferredVXL、现场palette/pose/sameclock及GPU/CPU切换 |
| `vulkan_shroud_scene.cpp` GPUmask到FrameResolve | 26帧、21,510,266颜色像素与同数mask字节exact，稳定上传0、最终回读1，Z/索引/emissive/alpha0/clip/atlas隔离通过 |
| `gpu_depth_plan.cpp` native-plane最近邻Z | 实际GPU6帧、548深度检查/2192channelchecks最大差0；覆盖分数、夹取、1x1平面、owner复用与CPU回退 |
| `vulkan_shared_assets.cpp` 同源多实例 | 两个SHP实例仅1次xBRZ构建，两个VXL实例仅1次resident raster，warm零上传/零构建，最大差0；Stage前再按目标slot显式去重 |
| `vulkan_allocation_fallback.cpp` 真实故障注入 | 将GPU映射预算设64KiB，实际设备建立后触发分配失败；回退前全部buffer释放/计数归0、caller输出保留、CPU精确replay、重复调用不重新初始化；无排队工作被错误销毁 |

Zig0.13.0/Win32x86已交叉编译新增共享实例和分配故障测试；完整Windows产品构建与主分支CI由最终发布检查记录。

历史第一阶段：run 40729123538 shader compile 成功；首次软件 dispatch 40734749488 提前退出时崩溃，ASan run 40735269615 查明触发 100ms 首帧软件 JIT fence 超时，未报告宿主越界。隔离软件测试等待后 run 40735580583 通过 61 帧/1,483,520 exact Q8 像素；其 rg 日志过滤命令缺失已改 Python，后续本节所列实际 Stage CI 的过滤正常。

本地无 ICD，明确只验证不可用时 CPU 回退；Zig 0.13.0 Win32 x86 交叉编译通过。完整游戏内视觉、AMD/NVIDIA/Intel 实机性能、不同驱动的浮点栅格精度和 GPU 直接呈现仍待验证。没有新增游戏地址/原生函数调用/游戏内存写入，也没有变更 IPC/CnCNet 逻辑；游戏 PE SHA 不适用于本 GPU 适配器。
