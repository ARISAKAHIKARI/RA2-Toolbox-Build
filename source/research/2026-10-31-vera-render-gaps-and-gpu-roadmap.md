# Vera20K 渲染差距与进一步 GPU 化清单

补充：[第二轮11项独立绘制缺口](2026-10-31-vera-render-additional-gaps.md) 继续核对保存图、隐形/伪装、沉船、污迹、尾迹、信标/雷达事件、桥影、废墟、闪光和全屏效果，并标明上游近似与未完成分支。

比较基线：本仓库 Hotfix21（远端 `1b9ecb3b08932ba905212ce0259d8c25ada7ac93`）与 [Vera20K `3d42426`](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)。本次重新读取规划、VXL图集、绘制调度、光束/光照、阶段和UI源码；不是仅按文件名猜测。Hotfix22单独补路径点，其余表中项目是后续工作，**不算本轮已实现**。

## 先纠正“已完成”的口径

此前12领域总表表示已有生产入口，不表示对应领域和Vera等价。路径点就是反例：`Foot+588` 是导航队列，而非 `PlanMgr`。Hotfix22才接通真正节点/成员/回环/高亮，实施和仍有限制见[路径点记录](2026-10-31-native-planning-render-hotfix22.md)。

已实际具备、不能重复计为新优化：主画面/观察窗/小地图GPU直出、成功路径不回读整帧；VXL GPU光栅；不可变图集/姿态缓存；地图局部失效；GPU动态原尺寸页和面积缩图；GPU迷雾/有序特效；完整帧相机标识；材质/特效内容复用。完整迁移也不等于把Vera的Rust模拟器和游戏状态机搬进外部只读宿主。

## 绘制能力差距

| 优先级 | Vera已有能力与具体源码 | 我们当前实际状态 | 接入工作和依赖 |
| --- | --- | --- | --- |
| P0，本轮 | `planning.rs`、`render/planning_lines.rs` 原生规划图 | Hotfix21只有导航队列；Hotfix22补节点、共享前驱、循环、current/mega连接、徽标和高亮 | 已接入；仍需实际游戏确认；精确坡面/x87/自定义目标边界保留 |
| P1 | `instances/tether_redraw.rs` 建筑卸载时重绘radio联系人，Unit跳过普通slot | 有radio读取和一般对象排序，但没有该绘制归属规则 | 在统一draw plan中登记建筑父slot与联系人，避免重复正文、保证extras保留；不改变卸载逻辑 |
| P1 | `ui_overlays.rs`、`sim/game_entity/pip_fill.rs` 完整pips分支 | `native_unit_ui_stencil.h`仅Ammo、资源、载员的已接纳子集；驻军/控制/生成物等不全 | 增加spawn、mind-control、garrison、absorber等状态采样、锚点、颜色、空格/换行分支，不能只增加图标 |
| P1 | `radial_indicator.rs`、`presentation/radial_indicators.rs` 建筑及超武范围圈 | 现有选框、血条、普通命令线不能代替这些范围绘制 | 独立读取触发状态和真实半径，保留fade/rays/ellipse及遮挡阶段；接入公共surface操作 |
| P1 | `native_z.rs`、`tactical_a.wgsl`、`terrain_packed*.wgsl` 完整原生Z/A和位移读写规则 | 有对象深度、readonly-Z裁减及565混合；没有完整原生A-buffer/word-offset串链语义 | 建立明确的Z/A所有权和按原顺序消费的材质命令，再接遮蔽、特殊半透明/变形效果；不能用普通alpha替代A-buffer |
| P1 | `laser.rs`、`radiation_beam.rs`、`wave_geometry.rs`、`surface_line.rs` 原生几何/数值/深度规则 | `world_effects.h`使用简化线段/正弦/光晕；RadBeam2直接跳过，Wave只接0/3 | 首先补足独立效果入口，再对齐数值、颜色、Z/A和写回依赖；Hotfix21波形分批只是调度优化，不等于像素语义已完整迁移 |
| P1/P2 | `combat_light.rs`、`building_light.rs`、`presentation/combat_lights.rs` 专用光照mask及stage | 我们glow是径向加亮近似，没有完整类型16/stage80–89原生mask通路 | 原始mask生成一次缓存，在GPU按准确阶段/颜色/Z/A组合；须区分建筑灯光与普通CombatLight |
| P2 | `unit_atlas.rs` 和 `instances/units.rs` 混合SHP/VXL零件、缓存/反冲细节 | 有普通SHP车身/炮塔合成、VXL主体/炮塔/炮管和HVA；不能据此宣称任意混合附件完整 | 分开核对SHP车身+VXL炮塔、建筑VXL零件、barrel recoil、特殊阴影缓存，用同一父对象顺序组合 |
| P2 | `sidebar_render.rs`、`sidebar_surface.rs`、`sidebar_chrome*.rs` 独立侧栏/建造内容 | 世界已正确裁切；侧栏主要仍由游戏绘制，工具仅补充部分提示/素材 | 如果要独立重画，需同时接队列、暂停/取消、按钮/文字/tooltip等只读状态；不可单抄背景图就称迁移完成 |
| P2 | `placement_preview.rs` 放置预览与合法性显示 | `FoundationStencil`目前保留精确原生透明孔，合法性由游戏画面提供 | 需可靠读取/复现放置判定后才能脱离原画面；不应猜绿/红格，也不应扩大透视孔 |
| P2 | `native_surface_format.rs` RGB555/565 runtime格式 | Hotfix21 packed混合覆盖565，字体/部分色盘已有555读取，并不代表整个合成器支持555 | 把格式掩码、混合、颜色展开统一为运行时配置，保留NativeSteps与高精度档区别 |

表中 P1 的收益首先是显示完整与排序正确。它们不必等整个GPU重构完成；但依赖A/Z的效果必须跟随公共surface契约，不能每种效果另补一个互不相容的近似。

## 可以继续搬到 GPU、或做得比当前 Vera 更好的部分

这些是由代码职责得出的工程候选，**没有实机耗时或提升百分比**。没有GPU运行数据时按可消除的CPU工作/拷贝和依赖排序，不承诺帧率。

| 顺序 | 当前事实 | 具体改进 | 判断依据与约束 |
| --- | --- | --- | --- |
| G1 | `radar_scene.h` 的静态底图仍由 `NativeCanvas` 在CPU合成；Hotfix21仅将动态原尺寸页搬到GPU | TMP、覆盖物、地表阴影和原生Z也在GPU脏页中构建；直接送面积缩图/视口 | Vera `tile_atlas.rs`、`terrain_instances.rs`、`terrain_draw.rs` 可参考；保留脏页、移除对象擦除、透明孔、越界留白和整帧回退语义 |
| G2 | `vulkan_scene.h:FrameResolve`、`vulkan_area.h:AreaResolve` 每次submit后仍 `vkWaitForFences`，生产预算100ms；已有12槽输出池并不等于异步计算队列 | 2–3个有界in-flight任务/command buffer，轮询完成，只发布最新有效相机；资源直到fence完成才回收 | 借鉴Vera `terrain_submission.rs` 的封闭pass后提交和资源生命周期；不要照搬Metal128pass/4096buffer限制。积压会增加延迟，必须丢弃尚未提交的旧请求 |
| G3 | **Vera `unit_atlas.rs::build_unit_atlas` 明确使用 `PreparedDraw::render_cpu`**；我们已经GPU栅格化，但 `gpu_voxel.h::prepare` 还在CPU逐面变换、投影、选VPL、展开三角形 | VXL原始几何/HVA/VPL常驻GPU，miss只上传姿态；按批次完成变换及多个姿态任务 | 这是现有GPU路径的深化；保留准确姿态/光照键、深度赢家及同深度原始次序；先看miss率，命中缓存时此项收益可能很小 |
| G4 | `xbrz_adapter.h::gpuState()` 是thread_local，各渲染worker自持GPU状态；CPU世界快照共享不保证GPU素材跨worker只上传一次 | 在同一适配器建立共享不可变素材/姿态资源服务，主画面、观察窗、小地图共享resident资源 | 引用计数、预算、设备丢失和异步fence必须统一；不要让一个小地图大任务阻塞主画面 |
| G5 | VXL已能延迟色盘查表，但大部分SHP/TMP仍先在CPU生成彩色像素 | 将更多SHP/TMP保持为索引/覆盖/Z，GPU选色盘、阵营色及现场光照 | Vera `sprite_source.wgsl`、`palette_light.wgsl`、`palette_textures.rs` 可直接对照；透明0、发光索引、素材自带alpha、xBRZ光照前判边不能丢 |
| G6 | `vulkan_scene.h` / `object_canvas.h` / `gpu_effect_plan.h` 仍在CPU建立空间bin/引用及顺序命令 | GPU可见性筛选、计数+前缀和+稳定散布，减少多视口反复建表 | 必须保留原draw序；简单atomic append或按贴图排序会破坏半透明/阴影。小场景CPU可能更划算，应保留阈值选择 |
| G7 | `local_ui_renderer.h`、`paintInteractions`仍把字形/线条展开为CPU矩形并光栅后上传RGBA | 原生字形SHP/font atlas常驻GPU，文字/路径/选框只传实例；按独立层失效 | Vera `bit_font.rs` 和presentation sprite实例是参考；内存读取/文字解码仍适合CPU。不是要求将每个字节解析都放到GPU |
| G8 | `vulkan_present.h::DirectOutput::record`仍buffer→shared image拷贝；成功直出已没有CPU回读 | 若同适配器导入资源允许STORAGE，计算结果直接写共享image | 需验证格式/usage/外部内存及keyed mutex兼容；不满足则保留当前路径，不能宣传已经“真正零拷贝” |

Vera并非所有地方都领先：它为复现原生像素保留很多CPU/x87生成步骤，我们可将可并行部分留在GPU；我们已有实际输出倍率的VXL几何采样、xBRZ、独立观察窗/小地图直出，不应为了“照抄”删除这些。但GPU算法必须继续保留原生顺序和材料含义，不能只用更多线程掩盖错误输入。

## 成组实施顺序

1. **显示完整**：本轮路径点 → tether重绘、特殊pips、范围圈；各自产生完整命令再进入统一绘制顺序。
2. **公共surface语义**：完整Z/A、格式和特殊读写规则 → 精确激光/辐射/波形/灯光。这里应该作为一个依赖明确的迁移批次。
3. **减少CPU瓶颈**：G1静态地形GPU化和G2有界异步提交优先，随后G3姿态准备/G4跨视图资源共享；继续保持CPU完整回退和当前相机的完整帧发布。
4. **进一步减小上传/调度**：G5索引素材、G6稳定bin、G7UI实例化；G8须先确认互操作资源能力。
5. **完全独立UI**：最后补全侧栏和合法性预览。它们与游戏状态深度绑定，不能以“世界已经全部GPU化”冒充完成。

本轮只实施路径点与该清单，不声称已执行表中所有后续工作。按用户要求没有跑CPU测试套件、GPU、Windows或游戏性能测试。
