# 小地图边缘 TMP 裁剪与渲染升级连续呈现

## 问题与证据范围

用户报告：小地图范围保持不变时，边缘本应露出半截的地形没有渲染；开启渲染升级后，画面在原生图像和重绘图像之间反复切换。

基线为已发布 v1.0.39-pre.6 的产品源码，及随后只增加普通 Spawner 审计资料的提交 `19fc6113037d43c3e538018a8ad88dea7358d18f`；本地同内容基线为 `a26b1bc`。调查前读取研究索引、边缘/南界/覆盖层保活笔记和同一持久 SQLite 笔记 v78。

本次是外部采样、像素合成和宿主发布策略修正，没有新增游戏函数地址、ABI、字段偏移或引擎二进制执行结论。现有引擎布局适用范围沿用先前记录；不把合成内存和素材夹具当作某个实际 EXE 的完整游戏、Windows/DWM 或联机验证。

## 1. 整格准入不能替代 TMP 像素裁剪

旧 `radar_map.h` 在读取格子前要求 `flatVisible`（平面雷达 2×1 RGB footprint 与 raw backing 相交）；reveal 模式还以玩法 `Bounds::usable` 决定该格是否保存 TMP。`radar_scene.h` 又只从非零 `Terrain::colors` 建立地形源列表。因此，平面原点位于范围外、高度上移后进入画布，或 TMP extra 图伸入画布的合法源格会被提前整格丢弃。

这与 pre.12 对原生粗雷达 RGB 的准入结论并不矛盾：粗 RGB 的格子准入、物件准入与实际 TMP 源像素是不同用途。更早记录的 Tactical `CoordinatesLegal` 绘图准入也不能反过来当作修改雷达范围的理由。

修正：

- 现有 `Terrain::colors` 保留示意图、物件和动画的原准入。`CellArt::sourceColor` 单独记录实景 TMP 源，不额外扩展物件和 overlay 准入。
- TMP 来源仍必须是物理合法格，验证 cell 指针、坐标身份和既有 shroud/fog 规则。先按现有解码器完整 TMP 最大 512×512 和 signed Level 极值作有界候选检查，再按实际 Level 缩小来源范围。
- 最终由现有 `NativeCanvas` 使用已解码图像的真实矩形和像素裁剪，不将保守来源邻域作为输出范围。
- raw width/height、`nativeViewport`、布局、导航投影和相机限位不变。原生未知目的区的后合成黑幕仍然遮挡扩展来源；fully-shrouded 源也不被采入。
- `sourceColor` 纳入背景源比较，避免新旧银行误复用。

`minimap_terrain_crop.cpp` 执行 production Sampler → SceneRenderer → NativeCanvas，逐像素检查左、右、上、下半格、raw 外的升高源、仅 extra 图相交和完全离屏的图像，共七种几何情况。所有案例输出保持 240×120，另比较示意图、点击导航、overlay 准入与原生黑幕。不是把裁剪函数另写一遍作为被测实现。

隔离旧源树后，只移除新字段/helper 的观察断言和载入计数期望以适配旧结构；保留像素断言，首个左边半格即失败。新版本 ASan+UBSan 通过；因环境不能访问 LSan 所需 `/proc` 线程信息，禁用 leak sanitizer，不宣称完成泄漏检查。

## 2. 新旧画面切换的多条直接路径

确认存在下列生产路径：

1. worker 任一次失败都发布带空 image 的 Frame，替换已经完成的图像；UI 随即清除整个原子组。
2. 有效期以请求发起时间计算且只有 400 ms；渲染或排队较慢时，新完成图可能很快或立即被判过期。
3. 采样前后逻辑帧必须完全相等，即使在现有 `freshMarkers≤2` 窗口内的正常推进也被拒绝。
4. 软件光标异步 worker 还停留在上一动画/动作帧时，反馈未就绪会撤掉整个新场景。
5. `paint()` 在 renderer 自己检查视口前，依据无关通用信息采样的 `snap.renderStatus` 提前调用 `stop()`；未知读失败也被当作确定隐藏。

本次将失败尝试与完成图像分开。完成图保留必须通过完全一致的 PID、match、窗口/尺寸/token、generation、reveal、材质/颜色/缩放配置和场景/House/Tactical/相机/视口检查。采样年龄最多 2500 ms、完成年龄最多 1500 ms；失败不能刷新这两个时间。正常新作业仍每次重新读取当前光照、色表和动态组成，没有以旧图跳过现场光照计算。

采样完成时允许现有两帧新鲜度窗口内的推进，并在 building pose 采样后再次检查；不放宽倒退、世界切换或视口变化。timeline 记录实际 raw.frame。CPU 渲染结束仍验证世界/视口，不要求渲染期间游戏时钟停止。

软件光标使用现有只读 CursorCache 同步捕获当前内存素材，位置直接取本次系统鼠标位置；缓存命中不重新解码，不再等待异步光标追上。每次光标捕获限制读取调用和总字节，无法验证时仍要保留可见的原生反馈。短暂未知读和完整反馈组保留有独立 120 ms 上限，失败不能续期；已知失焦、菜单、设置或上下文变化立即撤除。

`paint()` 先把包含 readable/visible 的状态交给 renderer 独立判断，再运行原有其他面板门槛。没有改变命令准入或其他信息层的采样策略。

## 验证入口与限制

- `assist/tests/minimap_terrain_crop.cpp`：真实采样/合成链像素与固定范围回归。
- `assist/tests/render_presentation.cpp`：完成/采样双时钟、兼容身份、未知读宽限、实际 Latest worker 的连续成功/失败发布。
- `assist/tests/render_feedback_host.cpp`：production feedback host、当前软件光标与缓存、选择反馈的内存/Win32 边界夹具。
- `assist/tests/render_presentation_gate.py`：抽取并执行真实 paint 入口；五次调用检查独立 renderer 调度和其他面板门槛。旧实现首次“信息采样失败仍应调度 renderer”断言失败。
- `.github/workflows/validate-minimap-render-fixes.yml`：portable 回归、Windows compositor 与产物构建/验证；具体结果另记录，不能在任务未结束时预称通过。

不主张这些是所有显卡/包装器/平台闪烁的唯一原因。实际用户地图与 DDraw/DWM 呈现仍需新版实机确认；本次不改原生 Runtime、联网事件、游戏状态或渲染范围，也不处理此前审计发现的实验性跨所属方命令缺陷。
