# Hotfix18：场景耗时、Vera20K 批绘与 GPU 直出

## 问题、基线与证据

用户反馈“场景”14.2 ms、新帧15 ms、逻辑状态间隔11.6 ms，快速拖屏露出原生画面，小地图矿石闪烁，树影缺失；要求参考 Vera20K、GPU 直接输出，并明确不要运行 Vulkan 测试。

基线是远端 Hotfix17 `98aaf4f8e0a432725dd7edeea58ae093b21ae724`，已合并 `ddf0002f795524fdc80760a0c8ae244746d4d52f`。本地等价基线 `367de0341c81367e1c73fdeb862022c7ff62e504`；CPU 阶段另留远端 `7593a400689f8f266295d36122f7f87c8a39ebea`（本地 `f5e1360`）恢复点。没有修改游戏线程的执行频率，也没有等待原生窗口绘制完成才采样。

本次证据为固定公开源码、合成内存／素材、CPU 行为夹具、静态 shader 编译及 Windows 交叉编译。**没有创建 Vulkan 设备，没有运行 Vulkan、D3D11、Windows loader 或游戏，没有实测显卡 FPS／输入到显示延迟。** 1000 Hz 是独立采样目标；“逻辑帧变化间隔”反映游戏状态变化，不是采样任务每次耗时，不能由该数值推断单位固定60 Hz。

## 固定参考及采用边界

Vera20K，YuriPlanet 及贡献者，GPL v3，固定 [3d42426](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)：

| 上游文件 | 本项目采用方式 |
| --- | --- |
| `render/unit_atlas.rs`、`sprite_atlas.rs`、`atlas_growth.rs` | 相同模型／姿态／源图复用，不变素材不重传，新增素材仅上传变化部分；保留本项目256细朝向及缓存预算，未强制降为32朝向 |
| `render/overlay_atlas.rs`、`app/presentation/instances/overlays.rs`、`render/native_z.rs` | Terrain 正文及第二半阴影，空帧保持为空，静态／动画 Z 规则，矿石 flat 和偏移规则；适配现有正向 scene-Z 约定 |
| `render/terrain_surface_lines.rs/.wgsl`、`wave_geometry.rs` | 按顺序 GPU 目标颜色批次；波纹在批次间读取已完成颜色快照，独立操作合并；本项目以图元／32像素 CSR 桶替代逐像素命令上传 |
| `render/electric_bolt.rs` | 参考“几何准备与目标混合分离”；保留现有只读电弧快照及确定性展示抖动，不调用或消耗游戏 Main RNG |
| `render/gpu.rs`、呈现 render 模块 | 正常帧提交 GPU 后呈现，不为常规显示回读 CPU；Windows 外部透明覆盖由本项目另行适配 |

未采用上游可选的“一逻辑帧延后插值”。上游 `gpu.rs` 使用 FIFO、desired maximum frame latency 2，不能说它使用 Mailbox 或天然没有队列。cnc-ddraw `279a057ee7e1e4d584b56141c014ab01f3c6ee6d` 的 `render_d3d9.c` 复用主表面／脏上传，其输入已经是原生绘好的表面；本项目从外部状态重建世界，工作量不同。此次不复制 cnc-ddraw 源码。

## 已定位并修复的耗时

- 全图相同地形快照的验证仍按4096格分轮，能够不断推迟完成帧。夹具在旧版失败，修改后连续6次均立即发布；5次矿石改变只重画403格，368640个输出像素与完整重画一致。
- 矿石局部更新检查包含地图几何外无效格，可能把缓存持续判脏；改为实际有效地形域，重画范围使用实际覆盖图像边界。
- 每格反复读取同一 tint converter。每轮缓存当前转换器，4096格夹具读取从32768次降为16388次；下一轮仍观察原生色盘变化。
- VXL 每个面／顶点重复计算姿态三角函数、pivot，并重复投影边界。现在每姿态计算变换、每面投影一次。256姿态的图像／深度／索引／GPU 三角形摘要前后均为 `ae556d7b6095a5d7`（694336像素，42405三角形）。5440面模型、64新姿态、5次中位数：153.707 → 38.207 ms。**这是 CPU 合成负载的准备耗时，不能解释为游戏帧率提升4倍。**
- NativeImageBank 淘汰由每次扫描全表改为链表 LRU；图集脏判断由每 draw 多次线性查找改为直接标记。
- 经过本轮状态／光照／色盘校验后，相同完整 GPU 绘制计划复用不可变结果；位置、顺序、图像、深度、迷雾或色盘改变即失效，不将旧逻辑状态当作新采样。

## GPU 输出和特效

`vulkan_present.h` 在 Vulkan 1.1、Win32 external memory／keyed mutex 支持且 D3D11 LUID 相同的条件下，将 GPU 输出 buffer 复制到共享 BGRA8 纹理。发布帧持有资源，最多6个槽；被消费者持有的帧不会被覆写，未展示的旧帧也可回收。跨 API 纹理在外部使用时转为 `GENERAL`，配对 keyed mutex：生产0→1、消费1→0；CPU AcquireSync 为零超时。

`gpu_compositor.h` 使用工具箱自己的透明、鼠标穿透窗口，D3D11 GPU 合成世界和 UI 源纹理，DirectComposition 呈现，最多一帧排队、`Present(0, DO_NOT_WAIT)`；未接受的呈现不计为成功。成功直出不分配／映射整帧回读，状态栏明确显示“回读：0（GPU 直出）”。仍有 GPU→GPU 复制、已有的有界 Vulkan fence 等待和 DWM 显示调度，不称为零延迟。

激光、辐射线、点、辉光、Alpha 和电弧颜色混合移入 GPU；波纹逐批快照避免并发读写同一目标。电弧先在 GPU 合成透明层，再乘一次迷雾；CPU 参考的波纹遍历方向按上游2..5正向，其余反向修正。几何计划最多65536条、2097152桶引用，超限整帧回到 CPU，不展示部分效果。是否跳过 CPU 后处理根据该视口本次实际 GPU 完成状态判断，不能复用同线程别的相机的 `gpuUsed` 标志。

短暂超出预绘制边界时，合成器沿用已完成场景并延展边缘到视口裁切区；正确覆盖部分仍按原摄像机偏移。新帧到达即替换。它是减少原生条带露出的连续性处理，边缘短暂重复不是新地形预测；菜单、对局切换和过期拒绝仍有效。Hotfix17 的退出2秒自身进程截止保留，恢复失败不无限等待。

## Terrain 字段与原生依据

沿用既有受支持 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。本轮未重新执行／反汇编这两份 EXE；以下是既有布局和公开源码适配，不是新的原函数认证。

Terrain 数组 `0xA8E988`、vtable `0x7F522C`，对象 `+0xC8` 类型、`+0x6C` health、`+0xAC` 动画帧、`+0x9C/A0/A4` 坐标；TerrainType `+0x2B3` Animated。静态帧使用 health<2 的布尔值；动画使用当前帧。SHP 头 `+6` 帧数，ref 包装 `+12` 已加载指针，阴影 F+count/2，保持实际裁切偏移／空白帧。产生矿石的树 Y−16 沿用上游标注的 `71C2A9/71C438` 规则，未声称本轮独立核验地址。

OverlayType `+0x2A8` wall、`+0x2A9` resource、`+0x2B3` DrawFlat、`+0x2B5` rock。矿石保持 flat；`+0x2AD` 的 vein 相关标志不能直接当作关闭 body Z 写入。overlay Y 偏移同时进入 Z 基准。正文、阴影仍走共享 CPU/GPU 图像附件，不是另画一份低精度轮廓。

## 验证及剩余边界

证据清单见 `evidence/render-hotfix18-final.json`。重点夹具：`terrain_global_scene.cpp`、`terrain_partial_scene.cpp`、`terrain_sprite.cpp`、`terrain_tint_cache.cpp`、`voxel_pose_cost.cpp`、`scene_frame_reuse.cpp`、`gpu_effect_plan.cpp`、生产 `game_assets_host_test.py`。新几何夹具比较12288组电弧整数线段／粗细覆盖和混合次数，校验 GPU 计划次序、波纹隔离、溢出拒绝及无 CPU 像素的共享帧契约；并不执行 shader。

GLSL 使用 glslang15.1.0 静态编译，spirv-val 验证；合成 HLSL 静态检查不等于运行 Windows D3DCompile。Zig0.13.0 构建 x64 工具箱、x86 Runtime／broker；静态验证 PE、协议80/4、共享 ABI、导出和依赖。没有启动 Windows loader 或 Vulkan。源码中的设备自检属于用户运行时现有路径，本次验证没有执行它。

CPU 仍读取状态、解码源素材、维护静态地形缓存和准备几何；全图小地图／独立副屏窗口没有全部切换为共享纹理直出。原生 A/Z、RGB565、抖动 RNG、波纹细节和 MOD 效果不能宣称逐像素相同。拒绝“采样频率等于逻辑刷新率”“原生画面完成后才能更新”“所有场景都必须每次完整重画”等解释；显卡端稳定性、真实矿石闪烁／树影画面与端到端延迟仍需实机观察。
