# Hotfix15：独立效果、HVA、残骸与队形公共画布

## 问题与证据等级

补齐渲染升级遗漏的独立动画类型，并检查队形预览是否真正复用公共绘制结果、GPU 和画质设置。此前队形只复用解码器，仍有独立 GDI+ 像素循环和每轮四项额度；独立 VoxelAnim、HVA 后续帧及多种程序化效果也没有进入共享场景。

地址布局沿用两个历史受支持样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。本轮没有这两份 EXE 的新执行或真实游戏录像。新增证据是固定公开源码、完全合成的只读内存、生产解码器夹具和本地编译；不能称为原生逐像素、GPU 驱动或联机认证。

固定来源：[Vera20k 0b6a073](https://github.com/YuriPlanet/vera20k/tree/0b6a073d0928453036c4a53d117bb7a2d8d573a7) 的 `instances/particles.rs`、`instances/voxel_anims.rs`、`lasers.rs`、`radiation_beams.rs`、`combat_lights.rs`、`render/planning_lines.rs`、`src/render/laser.rs`、`wave_geometry.rs`、`radiation_beam.rs`、`src/sim/particles/spark.rs`、`tools/procedural_drawing_oracle/rad_beam.py`、`tools/control_group_text_oracle.md`，以及 [YRpp d979554](https://github.com/Phobos-developers/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2) 对应类声明。Vera 用作语义及坐标对照，许可与来源见 THIRD_PARTY_NOTICES；不捆绑其游戏资产或私有 EXE。

## 新增字段与结论

所有新世界效果都只复制当前值，不调用游戏 AI、计时、RNG、绘制方法或推进物理。未新增游戏侧 ABI 或 native entry。

| 类型 | 地址／字段 | 使用和限制 |
|---|---|---|
| HVA | 类型机身／炮塔 HVA 指针 `+B4/+BC`，HVA 帧数 `+8`；Foot `+538`、Techno `+148` | 保存所有 frame×section 矩阵，读取真实 body/turret 计数。相对捕获 frame0 变换，缩放／镜像／平移只应用一次；缓存键含两个动画帧。未伪造工具箱时钟 |
| VoxelAnim | 数组 `887388`，vt `7F6318`；id `+10`、Type `+104`、House `+10C`、Location `+9C`、TimeToDie `+110`、Invisible `+13C`；当前 Bounce quaternion `+E0`，Type.VoxelIndex `+298` | 独立 kind8；使用真实当前位置和翻滚，不预测 Bounce 下一步。原始 VXL 可以没有 HVA；按实际节选取、所属色和地面／桥面阴影。相同类型跨对象共享不可变模型，失败瞬态对象不撤销完整世界帧 |
| LaserDraw | 数组 `ABC878`；age `+0`、width `+1C`、house/support `+20/+21`、端点 `+24/+30`、RGB `+41/+44`、duration `+4C`、blink `+51`、fade `+52/+54/+58` | 普通／光棱／飞碟实际生成的段统一捕获；本地旧激光 stencil 在共享采样有效时退出。外晕随机偏移、RGB565 舍入与原生 A/Z 未复刻 |
| RadBeam | 数组 `B04A60`；draw `+9`、mode `+10`、spacing `+18`、RGB `+20`、端点 `+24/+30`、duration `+3C`、amplitude `+40`、progress `+7C` | 接入直线 Radiation0、Temporal1；Eruption2 是另一种几何记录，明确跳过，不能把未定义 target 当普通束线 |
| Wave | 数组 `A8EC38`；source/target `+B4/+C0`、四世界角点 `+FC`、phase `+130`、front/back `+138/+140`、direction `+1CC` | 接入 Sonic0／Magnetic3 的目标表面位移；以世界角点重投影供任意视图使用，不复用主屏幕缓存多边形。旧 Wave1/2、原生 A/Z 和位级正弦表仍有差异 |
| Spotlight | 数组 `AC1678`；位置 `+0`、size `+10`、disable flags `+14` | 尊重禁绘与通道位，径向辉光近似；并非 BuildingLight 所有额外束线 |
| AlphaShape | 数组 `88A0F0`；Attached `+24`、Rect `+28`、AlphaImage `+38`、gone `+3C` | 复制实际已加载 SHP 索引，以 index/128 调亮／暗已有表面，零透明；转换相对世界位置跨相机重投影。额外视口原点／第三方布局仍待原生对照 |

Particle BehavesLike2 用父 System `+124` 指针的 `+F4` 方向／阶段字节与 Type.EndStateAI `+309` 做方向分带，不凭空构造脱离父对象的阶段。BehavesLike3/4 进入真实 RGB 点路径；`+B0` RGB、`+D0` 插值累加、`+D8` 索引与 Type.ColorList `+2B8` 的对应来自公开 Spark 实现和 YRpp 未命名字段的映射，**尚无本轮原生执行证明**，因此有范围／有限值／列表边界保护。透明字节按有符号条件处理：128..255 不落入 25/50/75 的半透明分档。旧 1,024 粒子选帧和 11,552 Anim 结果是历史证据，不是此次新执行。

新增世界效果保存在同一不可变 Snapshot，由现有 SceneBank 共享给主／副相机／实景地图，遵守粒子／光束开关及目的地迷雾遮罩。程序化光束、波形、点、Alpha 和辉光目前在公共 CPU 或 GPU 场景结果之后用 CPU 后处理；没有把这些近似绘制冒充新的 GPU 内核。VXL/HVA 几何继续可走 GPU，CPU/GPU 姿态键一致。完整原生深度遮挡、RGB565 混合、外晕 RNG 和复杂屏幕效果仍未完成。

## 本地 UI 与公共队形预览

组号来自 Techno `+214`，选中己方才绘；veterancy 原值 `+150` 决定实际 PIPS14/15/19，使用 PIPS 指针 `AC147C` 和当前 MOUSE converter `87F6C8` 的颜色。维修沿原 WRENCH 状态选帧，从共享世界移到主视图本地 UI。primary／发电建筑文字读取 Type.PowerBonus `+EE4`、当前 House.PowerOutput／Drain `+53A4/+53A8` 和原 CSF `TXT_POWER_DRAIN2`。

Campaign 判断 `A8B238==0`，SW suspended `+70` 且已满时，使用 CustomChargeTime `+24` 或 Type.Recharge `+B0` 隐藏对应计时器。读取已有状态，不推进计时器。

新增公共地面解析：Cell signed level `+11B`、ramp `+11C`，高度 `89E7C0`、坡面系数 `81C900+(ramp-1)*40`、比例 `89E740`、桥高 `AC13BC`。集结点可为当前 Map 实际 Cell 或有效对象，建筑对象取基础中心。Foot 导航向量 `+588`、当前 destination `+5A4` 支持选中己方多段队列和原字体节点编号；4-on/4-off 虚线使用原墙钟相位。**这还不是完整 PlanMgr 多分支、循环、悬停图**；公开规划样本的 `+AC` 等字段可能是计数／缓存，不应凭名称猜为坐标。

队形仍由自己的假想位置布局器工作，但现在经 `renderSharedActor` 复用 NativeImageBank 不可变结果，经公共 ViewportCanvas 合成，GPU／精细度／颜色精度／色盘／缩放／光照 Profile 一并送入独立工作线程。GPU 延迟 VXL 包不在 UI 线程强制物化。取消每轮四个新模型／朝向额度，改为耗时预算和游标轮转；八个朝向是 LRU 缓存容量，不是可绘制单位名额。结果不变直接复用最终透明位图，UI 只贴一张图。

公共画布过去默认输出不透明世界颜色，透明预览暴露了分数覆盖被画实的问题。此次 CPU 与两份精度 GPU shader 均改为预乘 source-over alpha；不透明底图仍保持 alpha255。队形统一施加 124/255 预览透明度，分数边缘继续保留覆盖度。Profile／位置／光照／素材变更会更新最终合成，静止预览不重复重建。

## 验证与未决事项

新增／扩展 `assist/tests/world_effects.cpp`、`voxel_motion.cpp`、`formation_host.cpp`、`gpu_scene_record.cpp`、`native_local_ui.cpp`、`particle_visuals.cpp`。覆盖 600 激光、生命周期／禁扫／遮罩、Sonic/Magnetic、Alpha 索引、真实格式的合成 VXL/HVA 多帧和无 HVA 残骸、当前四元数／桥影、组号／PIPS／维修本地化、坡面／对象目标、Campaign full/hold，以及公共队形素材调用、同轮十二新模型、热位图复用、透明边缘。`game_assets_host_test.py` 编译实际生产 Context，不能只用 mock 证明集成。

最终构建、回归、shader 校验、ABI 与 PE 结果和输入摘要见交付包 Build-info.json。ASan/UBSan 检查只验证合成夹具内存安全；环境不支持 LeakSanitizer，不能声称检查泄漏。软件 Vulkan 即便通过，也不是物理 GPU 性能或 Windows 驱动证明。

拒绝把完整主屏幕截图直接用于其他相机；拒绝把本地组号／指令／维修 UI 混入共享世界；拒绝为 HVA 构造动画时钟；拒绝把 HVA 文件设为碎片的必要条件；拒绝调用原生 RNG／绘制／AI 补画。尚缺 Eruption2、旧 Wave1/2、复杂 SourceShared、BuildingLight 束线、所有全屏效果、完整 PlanMgr 图、Phobos 新聊天框及扩展 UI、悬停／禁用／断电准入细节和真实 Windows／游戏／GPU／联机／FPS 验收。
