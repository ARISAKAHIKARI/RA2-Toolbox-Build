# Vera20K 第二轮绘制审计：11 项额外缺口

## 范围与证据

问题：用户在首轮清单后继续问“还有呢，还有什么可以抄的”。本轮继续读取实际生产代码和调用点，补充独立绘制来源及已有领域的具体遗漏，不把首轮的路径点、tether、pips、范围圈、通用 Z/A、激光/辐射/波形、灯光、混合炮塔、侧栏、放置预览、555 格式或 G1–G8 再算一遍。

对照：本仓库 Hotfix22，远端 `c7b052b2f5aee28bafafd5bd962b0de30db1ad6f`；[Vera20K 固定提交 `3d42426`](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)。来源为 YuriPlanet 及贡献者的 GPL-3.0 代码。逐文件 blob、对照文件和检查范围见 [证据清单](evidence/vera-render-additional-gaps.json)。

调查前查询了研究索引、Hotfix15/18/21/22 和首轮路线，以及同一持久 SQLite v97。新增项又按 `FoggedObject`、`LineTrail`、`smudge`、`sinking`、`Rubble=`、`ChronoScreen` 检索；旧维修记录只涉及沉船时不画扳手，不能代替沉船水线。

历史支持的可执行样本 SHA-256 仍为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 和 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。**本轮是固定源码审计，没有对任一 EXE 新反汇编、执行或实机观察。** 下列地址是上游源码中的逆向标注，未据此新增游戏调用、字段读取或 ABI 适配；引用上游已有 oracle 路径不等于本轮运行了该 oracle。

## 新增缺口

| ID / 优先级 | Vera 已有的生产行为 | Hotfix22 的具体缺口 | 迁移工作与 GPU 方向 |
| --- | --- | --- | --- |
| E1 / P1 | `instances/fogged.rs` 独立绘制 FoggedObject 的保存记录：建筑正文、阴影、地基和已保存动画；不依赖原建筑仍然存在 | `radar_map.h::snapshot` 和 `watch_view.h::sample/refresh` 采集活对象和当前姿态；未找到 FoggedObject 记录入口。GPU 迷雾遮罩与普通世界快照不能提供这类“最后看到的建筑”语义 | 读取原生保存图的可见位、类型/帧、阵营和坐标，独立放入原有 draw plan。实例和素材可常驻 GPU；重新见到、切阵营、读档时按记录生命周期更新。不能继续用迷雾后活对象的最新姿态更新保存图 |
| E2 / P1 | `draw_state.rs` 统一 SHP/VXL 隐形阶段、观测阵营/侦测、伪装外形/闪现，以及 warp 选择位 | `radar_map.h:167` 与 `watch_view.h::valid` 对 `+0x220 != 0` 直接排除；`visual_pose.h::Pose` 没有对应材质/伪装字段。不是只差一个透明度 | 先建立只读观察者上下文与最终显示类型，再统一正文/附件/阴影材质。简单透明选择走 GPU；VXL 折射/位移依赖首轮 Z/A 和 word-offset 工作。不能仅放开 cloak 过滤而泄露本应隐藏的对象 |
| E3 / P2 | `sinking.rs` 保存沉船水线，首次捕获仍完整绘制，之后按世界行裁掉水下像素 | 只有 `repair_visuals.h` 读取 sinking 来隐藏扳手；姿态、图像和 GPU 实例中没有水线裁剪字段 | 取原生有效水线，或在原绘制不再推进该字段时采用明确归属的呈现历史；按会话/对象身份维护。GPU 每实例一个裁剪行；水线来自合成光栅边界，不能用图集分配框或屏幕行代替 |
| E4 / P1 | `smudge.rs` 绘制独立弹坑/焦痕层，地形之后、对象之前，采用 Cell 材质且不读写 Z | `CellArt`、watch `Tile` 与地形绘制循环只持有 TMP/overlay 等，没有 Smudge 类型/帧与独立层 | 增加只读地表污迹采样和素材入口，形成 GPU 脏页/实例；按完整 SHP 边界裁剪。改变/清除时使旧覆盖区域失效。多格污迹不能直接套用上游尚未全覆盖的去重假设 |
| E5 / P1 | `presentation/line_trails.rs` 保留 32 槽弹道历史，弹体消失后继续淡出；`render/line_trail.rs` 按顺序读取现有颜色/Z，不写 Z | `projectile_visuals.h` 只捕获弹体姿态；`worldfx::Snapshot` 没有 LineTrail 记录。普通粒子、激光不能补上这一入口 | 优先复制原生历史环及颜色/强度，世界坐标重投影，按反向登记序生成 GPU 命令；重叠像素必须保序。不能用外部高刷频率推进衰减，也不能简单连上一帧屏幕位置 |
| E6 / P2 | `beacon_art.rs` 在主画面画 PBEACON，在雷达画 RDRBEACN，按阵营 Convert、选中帧组和原生闪烁周期绘制 | 有清理信标命令，但未找到信标绘制采样、素材或独立图层；命令功能不等于显示覆盖 | 共享信标只读记录，主画面/雷达各自投影并使用对应 SHP；雷达每四段周期只亮一段。Vera 雷达端 CPU 逐像素 blit 可改为 GPU 索引精灵实例 |
| E7 / P2 | `radar/events.rs` 与 `radar/minimap.rs` 在对象点之后、信标之前绘制事件轮廓，保留旋转、收缩和退场时序 | 当前地图/小地图帧包含地形、对象、世界效果和迷雾，没有原生 RadarEvent 绘制记录 | 读取现有客户端事件状态，GPU 画少量有序线段/轮廓；无需 CPU 修改整张地图纹理。17 类事件共有状态机，但只有带颜色的子集画轮廓，不能声称 17 类都可见；不迁移 EVA 或 Space 按键行为 |
| E8 / P1 | `instances/bridges.rs`、`bridge_atlas.rs` 有独立 Cell overlay 阴影扫掠、高桥 body 分派、健康桥 Latin-square 选帧和方向阴影偏移 | 已有桥面高度、照明和 Y 偏移；但 `camera_assets.h::overlay(..., terrain=false)` 只解 body，`terrainsprite::decode` 第二半阴影仅用于 TerrainClass。未接通通用 overlay 阴影层和高桥专用选帧 | 补全独立阴影/桥身命令，按 live 数字 ID 和状态选帧，不仅按素材名判断。墙、岩石、箱子按各自原生入口处理，不给所有 overlay 无条件附半帧阴影。GPU 复用模板与深度合成 |
| E9 / P2 | `building_rubble.rs` 与 `instances/overlays.rs` 按 Cell 保存的建筑类型选 LeaveRubble 图与阴影 | `CellArt` 未保存 rubble building type，overlay 素材只按 overlay type 解码；普通爆炸/碎片动画不能代替留下的废墟 | 捕获原生废墟关联类型，正文/阴影独立取图并更新脏页。零售 own-image 分支可迁移；自定义 `Rubble=` 是上游明确缺口，我们可按真实指针补齐其帧和阴影规则 |
| E10 / P3 | `pixel_fx_sparkles.rs` 逐帧为水面/矿石生成单像素闪光，门控含视野、占位与桥面 | 现有 RGB Particle 点不包含这种 Cell PixelFX 生产者 | GPU 按可见格与时间生成位置/颜色，避免 CPU 每帧生成和上传点表。若追求原生相位，应读取原生状态；Vera 当前采用无状态 hash/周期桶近似，不能称为原 RNG 序列复现 |
| E11 / P2 | `chrono_screen.rs/.wgsl` 与 `app/chrono_screen.rs` 保留场景表面，执行表驱动旋转、RGB565 变白和恢复；边界位于战术画面后、chrome 前 | 世界效果中未找到这一整屏呈现入口；单位 warp、普通 Wave 并不覆盖它 | 迁移已有 GPU 表面保留与后处理，读取原生 Action127/屏幕效果阶段；尺寸改变和退出清理。Vera 这里已经用 GPU，收益是补功能；不能将其叫作“我们首次 GPU 化”，也不应推断每次普通超时空传送都触发整屏效果 |

E8 是已支持桥梁/地形领域里的具体补全；E2 也依赖首轮通用材质工作。把它们单列是为了标明缺少的生产者和状态，不重复计算“完整 Z/A”已完成或有新的加速成果。

## 深度细化：已有首轮项目的明确子任务

`overlay_zshape.rs` 对坡面矿石/legacy 0x7E 使用 `SLOP01Z..04Z` 原始文件字节表面，配合源/目标联合裁剪与原始数据偏移；不是将 Z-SHP 当普通 RLE 图解码。我们现有 `overlayDepth` 仍是 flat/wall/rock 简化策略，已加载的 BUILDNGZ 也不能代替这四张表。此项归入首轮 Z/A，不新增第 12 个独立大项。

Vera 将超出原文件分配的读取定义为零，不能照搬原生任意堆内存读取。我们可把有界原始深度表常驻 GPU，同时保留其安全边界。

## 地址、字段与现有上游证据入口

这些仅用于后续定位；当前没有新增可调用 ABI，也没有宣称已验证本地两种 EXE 的所有布局。

| 项目 | 上游标注 | 相关证据/代码 |
| --- | --- | --- |
| FoggedObject | constructor `0x4D0EF0`，DrawAll `0x4D1890`，排序 `0x4D2ED0` | `instances/fogged.rs` 的保存帧/影/地基/动画顺序和源码测试；未据函数名猜测数组或结构偏移 |
| 隐形/伪装/warp | Techno DrawVoxel `0x706640`，TeleportLocomotion `0x7192F0`，伪装 flags `0x70ED80`；blink `+0x1EC/+0x1F4`，不是 redisguise 的 `+0x1E0/+0x1E8` | `draw_state.rs`；`tools/procedural_drawing_oracle/translucent_blitter_a.md`（代码所引，未运行） |
| 沉船 | Techno `+0x3CA` 有符号保存行，Unit `0x73BEA4..0x73BF7B`；现有扳手只读 `+0x3CD` | `sinking.rs`；`tools/spatial_oracle/naval_sinking_clip.py/.json`（代码所引） |
| Smudge | Cell `0x4804B4` → Smudge `0x6B55F0`，Cell intensity `+0x10C` / Convert `+0x34`，flags `0xE00` | `smudge.rs`；`tools/spatial_oracle/smudge_render`（代码所引） |
| LineTrail | 分配 `0x556A20`，detach `0x556B30`，更新/绘制 `0x556D40`，draw `0x556C00` / Surface `0x4BEAC0`；Object backlink `+0xA8` | `presentation/line_trails.rs`；`tools/projectile_oracle/line_trail.{py,json,md}`（代码所引） |
| 信标 | Init `0x4309D0`，主画面 `0x430250`，雷达 `0x430650`；SHP globals `0x89C474/0x89C478`；manager `0x89C3B0` | `beacon_art.rs`、`radar/minimap.rs`；已有 `ClearBeacons` 的 `0x431410` 是命令，不是绘制 |
| 雷达事件 | create `0x65FA70`，tick `0x65FE00`，draw `0x660050`，配置表 `0x7F0998`；event timer `+0x24/+0x30` | `radar/events.rs`，原生顺序归属 `radar/minimap.rs` |
| overlay/桥阴影 | body `0x47F6A0`，shadow `0x47F510`，shadow rect `0x47FDE0`；Cell ID `+0x44`、data `+0x11E`、flags `+0x140` bit0x80 | `instances/bridges.rs`、`bridge_atlas.rs`；`tools/spatial_oracle/bridge_{body,shadow}_render.json`（代码所引） |
| 废墟 | BuildingType body `0x45F160` / shadow `0x45F1D0`，LeaveRubble `+0x1768`，独立 Rubble SHP `+0x14F4` | `building_rubble.rs`；own-image body3、shadow count/2+3；独立 Rubble body0 的原生 shadow selector 返回 false，上游尚未接这一素材分支 |
| PixelFX | water table `0x8367C8`，ore table `0x8367F0`，Cell land `+0xEC` / gate `0x6D7A6C` | `pixel_fx_sparkles.rs`；当前 hash/2500ms 桶为呈现近似 |
| ChronoScreen | draw `0x53BBA0/0x53C0A0`，控制 `0x53B460/0x53B560`，Composite `0x88731C` | `render/chrono_screen.rs`、`app/chrono_screen.rs`；`tools/chrono_screen_effect.json`（代码所引） |
| 坡面 Z | 初始化 `0x54530E..0x545458`，可选表 globals `0xAA1060..0xAA106C` | `overlay_zshape.rs`，原始 encoded-data 表面与有界零填充 |

## 可比 Vera 做得更好的具体部分

1. **雷达事件和信标**：Vera 的最终雷达组合仍在 CPU 像素数组画轮廓/精灵并上传纹理；我们已拥有 GPU 小地图直出，可以上传少量状态/实例，在 GPU 分层合成，保留对象点 → 事件 → 信标顺序。
2. **Cell PixelFX**：Vera 的逐格 CPU 实例生成可转到 GPU；观测过滤和原生时间仍由只读快照提供。若采用原生状态，显示真实性也可能优于其 hash 近似，但布局尚需落实。
3. **污迹/废墟/overlay 阴影**：接入时直接复用原有脏页和共享素材，接上首轮静态地形 GPU 构建；消失的覆盖范围也须失效，不能只重画新增部分。
4. **完整性而非算力**：补齐上游多格污迹局部清除/异材质与自定义 `Rubble=`，比机械复制其尚未完成的分支更有价值。是否有效仍需后续实现证据，不预报帧率。

沉船裁剪与 ChronoScreen 在 Vera 已经是 GPU 消费；LineTrail 的重叠像素和原生目标读取有顺序依赖，不能一段一个无序原子写来宣称等价。CPU 继续负责有界内存快照、身份/状态验证和必要解码。

## 实施分组与明确未决

- 第一组补场景状态：E1 保存图、E2 显示材质/伪装、E4 地表污迹、E8 覆盖物阴影/桥身、E9 废墟；统一状态入口、实际覆盖范围和失效条件。
- 第二组补呈现历史与表面操作：E3 水线、E5 尾迹、E11 整屏效果，接首轮 Z/A 契约。每种历史明确原生或外部宿主的唯一所有者，避免高刷视图重复推进。
- 第三组补反馈细节：E6 信标、E7 雷达事件、E10 闪光，直接走现有 GPU 主/副视图。

不能直接当作成熟原生等价实现的上游内容：Smudge 去重只在完整、平坦且同材质的选定 footprint 得到说明；自定义 `Rubble=` 明确未接；PixelFX 的随机相位是近似；ChronoScreen 的 f32 与原生 x87 中间值有舍入差异，resize/load 恢复策略也非原生等价证明。`draw_state.rs` 的 `FX_EMP`、`FX_MIRROR` 只保留残余位，没有独立、已证明的原生材质，不把名字列为“Vera 已实现的 EMP/镜像效果”。

本轮只新增审计文档、索引与持久研究记录。未修改生产代码、未发布新热修包；没有编译、CPU 夹具、GPU、Windows 或游戏测试。静态对照能确认入口和状态缺口，不能确认实际画面、性能或任意 MOD 的兼容性。
