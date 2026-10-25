# VERA20k 渲染源码参考：与 pre.9 五项修复的对应关系

## 来源、范围与约束

用户推荐 `YuriPlanet/vera20k`；已确认 fork 为 `ARISAKAHIKARI/vera20k`。本次通过 GitHub 插件只读固定提交 **`3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`**；读取时上游与 fork 的 main 指向同一提交。先读中文 README、docs 中文索引、目录树，再按函数读取相关源文件，没有克隆约 393 MB 的整个仓库。

仓库 [README 的许可证声明](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/README.zh-CN.md#L151) 与 [LICENSE-GPL](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/LICENSE-GPL) 标明 GPLv3。本次仅记录设计与核对入口，没有复制其 Rust/WGSL 实现进工具箱，也不切换 pre.9 的底层架构。

[docs/zh-CN/index.md](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/docs/zh-CN/index.md) 明确这是 Rust 重写引擎，sim 持有游戏状态，render 使用 wgpu；我们的现有工具箱仍读取外部原版状态并独立呈现，两者不能直接互换。下述原版地址来自其注释/fixture 元数据，属于上游定位线索；本次没有重跑 VERA 的 cargo/GPU/Unicorn 测试，没有确认其原始 PE 与本地支持映像的 SHA 一致，因此不能把这些引用记作本工具箱新增原生重放结果。

## 五条可执行结论

### 1. 原始尺寸统一合成，最后只缩放一次；源、目标和输入映射共用一份几何

[ScaledSurface](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/scaled_copy.rs#L29) 为中间表面持有独立颜色和深度目标；[UpscalePass/ZoomGeometry](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/upscale_pass.rs#L43) 在整个场景已画完之后呈现，并用同一几何提供源到窗口和窗口到源映射。

[scaled_copy.wgsl::fs_main](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/scaled_copy.wgsl#L33) 的整倍数放大保持完整像素块，分数放大按目标像素覆盖源像素的面积混合。这支持本轮小地图的“先按原始像素完成地块/覆盖图/物件/特效/Extras，再统一缩放”路线，避免逐块取整导致接缝。其 shader 明确假设 scale>=1、每轴最多跨两个源像素，**不能作为大倍率缩小的完整滤波器**；我们的缩小路径仍需覆盖全部源 footprint。

行动：pre.9 保留分页 native canvas 与一次 area-filter 呈现；精确 source span、整型 backing 和导航反变换由同一几何描述。将来需要 GPU 时沿用这个边界，把最终呈现/离屏合成迁入 GPU，不同时重写读取和游戏逻辑。

### 2. 深度测试、深度写入和目标相关阴影必须按绘图家族区分

[RenderZPolicy/BlitPolicy](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/tactical_draw_plan.rs#L17) 分开无 Z、只读 Z、读写 Z 和带 alpha 的读写 Z；[native_z.rs](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/native_z.rs#L311) 将原版行 seed/gradient 与兼容 float 排序轴分别保留。

[terrain_edit.wgsl](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/terrain_edit.wgsl#L14) 在有符号 candidate 与已有 u16 深度比较之后才执行相应写入；Terrain 阴影与 Bullet 阴影不是同一 Z 策略。[只读目标合成](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/terrain_read_only_resolve.wgsl#L12) 保留 body 和后续阴影的次序，并对目标 RGB565 执行阴影操作。

行动：本轮 Bullet/Extras 不共用一个“全部写 Z”的通用材质；尤其不能把“让所有阴影写 Z”当作普遍防叠黑方案。后续 GPU API 应携带经本地原版验证的候选深度、梯度、读写策略和源编码，不能假定当前 float depth 已逐位等同原版 u16。先保持现有独立原生证据和回归，再逐家族扩展。

### 3. 物理地图、玩法范围、原生雷达表面和相机范围保持不同职责

[playfield.rs](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/map/playfield.rs#L98) 分开 mode-zero、带 signed level/slope 的 mode-one，以及 [Size 菱形](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/map/playfield.rs#L155)。[NativeRadarSurfaceGeometry::from_playfield](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/radar/geometry.rs#L50) 特别说明原生雷达尺寸计算采用 mode-one-valid cells；这是雷达表面重建规则，不是 Tactical 所有地形绘图的准入规则。

行动：保持本轮 `CoordinatesLegal` 与 usable/camera 分离；不将 VERA 雷达生成器中的 playfield 过滤直接搬来裁我们的实景地表。signed Level 和精确 native viewport 的本地修复继续保留，真实不齐外缘通过一次呈现滤波处理，不补画非法格、不按最浅边缘统一裁其他列内容。

### 4. 驻军民房帧与维修 Extras 是两个独立层面的状态

[occupied_body_frame](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/sim/building_art.rs#L874) 把是否有 occupants、当前健康、TechLevel 和黄/红阈值作为显式输入；正常/驻军基帧和受损分支独立，另保留 TechLevel=-1 的特例。[body_frame fixture 元数据](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/tools/garrison_oracle/body_frame.meta.json) 自述覆盖 54 个 completed CanBeOccupied 案例，并明确不是全输入或 GPU 像素证明。

维修扳手由 [techno_extras_for_parent](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/app/presentation/ui_overlays.rs#L1061) 在父对象 Extras 阶段产生，其修理状态判定先于选择/悬停的后续状态条门禁；它单独计算建筑 render 坐标，使用 [WRENCH.SHP 与 MOUSEPAL.PAL](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/selection_overlay.rs#L406)，[repair_wrench_frame](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/app/presentation/ui_overlays.rs#L835) 按游戏速度归一化周期选择 0..6 帧。[object_extras.rs](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/app/presentation/render/object_extras.rs#L14) 保持时钟、扳手、军衔、状态条等的父对象顺序。

行动：驻军帧进入姿态缓存 key，并按真实健康阈值更新；维修扳手作为独立 Extras 图像/色盘/帧序和绘图顺序处理，不能只扫描 AnimClass 或把它粘进建筑 SHP 主体缓存。本轮两项仍以我们自己的目标映像重放为验收来源。

### 5. Bullet 保留自己的生命周期、图像、色盘和桥层语义

[build_projectile_visual_instances/projectile_draw_instance_sampled](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/app/presentation/instances/projectiles.rs#L199) 首先要求 retained Display 成员身份；Inviso 和仍等待相关动画的 Bullet 不画。SHP 的方向/帧、桥层/地面阴影和 VXL 分支分别处理。VXL 分支早于 SHP 帧/阴影/色盘路径返回，不能直接复用 Unit 的完整选型。

[projectile_key](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/sprite_atlas.rs#L374) 明确 Bullet 的 AnimPalette 优先级高于 FirersPalette；后者使用 Bullet 保留的 scheme，而非发射单位当前 owner。其 [bridge_render fixture 元数据](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/tools/projectile_oracle/bridge_render.meta.json) 区分真实 draw 参数重放、绘图边界替身、原始输入和未覆盖的完整 GPU/游戏行为。

行动：本轮 Bullet Visuals 继续独立采集与身份校验；不要因为遍历得到 BulletClass 指针就默认可画，不用单位 palette/朝向/阴影字段代替。对 Inviso、等待动画、SHP/VXL、桥上/桥下、独立 scheme、同帧删除和地址复用分别覆盖；保留物件逻辑和插值呈现的职责分离。

## 本次完成与未完成

完成了固定提交、许可证、文档索引、相关生产函数及部分上游 fixture 元数据的只读核对。未下载或引入其源码到工具箱生产目录，未运行其构建/测试，未宣称上游实现天然等同本工具箱目标映像。本条作为可回查的设计/地址线索，pre.9 现行读取、native canvas、相机限位和验证流程继续使用各自独立实现。
