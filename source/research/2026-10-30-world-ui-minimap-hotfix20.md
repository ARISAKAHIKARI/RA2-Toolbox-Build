# Hotfix20：小地图直接呈现、盯防世界管线和单位 SHP 绘制

用户要求继续成套迁移画面路径。本轮延续 Hotfix19，使用固定 Vera20K 源码适配已有 C++ 外部呈现器。按用户要求仅编译和检查输出文件；不运行 Vulkan、游戏或 CPU 测试套件。

## 来源与进入生产路径的内容

固定上游 `ARISAKAHIKARI/vera20k@3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`，YuriPlanet 及贡献者，GPL v3。

| 上游流程 | 本项目接入 |
| --- | --- |
| `app/presentation/render/{build_instances,draw_passes}.rs`、`render/batch.rs` | 三个盯防窗口由一个 SceneRenderer 准备完整世界，分别交给与主视口相同的 worldscene::Pipeline；相机移动不重新建立三个局部地形库。只有成功完成图像才一起更新对应相机及目标信息。 |
| 已移植的有序世界特效批次 | 全图 AreaResolve 在一次提交内完成面积缩图、黑幕和相同的粒子／Alpha／波形／电弧批次。波形读取前一个完整输出的 GPU 快照。原有原生地形／对象颜色和 Z 合成仍由 NativeCanvas/ObjectCanvas 准备。 |
| 完整帧输出、独立视口呈现 | 全图小地图可以直接输出 Vulkan/D3D11 共享纹理。主、副屏各自的 DirectComposition target 消费同一不可变帧；边框和视口框在同次 GPU 呈现中绘制，保持一物理像素宽度。缩放仅采样地图内部，不把边框当地图采样。副屏拖动／缩放期间继续呈现。 |
| `app/presentation/ui_overlays.rs::scale_pip_draws`、`render/selection_overlay.rs` | 已支持的选中己方 Vehicle/Aircraft 弹药和载员格改为实际 PIPS2 SHP 命令。普通弹药选 frame13、耗尽不画空格，保留已有 wrap、乘员具体 Pip、Size 和 Gunner 间距。按 PALETTE.PAL 的现场 plain Convert 绘制，不再透出原生像素。 |
| `app/presentation/instances/units.rs::harvest_overlay/emit_harvest_overlay` | OREGATH 在矿车所属对象的绘制位置之后提交，Z 只读、不写，使用现场 ANIM Convert 和现有单位亮度；进入主视口、盯防和实景小地图共有的相机／黑幕流程。 |

`live_shape_cache.h` 按 SHP 身份、完整 shape/frame header 和实际色表缓存不可变图像，每个缓存最多256项、32 MiB。每轮重新读取 plain Convert 中间行，允许同地址色表更新，识别 RGB565/RGB555；不将 plain Convert 当作具有 `+188` 原始色盘的 LightConvert。军衔图标改用 PALETTE Convert；维修扳手保留 MOUSE Convert。

生产反馈宿主删除了 UnitUIStencil 和 EffectStencil 的透视孔调用；旧 mask API 仅保留给兼容调用者。弹药和采矿动画不会再依赖下层游戏画面与外层相机恰好一致。小地图已完成 GPU 特效时，不重复执行宿主 CPU 电弧装饰，也不把已带特效的输出缓存成下一帧底图。缺失素材的临时标记或不支持的共享设备路径仍可返回完整 CPU 图像。

## 沿用的原生证据

本轮没有新增反汇编、受控执行或游戏函数调用。先查询研究索引及持久 SQLite v94，复用 [Hotfix2 矿车与 pips 记录](2026-10-29-render-hotfix2.md) 和 [Hotfix14 UI 记录](2026-10-30-independent-local-ui-hotfix14.md)。

| 用途 | 已有支持 EXE SHA-256 |
| --- | --- |
| pips／UI | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |
| 矿车已认证 draw gate／frame／arm | 上述第一份，以及 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`；不能据此扩大第二份样本的矿车认证范围 |

沿用 `DrawPips 0x709A90`（thiscall，ret12）、`GetPipMax 0x716290`、`CurrentObjects 0xA8ECB8`、`PIPS2 0xAC1480`、Type `+3D4/+3E4/+5E0/+684/+805`、乘员链 `+114/+118/+30/+11C` 及 Size `+380`。本次没有重新执行这些函数。

矿车沿用 `Unit::DrawIt 0x73CEC0` 的已记录局部区间、Type `+E0E`、Unit `+6D2/+538/+388/+674`、Drive `0x4AFC20/0x4AFB80` 的已证明静止路径。形状槽 `0xB1CF98`、ANIM Convert `0x87F6C0`，PALETTE Convert `0x87F6C4`，MOUSE Convert `0x87F6C8`。plain Convert vtable `0x7E5358`，BPP `+4`、shades `+16C`、中间行 `+174`、实际16位像素布局 `0x8A0DD0`。读失败不猜色盘、对象身份或活动状态。

没有增加 hook、游戏写入或 ABI 字段；Assist80／Runtime4 和32位游戏进程保持现有接口。

## 编译记录与尚未完成部分

最终记录见 [render-hotfix20-final.json](evidence/render-hotfix20-final.json)。范围是 Zig 0.13.0 交叉编译、PE 架构／版本／导入检查，以及 compositor HLSL 的离线编译和 SPIR-V 校验。HLSL 离线编译不等于 Windows D3DCompile 或真实驱动执行。没有运行 CPU 夹具、图形设备、Windows loader、游戏；没有 FPS、残影消失或完整像素一致性测量。

建筑放置红绿格仍沿用原生判定和精确相机透视孔。既有记录已证明 Cell membership 不是每格可放置值；本轮没有将它误用作红绿选择。原生侧栏、全部特殊状态 pips、非整数乘员 Size、自定义 Phobos OreGathering 和任意 MOD 特殊 UI 仍未成套移植。矿车桥／特殊格亮度、A-buffer 与原版像素一致性也不作认证。此版本是上述生产模块的继续迁移，不是整个 Rust/wgpu 引擎的原样替换。
