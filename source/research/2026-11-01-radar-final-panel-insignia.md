# 小地图星级在最终面板尺寸合成

## 原因

旧路径在 `radar_decoration.h::radarDecorations` 将 PIPS 14/15/19 写入 worker 的共用场景像素或 GPU 效果计划。worker 按主、副面板的较大尺寸生成一张图，`radar_panel.h` 再分别缩放给两个面板；因此原先「至少一个像素」的星级还会随这第二次缩放缩小，拖动副面板尺寸也会改变星级大小。

## 修改

- `radar_map.h::Scene::ranks` 保存与完成场景绑定的不可变图案和投影位置。`radarRanks` 仍读取原生 PIPS 图和原有星级/伪装判定结果，不改游戏数据。
- 星级不再写入世界 raster 或世界 GPU 效果计划。`panel_glyphs.h` 将已投影的归一化位置换算到各面板内框，保留高度投影；图案按原生像素显示，最小一个物理像素。世界图即使来自较大面板，星级也不再被二次缩小。
- CPU 在最终 `StretchDIBits` 完成后绘制星级，再画视野框和边框。采用两张复用 DIB，失败不替换旧完整前图。GPU 先显示共用世界纹理，再用同一最终坐标绘制小图；同一 PIPS 图共用缓存纹理，裁切与视野框优先级和 CPU 一致。
- `SceneRenderer` 普通完成、静态返回、完整计划复用均携带同次结果的星级。只升星或清除星级也会产生新的输出身份，触发面板重画。渲染失败继续保留旧 Scene 的图案和位置，不把最新 `Frame.markers` 拼到旧图上。
- 显示开关、主副面板生命周期、模式和既有世界绘制策略保持不变；副相机世界渲染不加入小地图星级。

## 验证与边界

`assist/tests/radar_panel_glyphs_windows.py` 编译实际 GDI `Buffer`，并从 production `gpu_compositor.h` 抽取相同 shader，在离屏 D3D11 WARP 执行。52×22、102×42、202×82、302×122、1202×482 五种最终尺寸逐像素 RGB 一致，覆盖一星/三星、缩小/放大/非整倍数尺寸、内框裁切、视野框置顶和失败后前图保留。此测试未操纵游戏或创建可见测试窗口。

同 runner 执行实际 `SceneRenderer` fixture，覆盖 PIPS 元数据不进入世界像素/效果计划、不可变 owner、大小/世界变化、读取失败恢复，并追加只升级、仅位置变化、清除星级时的完整计划复用验证。最终结果记录在工作区 `diagnostics/radar-panel-glyphs/result.json` 与相邻构建/运行日志。

这不是实机 FPS 或 DirectComposition 显示器端测量。CPU 底图可复用但只有星级变化时，当前 `Scene` 包装会复制一次 CPU 像素数组；本次未扩展整套像素所有权架构。普通单位移动本来就会更新世界图。GPU 星级使用小纹理，不上传完整面板遮罩。
