# 矿石动画的局部地形重建与跨帧 GPU 素材保留

## 问题、已有记录与证据范围

用户报告渲染升级约每秒卡顿，并要求把适合的绘制工作移到 GPU。整帧 GPU 管线已经移走 xBRZ、动态合成、VXL 像素栅格和原生迷雾，但静态 TMP/覆盖图的 native bank 仍由 CPU 准备。调查前检索研究索引、[反馈与性能修复](2026-10-29-render-feedback-performance.md)、[原尺寸分页合成](2026-10-25-native-minimap-composition.md)、[目标像素地形](2026-10-28-target-terrain-composition.md)和持久 notebook v79 的相关条目。

这是宿主生产代码、合成资源及受控计数/像素 oracle 的研究，没有新增游戏地址、原函数调用、Hook 或进程写入。游戏 EXE SHA、ABI、字段偏移不适用于这个缓存算法；相关素材/光照字段仍沿用既有笔记。本项不能证明用户机器的周期卡顿均来自此处，也不能把 CPU 合成计时换算成游戏 FPS。

## 发现：一个覆盖图帧变化会使整个 native bank 失效

原 `SceneRenderer::sameSource` 正确地把覆盖图帧纳入静态源身份，但任一 `overlayFrame` 变化都会进入完整重画。即使只有一格矿石动画前进，旧路径也会丢弃全部 native 地形页和连续重建缓存，按4096格一轮重画本地源。GPU xBRZ 能去掉放大工作，却不会自动去掉这一层 CPU 原尺寸重画。

受控合成 workload：128×128合法菱形地图、每格60×30/约900不透明像素 TMP、一个12×12覆盖图换帧，目标3840×2160，native crop2560×1440（1.5×），CPU xBRZ 模式3。三次旧路径各重画 **16022格**、需4次调用/3次空结果，总耗时 **733.940、713.497、703.837ms**。相同 workload 的新局部路径每次重画 **35格**、1次调用/0次空结果，总耗时 **114.972、123.827、104.480ms**。未变化时两者重画均为0。计时包含完整 CPU 输出/xBRZ，既不是35格本身的耗时，也不是GPU计时、游戏帧率或驱动呈现延迟；单机少量合成样本不构成普遍性能保证。

覆盖源准入为了抬高/extra TMP 保守保留512像素范围，这是必要的源搜索；它不意味着必须在目标覆盖区之外也分配页面。新路径额外把 native 画布写入范围限制到当前 coverage 的128像素页边界，保留原有源搜索范围，能进入可见目标的远处 TMP 仍会参与绘制。

## 有界局部重建

局部路径只在主视口、旧 bank 完整、背景光照相同、已绘制顺序完整时启用。`overlayFrameChanges` 比较 geometry、reveal、baseColors、cell/type/sub/level、overlay/source type/index、sourceColor 等原身份，允许至多256格仅覆盖图帧不同。类型、地形、高度或全局光照变化继续完整重画；当前可见范围的实际 per-cell light/overlayOffset/bridge 签名也必须与旧 bank 一致。迷雾仍由目标 mask 独立处理。

1. 取得旧帧和新帧的实际 Image 边界，递归包含附件/阴影，并沿用原生锚点和覆盖图偏移。缺失或不安全的旧/新素材、预算和检查失败均放弃局部路径。
2. `NativeCanvas::repaint` 共享旧 bank 未变化的页，只清空与这些新旧边界相交的整张128×128页。至多256张脏页，预先检查旧页与替代页同时存在的预算，旧 bank 在提交前保持不可变。
3. 依原 painter 顺序重新执行所有可能进入这些脏页的地形、underlay 和覆盖图，包含相邻格的遮挡、Z 和阴影。写入门限阻止修改其他页；删除旧帧像素同样会失效，即使新帧没有像素。
4. 全部局部工作完成并再次验证现场光照后才提交新 bank。无法安全局部重建时使用原完整路径，不降分辨率、不跳过光照校验。

这里重新执行的是整页相关源，不能简单在旧彩色图上覆盖矿石新帧；后一做法无法擦除旧帧、恢复背景和重现相邻 TMP 遮挡。

## 连续 xBRZ 与 GPU 缓存

连续地形仍按原262×262源块（含 halo）重建。`RegionCache::invalidateRegion` 仅删除源 footprint 与脏128像素页相交的 CPU scaled/raw source 项；其他项保留原 immutable shared owner。因此在 GPU 色图集内，远处素材既不重新上传也不重新执行 xBRZ。邻域 halo 命中的源块必须重建，不能只按矿石自身矩形失效以免接缝。

这个优化没有把所有 TMP 解码、native 地形合成、游戏内存读取和身份验证移到 GPU；它减少这些工作发生的范围。GPU 各像素阶段仍在一份 frame batch 内衔接，末尾统一回读 RGBA/必要 mask 给现有 Win32 `UpdateLayeredWindow`，尚未实现 GPU 直接呈现。

## 验证与未决

`source/assist/tests/native_repaint.cpp` 的优化版及 ASan/UBSan（`detect_leaks=0`，不主张LSan）通过：新帧与删除帧的完整颜色、逐bit浮点Z均等于完整重建，覆盖 underlay/阴影/遮挡/NaN；18张旧页只新增1张替代页，旧 bank 不变；6个原始GPU源块中2个远处owner保留、4个相交halo失效；删除最后像素、clean/fork门限清除、coverage裁剪和预算失败保持均通过。

生产 `SceneRenderer` 的 `source/assist/tests/terrain_partial_scene.cpp` 优化版通过：原生/1.5×两种场景共10个局部bank，局部累计重画730格，初始完整绘制累计4030格，每次局部bank仅1次调用；每轮局部/完整回退后与新建完整renderer比较，**8036352个输出像素逐字节一致**。包含改变/删除/仅附件的覆盖图、阴影、抬高cliff、actor及baseRGB交叠；source/sub/level/type/color、overlay描述、baseColors、当前局部/全局光照及色盘变化均验证完整回退。详细数值和 sanitizer 状态见 [terrain-dirty-recomposition.json](evidence/terrain-dirty-recomposition.json)。两项测试均已通过优化构建及 ASan/UBSan（detect_leaks=0）；生产场景10次局部更新的8,036,352个输出像素与新建 renderer 完整重画完全一致。最终主 CI 必须对最终源提交重新执行，不能用此前 GPU bootstrap 代替新缓存整合的检查。

尚无本项的真实显卡、游戏画面或游戏 FPS 证据。大量矿石同时换帧、光照或地形变化、覆盖页/预算超限仍可能走完整 bank；动态场景、原生迷雾采样和最终窗口提交仍有独立开销。
