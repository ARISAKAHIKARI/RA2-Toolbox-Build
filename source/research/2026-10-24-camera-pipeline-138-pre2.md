# pre.2 副相机、实景小地图与覆盖层实现

日期：2026-10-24。实现进入集成验证；尚无游戏实机画面、真实 FPS 或联机验证。

## 证据

先阅读 INDEX、ccmaps 研究准备、pre.14 显示笔记与 INI 图标记录。持久 SQLite 因执行环境不可用，保留仓库记录供后续写回同一文件。

算法参考固定 [ccmaps-net 56712d7](https://github.com/zzattack/ccmaps-net/tree/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a)。C++ 绘制实现独立编写；VXL 法线数字表是资源格式兼容常量，文件内保留来源。没有移入上游 C# 实现。公开格式与上游测量常量不等于本轮引擎反汇编证据。

动画布局来自 [YRpp d9795546](https://github.com/Phobos-developers/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)。坐标依据 [Phobos Anim Hooks](https://github.com/Phobos-developers/Phobos/blob/e81bf980f4606272a5928edef111852c81961433/src/Ext/Anim/Hooks.cpp) 的 _GetCenterCoords：附着动画 Location 加 OwnerObject::GetCoords。没有新增实机支持的 EXE SHA256；之前 LBW/战网样本哈希不能替代本次 ABI 执行验证。布局不符时只读采集失败，不调用或补丁引擎。

## 核心变化

- 同一 cameraassets::Context 与 CPU 画布供副相机和实景小地图使用，各线程保留自己的读取会话与可变缓存。
- VXL 捕获法线，HVA 后转换与独立车体/炮塔方向；读取 VOXELS.VPL 选择颜色映射，导出逐像素深度；资源不足时保留普通光照回退。增加独立地面阴影。
- SHP 保留原色索引、画布尺寸，区分自发光索引 240–254，光照按 63 档量化。采用游戏采样后的每格强度，不再次叠加高度/灯源。
- TMP 按 flags&2 读取主图与额外图 Z 数据；无 Z 数据保留普通图像路径。颜色、深度分别合成；近处像素严格覆盖，单位/动画只测试深度。
- 建筑主体读取 BUILDNGZ.SHA/SHP 深度形状，按 Foundation、ZShapePointMove、NormalZAdjust 定位。RA2/YR 参考点 198,446、偏置 -66 来自固定上游测量。普通主体增加健康/受损帧；施工、闸门、废墟等特殊主体序列尚未逐项复刻。
- SHP 后半段阴影在主体前合成；空中投影按地面/桥层落地。桥面单独光照和 4 层深度偏移。
- 最小化按帧磁盘读取：模型/解码/朝向缓存分别保留，有工作预算；主体 Z 图共享，模型与纹理缓存有界。

## 实际建筑动画

读取 BuildingClass::Anims[21] +0x55C 与 DamageFireAnims[8] +0x5C8，仅渲染实际存在并归属该建筑的动画，不根据时间虚构生产/停电/超武状态。

AnimClass：Animation.Value +0xAC，Type +0xC8，OwnerObject +0xCC，PaletteName +0xDC，ZAdjust +0x100，Paused +0x11A，PausedAnimFrame +0x11C，TimeToDie +0x179，Invisible +0x19D。BounceClass 大小 0x50 用于推导尾部布局。AnimTypeClass：YDrawOffset +0x344、UseNormalLight +0x35D、AltPalette +0x361、Flat +0x369、Shadow +0x372。验证父对象 ID、动画 ID/类型/所属方，限制帧号和位移；暂停使用原暂停帧。

建筑光照/重映射与自定义 Palette 分开处理；动画不写主体深度。未宣称支持所有 Ares/Phobos 私有扩展、复杂像素偏移、体素建筑炮台或所有特殊建筑帧。

## 小地图与窗口

Display.MinimapMode：0 示意、1 实景，默认 0；小地图功能开关仍默认关闭。实景按等距投影宽高比例显示，与副相机共用 TMP、SHP、VXL、建筑附加动画管线；完整地形帧逐步生成并原子发布，生成下一帧期间保留上次完整画面，单位位置继续更新。点击/拖动导航沿用同一地图几何。

overlay_stack 统一层级：世界标记 < 单位统计 < 小地图 < 副相机 < 飞机按钮 < 建造面板 < 诊断 < 光标。各所属线程只移动自身窗口；主覆盖层排除已显示的不透明面板矩形，不再依赖最后一次 HWND_TOPMOST 的先后顺序。

## 验证入口

camera_raster.cpp：逐像素交错遮挡、同深度、动画不写 Z、自发光、量化光照、VPL 范围与朝向变化。
building_animations.cpp：实际 slot、相对坐标、暂停/隐藏、父对象重用、读中变化、Z 图定位。
radar_scene.cpp：完整背景、真实对象合成、暂停帧构建、缓存、尺寸和资源变化。
watch_host.cpp：300 类型公平预算、共用姿态缓存、共同位置时钟、单画布坐标和逐像素遮挡。
overlay_stack_windows.cpp：真实 HWND 层次、重复移动、显示/隐藏、失效句柄和遮挡矩形。
windows_settings_smoke.py：真实组合框选项、MinimapMode 写入和两次重启，以及原有全部默认关闭与用户快捷键检查。

Actions 成功仅证明受控测试、编译与真实 Win32 窗口/设置行为。实景质量和负载仍需用户安装环境实机对照，不能据此声称原版渲染完全一致。
