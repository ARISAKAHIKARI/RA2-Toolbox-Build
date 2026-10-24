# 1.0.38-pre.2 副摄像机渲染升级：ccmaps-net 核对记录

日期：2026-10-25。状态：源码研究与实现范围确定，尚未完成移植或发布。

## 来源与边界

- 参考 [zzattack/ccmaps-net](https://github.com/zzattack/ccmaps-net/tree/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a)，固定提交 56712d7c9c5048afbfe9bddb01c0ec43f6fc008a。
- 本项目基线 40e2a098266d808d190b7302e223b2502252cd3b。
- 先阅读本项目研究索引和 pre.14 相机说明，核对 game_assets_host、watch_windows 与 scene_lighting。
- 本轮是源代码证据，没有新游戏映像执行或实机画面对照。上游注释中的引擎地址不是本轮验证的新 ABI。
- 执行环境不可用，无法读取本地持久 SQLite，记录先保存在仓库；恢复后同步同一笔记文件。

## 已确认的参考实现

### 光照与色盘

[Palette.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/Palette.cs) 使用 Ambient−Ground+Level×高度，加灯源后对环境与 RGB 分量分别限幅，再按最大色调通道归一化；亮度量化到 63 档。对象色盘 240–254 保持不受该光照变换影响。普通物体、地形、动画、矿石有不同色盘和光照路径，不能统一乘 RGB。

本工具已读取游戏每格计算后的亮度，因此不能再次叠加同一 Ambient/Ground/Level 或灯源。应复用已采样结果，校正色盘量化与类别，再分别验证阴天、夜景、有色灯、不同高度和临时光照。

### VXL / HVA

[VxlRenderer.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/VxlRenderer.cs) 为各 HVA section 构造变换，分开处理朝向、平移与模型 bounds。优先使用 voxels.vpl：法线、朝向、光向和镜面分量决定 VPL 页，再把原体素颜色索引映射到色盘索引；特殊法线索引有单独处理。无 VPL 时才采用简化亮度回退。

阴影使用独立地面投影；车身、炮塔、炮管的姿态和阴影不是一张图的统一旋转。本工具目前的模型缓存与车体／炮塔分离可继续使用，需补齐 VPL、法线明暗和像素深度。

### SHP / TMP 与遮挡

[ShpRenderer.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/ShpRenderer.cs)、[TmpRenderer.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/TmpRenderer.cs) 和 DrawingSurface 使用逐像素深度／高度和阴影数据。建筑形体、地面平铺、单位、动画各自决定是否写入深度，不能统一按图像下沿覆盖。建筑附加动画通常只测试深度，避免覆盖主体的形体深度。SHP 后半段阴影、透明索引、平面对象和桥层都有独立逻辑。

这正是本工具 pre.14 分成地表／树木／对象三个整图层仍无法解决的部分。升级目标是颜色缓冲加逐像素深度与类别写入策略，而不是继续叠加 Y 排序特例。

### 覆盖图

[ObjectCollection.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Game/ObjectCollection.cs)、SpecialOverlays 和 DrawProperties 区分墙、资源覆盖图、高桥等。覆盖值选择帧，高桥有图像／阴影偏移和额外高度；矿石的图像及阴影来源也不必相同。

需按上游分类建立对应规则并与原版资源对照，不将 FA2 色盘选择当成所有覆盖图都正确的终点。

### 建筑动画

[BuildingDrawable.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Drawables/BuildingDrawable.cs) 组合主体、bib、炮塔、ActiveAnim 系列、IdleAnim、SuperAnim、受损替代和火焰；每个部分有自己的偏移、深度调整、色盘及阴影。AnimDrawable 和帧选择器处理开头、循环以及缺电／未占领停帧。

ccmaps-net 是地图渲染器，某些动画帧依靠模拟时间和地图初态，并有意选择 IdleAnim 代替生产动画。本工具的实时副摄像机必须用实际游戏帧和已经核实的建筑状态，不直接套用静态地图的状态假设。

## 实现顺序与验收

1. 保留现有统一读取、外部渲染、素材缓存和共用刷新时钟，先新增可独立测试的色盘/VPL 与像素深度模块。
2. 接入 VXL 身体、炮塔、炮管及阴影；对照原版素材各朝向，检查玩家重映射色和法线。
3. 接入 SHP/TMP/覆盖物深度和桥层；对照楼前／楼后、树前／树后、悬崖和桥上／桥下。
4. 加入建筑主体及附加动画；对照普通、受损、缺电、生产、超级武器状态，并确认独立色盘。
5. 验证三个观察窗共用缓存、主覆盖层遮挡、菜单隐藏、缩放及连续移动；资产加载不得重新变成每帧磁盘读取。

只参考并核对上游算法；如实际纳入 GPL-3.0 源码，需要连同许可证与来源一起处理。尚未运行图像黄金样例、性能对照或 Windows 实机测试，不宣称与原版一致。

## 用户补充：MIX 图标

建造列表与单位统计统一使用实际游戏资源来源和资源缓存，按类型的 Image / Cameo 配置解析 SHP 图标及对应调色板，支持 loose 文件与 MIX 优先级；读取失败保留明确的替代图。先核对现有 game_assets_host::cameo 与 unit_monitor 的 assetFiles/icon 缓存路径，避免重复实现已有 MIX 能力。此项是后续实现要求，当前未宣称完成。

后续核对：单位统计原有 Cameo SHP、cameo.pal 文件优先读取，建造面板原有 SHP 文件优先、色盘却只读原生转换器；1.0.37 正式版工作已统一共同色盘入口。图标无需等副摄像机大改才能修复。
