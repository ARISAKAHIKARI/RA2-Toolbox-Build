# Vera20K 绘制迁移总表 · Hotfix21

用户要求先系统梳理，再连续完成一轮集成。起点为 Hotfix20，远端 `9d38179b0ba2140cd8bfa6f4b691ab482615209f`；固定上游 `ARISAKAHIKARI/vera20k@3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`，GPL-3.0。上游 `src/render` 和 `src/app/presentation` 共175个非测试源文件（不计嵌入表二进制、字体数据）。下表按实际生产职责组织，文件数不等于已移植数。

## 现状、依赖和本轮工作

| 领域 | 上游主要文件 | 已有路径 | 本轮接入/核对工作 |
| --- | --- | --- | --- |
| 素材与图集 | `sprite_source.wgsl`、`unit_atlas.rs`、`sprite_atlas.rs`、`atlas_growth.rs` | SHP/TMP/VXL原始素材、不可变上传图集、xBRZ | VXL姿态/覆盖与现场色盘解耦；相同几何不因换色重复GPU光栅；同一素材输入的存储及上传复用 |
| 姿态与阴影缓存 | `unit_pose_cache.rs`、`unit_shadow_cache.rs`、`vxl_shadow.rs` | HVA多帧、Drive/Ship坡面、阴影矩阵 | 保留精确姿态键、缓存寿命和CPU回退；按字节计入新增材料存储，不降朝向精度 |
| 静态地形、矿石、树木 | `tile_atlas.rs`、`overlay_atlas.rs`、`instances/overlays.rs` | 全图稀疏页、变化矿石局部重绘、树正文/阴影 | 与动态物体GPU页合成连接，保留原尺寸Z/阴影后再缩图，不把小物件先缩成点 |
| 建筑、附属动画 | `instances/shp.rs`、`tactical_draw_plan.rs` | Bib/门/正文/造卖、槽动画、深度形状 | 保留每个对象及其固有部件顺序；槽Anim保留自己的Display语义，不能一概塞到建筑后面 |
| 单位、弹体、粒子、碎片 | `instances/{units,projectiles,particles,voxel_anims}.rs` | 四类对象与现有采矿SHP共享世界绘制 | 统一使用延后色盘、明确深度/混合策略和遮挡裁减；保留缺失素材的整帧回退 |
| Z与像素混合 | `terrain_read_only*.wgsl`、`terrain_packed.wgsl`、`native_packed.wgsl` | 每像素有序GPU循环、CPU同源回退 | 只读Z连续段从最后可见不透明正文开始合成；加入原生565半阴影/25%、50%、75%混合，RGB高精度档保留自己的采样策略 |
| 特效依赖与提交 | `terrain_batch.rs`、`object_stages.rs`、`terrain_submission.rs` | 激光、电弧、辐射、Alpha、波形批次 | 波形读取足迹纳入依赖；互不干涉的波形共用快照；只复制需要读取的行区间，保持重叠顺序 |
| 黑幕、裁剪 | `shroud_buffer.rs`、`tactical_a.wgsl`、`draw_passes.rs` | 当前相机读取、原生SHP遮罩、战场裁剪 | 新路径使用同一帧遮罩；世界边界、UI边界和缺失页不可互相填充 |
| 本地状态UI | `ui_overlays.rs`、`selection_overlay.rs` | 原生字体、军衔、扳手、弹药、整数载员格 | 补齐可只读取得的载员/资源格输入，保留逐对象身份及帧一致性；UI失败不应让已完成世界帧消失 |
| 放置与侧栏 | `placement_preview.rs`、`sidebar_surface.rs`、`sidebar_render.rs` | 原生放置判定透视孔；侧栏独立于战场覆盖 | 核对独立合成/裁剪责任。上游含Phobos自定义预览和自己的完整模拟，不能把Cell membership当可放置结果，也不能伪造当前队列/鼠标状态 |
| 主画面/观察窗 | `render/{build_instances,draw_passes,merge_passes}.rs` | 常驻世界输入、当前相机整帧、观察窗共用世界 | 对完整实际命令比较复用，纳入材料与特效身份；相机/颜色/遮罩变化时失效，不重复旧画面填边 |
| 实景小地图 | `render/radar/{minimap,terrain_updates}.rs`、`terrain_draw.rs` | 完整GPU缩图与呈现，但动态原尺寸合成仍在CPU | 把动态页的颜色/Z/阴影合成接到GPU，变化页重建面积积分；主、副屏继续消费同一个完整GPU帧 |

## 执行顺序与交付口径

先建立共享材料/绘制命令，再接颜色混合与遮挡、动态页、特效批次，最后接UI及视口复用。所有本轮条目完成后统一交叉编译、离线shader检查、提交和打包，不运行CPU测试套件、Vulkan、Windows loader或游戏。最终结果逐项记录实际进入的生产调用点，不能把阅读、可行性分析或兼容回退写成已经迁入。

Vera自身也保留原生未覆盖与Phobos扩展边界。完整Rust模拟器、侧栏输入状态机和本工具箱的外部只读宿主不是可互换接口；这些差异必须列明，不能靠猜测游戏状态补全。现有游戏内ABI、Hook、写入、逻辑时钟保持原有契约，本轮不新增游戏函数调用。

## 本轮落实结果

| 领域 | 实际接入位置与结果 |
| --- | --- |
| 素材/图集 | `camera_assets::gpuVoxel` 把已支持VPL索引的姿态覆盖与现场256色表分开；`DrawJob::sourcePalette` 延后查表；原来的diffuse回退不冒充索引材质。 |
| 姿态/阴影缓存 | 姿态缓存保持精确矩阵、角度、HVA和VPL版本；阴影用中性色覆盖；`gpuvoxel::materialize` 保留同一色表用于CPU回退；新增色表/原色缓冲进入材料内存计数。 |
| 地形/矿石/树木 | 静态NativeCanvas页提供颜色、版本与深度；GPU动态合成先消费这些深度，再构建面积积分。原有地形、矿石局部更新与树影读取保持接通。 |
| 建筑/附属动画 | 复核现有 `captureBuildingParts`、`GpuSceneRecord::flatten`、世界 `orderKey`；Bib/门/正文/阴影保持相对顺序，槽Anim依自身Display排序，不增加错误的“全部移到父对象后面”。 |
| 单位/弹体/粒子/碎片 | 现有四类对象进入统一DrawJob；同一套延后色盘、nativeBlend、Z只读段同时服务主画面、观察窗与动态小地图页。 |
| Z/混合 | `gpu_draw_encoding.h` 和同源 `vulkan_scene.comp` 编码连续只读Z区间；从最后被接纳的不透明像素起回放其后所有操作。`native_packed_color.h` 为CPU回退提供同一565半阴影及25/50/75混合。 |
| 特效提交 | `gpueffects::scheduleWaves` 用32像素格记录写区及最大位移读区依赖；独立波形共用快照，重叠波形保序，普通绘制/电弧为边界。主场景和AreaResolve仅复制快照读区所需行段；连续素材上传也合并copy区间。 |
| 黑幕/裁剪 | 原生动态页只写合法世界坐标；面积缩图后应用当前遮罩。主场景仍以当前完整相机绘制，原生放置透视孔仅在相机精确一致时存在。 |
| 本地UI | `native_unit_ui_stencil.h` 支持正有限非整数Size与正确Aircraft填充规则；新增选中己方Vehicle/Aircraft/Infantry的Tiberium格（完整整数storage输入），资源计数用PC53/chop专用数值核；`local_ui_renderer/task` 各生产层独立提交，失败清除该层。 |
| 放置/侧栏 | 核对 `main_viewport::sample`、`render_upgrade_host::update`、`render_feedback_host`：世界覆盖范围取Tactical矩形；侧栏由游戏的独立表面保留；放置判定不以Cell membership或Phobos ghost替换。此项是接口边界核对，不是新的完整侧栏移植。 |
| 主画面/观察窗 | `scene_frame_reuse.h` 比较实际特效命令/批次/素材及所有DrawJob参数；`world_scene_pipeline.h` 和SceneRenderer已接入。相机、色盘、材料、光照、黑幕或特效变化都会失效。 |
| 实景小地图 | `ObjectCanvas::planGPU → AreaResolve → gpuarea::ObjectStage → SCENE_PAGES/SCENE_BANKED → 面积积分/缩图/遮罩/特效/呈现`。动态物体不再要求CPU先栅格全尺寸页面；GPU失败才调用完整CPU compose。物体移动/移除、页面变化及地图尺寸变化会重建相应页。 |

这12项按职责收口，其中既有模块的复核和新代码接入已明确分开。更细的证据及尚未覆盖的上游能力见 [Hotfix21实施记录](2026-10-30-vera-render-integration-hotfix21.md)。
