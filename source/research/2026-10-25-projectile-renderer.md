# 抛射体采样与外部绘图接入

## 问题、基线与证据

问题是观察窗和实景小地图没有采集、绘制飞行弹体。实现基于 pre.8 源码提交 `070f19e91cee76629cbaa4a45f66ee310da3e7b5`。调查前已读 AGENTS、研究索引及 SQLite v44；已有躲弹研究只证明部分位置／速度字段，不足以直接生成画面。

支持原版映像 SHA-256 为 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。新增原版地址、ABI、字段、受控执行及排除结果详见 [抛射体原版绘图](2026-10-25-projectile-native.md)；该笔记与 `native_projectile_capture.py`、`verification/native-projectile-capture.json` 是本实现的数值来源。三份私人 capture 的 Bullet 数组均为空，本轮使用合成对象执行原版选择／数学代码，不将其描述为实战画面重放。

## 独立采样，不借用单位生命值

新增 `external_reader/source/projectile_visuals.h`，提供：

- `capture(read, pointer, State&)`：读取 Bullet 独立身份、位置、类型、运行时动画帧、速度及实际色盘选择；只有完成一致性检查后才覆盖输出。
- `all(read)`：遍历 `BulletClass::Array 0xA8ED40`，去重并采样。调用者继续使用当前地形／黑幕／视域准入。
- `light(read, state)` 或 `light(read, converter)`：固定1000亮度，只在真实 LightConvert 上读取当前 tint；不增加 Unit／Infantry 的额外亮度。
- `captureArt(assets, pointer, type, id, out)`：优先调用资产上下文的 `captureProjectile`，不提供该方法的既有测试替身返回false。

Bullet `+0x6C` 是伤害，零伤害和负伤害均不能作为死亡条件。OnMap读者准入亦对照了普通Bullet AI `0x4666F7` 的+90检查；该检查是新增反汇编证据，没有冒称是Draw_It自身的gate。此路径完全独立于 `GameView::live`。采样检查数组、虚表／ID／Type、InLimbo、OnMap及可读位置，拒绝 Inviso、SpawnNextAnim、非有限速度及读取中身份／位置变化；这些读取检查不等于对整个世界的原子快照。

层级按原版 `0x468B90`，为 `Flat ? 1 : 3`。不读取或验证 `+0x94 CachedLayer`。SHP绘图偏移保留原版固定Z调整-30；VXL按独立路径处理。SHP朝向或动画帧、VXL双轴朝向均进入既有 `visual::Pose`；palette converter作为variant进入源素材和姿态缓存，避免不同InheritedColor互相复用。

## 资产和绘图

`camera_assets.h::captureProjectile` 不使用 TechnoType 的大偏移字段，也不从当前发射者的通用单位图像猜弹体素材。它读取 BulletType 已解析的 SHP 或 VXL/HVA，按原版分支选择资源／ANIM／InheritedColor／VXL ColorScheme 的色盘。

plain ConvertClass 没有 `+0x188` 原始色盘。资源及ANIM优先读取它自己的RGB565中性色阶，缺失时分别回退 `PALETTE.PAL`、`ANIM.PAL`；Scheme使用对应原始色盘。全黑但完整可读的色盘可以合法保留。

SHP渲染使用真实当前帧；原调用的ZGradient参数固定Ground(0)，因此图像使用现有ground/flat深度近似，独立于Type.Flat只改变Layer的规则。完整逐像素Z ramp未重新执行。弹体阴影复用同一帧的不透明遮罩，放到地面；不使用普通单位的“后半SHP帧”规则。新增 `Image::airborneShadow` 仅使这种附件在距地高度为0时不绘制；其余附件行为保持既有路径。

VXL通过原有模型／HVA捕获与面绘制，但增加本地Y俯仰旋转。非Flat弹体的0x800原版blitter既不读也不写Z，外部通过现有NaN-depth语义跳过场景Z测试；Flat的0x2800保留地面Z读取，均不写Z。不能把所有VXL都改为普通单位的只读Z模式。`voxel::render` 的新增可选参数默认 `pitch=0, projectile=false`，既有调用保持车辆路径。弹体使用 `753C80 -> 758670` 对应的纯diffuse法线页，不能使用车辆的diffuse＋specular页。原版mode4的32个水平朝向、5种竖直速度，共119,040个生产helper／原指令页值逐项比较，零差异；旧车辆11,936项VPL黄金值回归继续通过。

弹体VXL缺少有效VPL时返回明确失败，等来源revision更新后重试，不伪造车辆Lambert结果。该失败只影响此弹体图像，不写入游戏、不修改弹道、不安装新Hook。

## 验证

- `assist/tests/projectile_visuals.cpp`：零／负伤害、独立数组、SHP四方向和运行时动画覆盖、VXL双轴姿态、三种色盘选择、身份竞争／逐项失读拒绝、显式Layer、SFINAE替身、纯diffuse与车辆页差异、离地阴影组合。
- `assist/tests/game_assets_host_test.py`：真实生产Context＋合成Win32文件／内存边界，覆盖已载SHP、plain／ANIM中性色盘、`PALETTE.PAL`回退、同帧阴影、独立VXL/HVA捕获、pitch／diffuse输出像素、Flat结果及缺VPL拒绝。原有单位、建筑、驻军、动画和VPL恢复测试继续通过。
- `assist/tests/voxel_lighting.cpp`、`voxel_preview.cpp`、`camera_raster.cpp`：均通过；默认车辆着色和原有深度／透明度行为无新增失败。
- 原版受控数字、图像选择、矩阵和Layer结果由上述native笔记及JSON单独计数。

## 范围与未决项

这里覆盖SHP／VXL Bullet本体，以及已存在AnimClass集合中的动画效果。Trailer为AnimType，不在外部重新生成；尾迹生成时序、粒子系统、激光、辐射束、电弧各有独立实现，不能宣称仅遍历Bullet就已覆盖所有武器视觉。

目前没有真实飞行弹体的同帧截图、完整原版rasterizer输出、Windows最终合成、所有VXL法线模式或双机联机验收。原版数值匹配与合成像素测试不证明这些未执行范围。完整场景层级／遮挡仍沿用现有外部compositor，其近似界限没有因新增弹体而消失。
