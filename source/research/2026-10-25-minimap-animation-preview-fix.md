# 实景小地图消失、建筑常驻动画与队形预览

**后续更正**：本篇 OwnerObject、相对坐标、PausedAnimFrame 和动画最后绘制的判断被实机快照否定；请以 [RenderFix2 实证](2026-10-24-native-animation-alignment-lighting.md) 为准。其他小地图发布/透明度修复保留。

基线：`01f395f065f5e9d11547601da92e6b8e74e9ae17`（1.0.38-pre.2）。日期：2026-10-25。

## 问题和证据边界

用户澄清：切换“实景”是整张小地图消失；另有油井、展开基地的常驻动画/气候素材缺失，队形预览不透明。用户提出原版动画可能停在第一帧。

调查前读取 INDEX、pre.2 渲染集成及 ccmaps 参考记录，并查询同一持久 SQLite 笔记。此次为本项目源码、固定上游源码与构造内存/素材测试；没有新增受支持游戏 EXE SHA-256、没有实际游戏截图或联机执行证据。以下地址是沿用既有只读布局，不是本轮新反汇编确认的 ABI。

## 整张小地图不能发布

`assist/source/minimap_overlay.h` 在完成 MIX 检索、解码、全景光栅化后检查 `endFrame - sample.frame <= 2`。只要耗时超过两个游戏逻辑帧，就执行 `latest.reset()`；UI 没有 latest 时隐藏窗口。原“示意”路径耗时少，不能据此排除发布门限缺陷。

现在先验证采样上下文、立即发布可用底图；下一张场景计算时保留同一地图、同一尺寸的完成画面。完成后的校验保留阵营、地图 cell buffer、游戏运行/菜单状态、帧回退及请求 generation，正常游戏帧推进不再抛弃渲染结果。采样结束时已经超龄的单位标记单独舍弃，底图仍可显示。

`radar_publication.h` 沿用 `0xA83D4C`（当前 House）、`0xA8ED84`（逻辑帧）、`Map 0x87F7E8 + 0x13C`（格子指针表）、`0xA8E9A0`（对局运行）及原 display 检查。没有新增调用、写内存或 Hook。

被否定的解释：把整个窗口消失仅归因于没有建筑动画。尚不能断言用户机器上只有这一条触发路径；实际安装环境仍需复测。

## 动画采集、素材与合成

原 pre.2 已有 `BuildingClass::Anims[21] +0x55C`、`DamageFireAnims[8] +0x5C8` 读取，因此修复的是现有链路的完整性，不能描述成此前完全没有读取入口。

- 使用当前 `AnimClass::Animation.Value +0xAC` 或 `Paused +0x11A / PausedAnimFrame +0x11C`。零号帧（用户口语的第一帧）有效，停电/未占领时不人工推进。合法的全透明帧仍按原资源渲染，不凭空选另一帧。
- 动画所属关系仍用 `Type +0xC8`、`OwnerObject +0xCC`、父对象与动画 ID 二次检查；隐藏/待销毁继续过滤。
- 优先游戏 SHP 引用中已经选定的文件；建筑/附属动画按当前气候、通用 G、声明基础图回退，不再枚举其他气候。附属动画考虑父建筑 NewTheater。磁盘文件不可用时可解码游戏已经载入的 SHP。
- 气候来源沿用 `LastTheater 0x822CF8` 和 `Theater::Array 0x7E1B78`、stride 112、Extension +78、Letter +86；加入资源修订号，避免同一上下文沿用上一气候图像。
- 实景小地图解码由固定从头遍历改为轮转预算；更新动画帧排队时保留对象最后完成图像，隐藏/删除/类型与所属方变化则清理。
- 附属动画在物体绘制之后合成，仍测试场景深度、不写建筑深度；副摄像机采用同样顺序并继承父建筑桥层。

固定参考：[YRpp d9795546 AnimClass](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/AnimClass.h)、[BuildingClass](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingClass.h)、[Theater](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Theater.h)；[ccmaps BuildingDrawable](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Drawables/BuildingDrawable.cs) 的延后动画绘制与父建筑 NewTheater。独立实现 C++，没有移入上游实现。

## 队形预览

pre.2 共用 `renderActor` 已输出 alpha=255，但 formation 的 ColorMatrix 仍保留 alpha=1，导致旧预览透明度丢失。现在只在预览呈现时应用 `124/255` 不透明度，RGB 与 alpha 同步预乘；共用图像缓存仍是不透明原图。SHP/VXL 使用与实景相机相同的量化光照规则和 240–254 自发光索引处理。

预览继续使用目标格的光照与 Slot.z，投影使用当前 Z 比例；不以源单位所在格代替落点。受控测试覆盖高地、桥面/桥下与动态灯光。没有调整原生队形寻路或发令。

## 验证入口

- `assist/tests/minimap_overlay.cpp`：正常采样后逻辑帧推进 120 帧仍可发布；帧回退、换 House/cell buffer、退出可见对局仍拒绝；10,368 组布局。修正此前测试中遗留的导航半屏偏移期望，使其符合已有“传递中心点”的实现，导航代码没有更改。
- `building_animations.cpp`：播放/暂停零号帧；六种气候的油井和三方基地动画名称；已解析文件优先；隐藏、回收与坐标。
- `radar_scene.cpp`：32 个持续变帧对象不能饿死末尾动画；预算不足保留可见图像；负偏移动画延后合成；删除立即清理。
- `formation_host.cpp`、`camera_raster.cpp`：真正输出像素的预乘透明度、光照、自发光与 SHP/VXL；目标高度、桥面和桥下光照。
- 原有 watch、SHP/TMP、资源解析、live INI、投影与架构边界检查。

受控测试不证明原版画面完全一致，也不证明 Windows 游戏内显示或联机稳定性。
