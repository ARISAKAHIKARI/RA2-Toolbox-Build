# 星级 SHP 素材缩放：共用 CPU 重建与屏幕锚点

基线：GitHub main / v1.0.41，`513aeb4cf03238cce956bd35e0c9381c2a7cc484`。本笔记记录源码修复和 Windows 离线验证；没有实际游戏画面、游戏帧率或联机验证。

## 问题与已有约束

`external_reader/source/local_ui_renderer.h` 的等级图标直接把 PIPS 14 / 15 / 19 的原始颜色行送给 `Painter::fill`。几何会随主画面输出尺寸变化，但绕过普通 SHP 的素材重建，扩大时仍是原像素块。它属于屏幕 UI，不能再次当作整张场景缩放，也不能跟着地形的深度、遮罩或光照走。

小地图自 2026-11-01 的 final-panel insignia 修复后，把等级图标保存在已完成 Scene 的不可变 metadata 中，在最终面板尺寸叠加。`panel_glyphs.h` 的锚点随 world-to-panel 比例移动，图标每轴保留至少一个物理像素的原生尺寸；此设计防止小图把星级二次缩小。本次保留这套定位、最低物理尺寸、场景所有权和 CPU/GPU 最终叠加契约，只处理素材像素。

HF29 以来 `assist/source/main.cpp` 的 recovery 仍固定 detail / precision / palette 为 0，effective scale 为 1，关闭全局重建与插值。这是整场景 GPU 卡死、闪烁等问题后的恢复设置。本次没有修改这些开关；放大星级作为小型屏幕素材，按实际输出尺寸局部调用普通 SHP 的 CPU 函数。世界 SHP 的默认启用条件仍受 recovery 约束，并非宣称全场景重建已恢复。

## 修复

- 将 `xbrz_adapter.h` 里纯 CPU 的 `ScaledImage`、straight / premultiplied alpha 转换、`factor`、`scale`、`sample` 原样提取到 `shared/source/sprite_scale_cpu.h`。普通 SHP 保留原命名空间的 using aliases；算法、2/3/4 倍因子、`argbUnbuffered`、像素中心双线性 resolve 与原版完全相同，不拉入 Vulkan 或 D3D 对象。
- `shared/source/screen_sprite_scale.h` 只管理小型不可变素材的输出尺寸缓存。素材身份、源尺寸和精确目标尺寸共同作为 key，保留 source owner 防止地址复用。每线程最多 4 MiB / 128 项，重建和输出各受 1M 像素上限约束。原生、关闭、缩小、超 4 倍或超安全像素上限保留旧源像素路径；合法大源不变空白。不可缓存的大输出不会驱逐已有 warm entries。
- 主渲染仅让等级 14 / 15 / 19 走此缓存，沿用原先像素边界舍入和 SHP 偏移；只在局部素材重建后直接生成最终物理坐标的颜色行，避免再经 `fill` 放大一次。其余字体、编队数字、普通 pips、维修和规划图标不变。旧结果的可见性现在也比较 presentation profile，避免切换时采用旧 profile 的 UI 发布。
- 小地图 `panelglyphs::place` 保留原来的 x/y/width/height，给 CPU Buffer 和 GPU compositor 共同提供同一份已重建素材；无需各后端另写滤波规则。小于原生的面板仍保持最低物理像素规则。
- `radar_panel_glyphs_windows.py` 的两个编译目标都显式链接 xbrz.cpp；生产 host 原已链接。没有改全局 canonical build 设置。

## 有效离线证据

`assist/tests/rank_sprite_scaling.py` 冻结了原始 `local_ui_renderer.h`、`panel_glyphs.h` 和 `xbrz_adapter.h`，manifest 校验 SHA256 与 Git blob；生成的对照直接编译原始 Painter / Engine / placement / CPU 函数，同时编译当前源码，不以镜像实现替代生产代码。输出记录测试前后 source SHA 稳定性、实际编译命令和时间。

2,906 项断言涵盖：真实 SHP 解析缓存与 PIPS 14/15/19 的等级选择；125%、150%、200%、300%、400% 和非等比输出的 18 项旧版负向对照；1:1 / 缩小及其他 UI 逐像素不变；recovery scale=1 下局部修复有效；profile 变化拒绝；不可变 live palette 更换；alpha / 素材身份 / 命中；小地图原锚点与最低物理像素；合法大源和预算 fallback。提取的普通 SHP CPU 函数在混合 alpha、2/3/4x、fractional resolve 上与冻结的旧函数逐像素完全一致。

`assist/tests/radar_panel_glyphs_windows.py` 比较实际 GDI Buffer 与 production GPU shader 的 D3D11 WARP 输出：原 5 组 native RGB 完全一致；另 8 组含 150%、200%、300%、400%、非等比、超支持比例 fallback、半透明、裁切和 viewport chrome 交叠，RGB 最多相差一个整数混合舍入单位。还保留失败 stage 留旧图、不可变 Scene 归属、SceneRenderer 最终尺寸与 metadata 测试。

这些是可重复的源码和软件渲染验证，不替代真实游戏的视觉验收。复现 runner 均使用 `--zig <zig.exe> --out <目录>`，路径作为独立 subprocess 参数，支持空格路径。
