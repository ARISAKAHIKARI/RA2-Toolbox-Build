# Hotfix14：独立本地 UI，替换宽松原生透出区域

## 问题与适用范围

问题：Hotfix13 通过保留原生消息、倒计时、主要工厂和集结线附近的矩形显示本地 UI。主要工厂矩形还会透出原版建筑／地形，不能视为已完成的独立绘制。此版删除这四类矩形并绘制自己的字形／线段。

原地址布局沿用 Hotfix13 的两个受支持样本：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。新适配依据公开源码及值夹具；本轮未重新执行这两份私有 EXE，也未取得真实 Windows 游戏／GPU 画面。

## 固定来源与新增认识

- [Vera20k](https://github.com/YuriPlanet/vera20k/tree/0b6a073d0928453036c4a53d117bb7a2d8d573a7)，GPL v3。具体参考 `src/assets/fnt_file.rs`、`src/render/bit_font.rs`、`src/render/surface_line.rs`、`src/app/presentation/ui_overlays.rs`、`src/app/presentation/target_lines.rs`、`src/rules/object_type.rs` 和 `tools/procedural_drawing_oracle/rally.py`。这是独立语义／字体适配的依据；不将上游原生执行结果冒充本轮执行。
- [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw/tree/279a057ee7e1e4d584b56141c014ab01f3c6ee6d) 的 `src/render_d3d9.c`、`src/ddsurface.c`、`src/IDirectDraw/IDirectDrawSurface.c` 用于检查现有像素表面、8／16／32bpp、调色板和透明键的呈现关系。它消费现有表面，不能单独提供 RA2 对象、CSF 或集结线语义；未移植其代码。
- [YRpp](https://github.com/Phobos-developers/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2) 的 BitFont、StringTable、ColorScheme、MessageList、TextLabel、BuildingType 等继续作为公开字段命名依据。

字体：`BitFont*=89C4D0`，内部数据指针 `+4`，行高 `+1C`。内部九 DWORD：最大宽、每行字节、位图行数、单元高度、槽数、槽步长、UTF-16 索引表指针、字形指针、有效符号数。65536 个 u16 索引为 1 基槽号，0 表示缺字；每槽首字节为宽度，余下逐行 1 位、最高位在左。原生文本宽包含字符尾部 1 像素间隔。空格只前进，不输出墨迹；缺字使用反相 U+00B0 和 RGB565 XOR 5555 的变色。只复制游戏当前载入的字体，不捆绑游戏字体或用宿主字体替代中文。

本地化：StringTable 的 LabelCount／ValueCount／Labels／Values 位于 `B1CF6C..B1CF78`；标签 40 字节，名称 32 字节，`+20` 数量、`+24` 首值索引。值是游戏已解码的 UTF-16 指针。读取 `TXT_PRIMARY`／窄基础用 `TXT_PRI`、`MSG:BlackoutTimer`。颜色使用 ColorScheme vector `B054D0` 的方案 `+308` RGB，或 House `+56F9` RGB，再按 RGB565 量化扩展。

文字：MessageList `A8BC60` 的普通链、编辑标签和 SkipDraw 条件继续读取，不调用游戏绘制方法。标准 TextLabel 的位置、文本、颜色和宽度裁切形成透明字形蒙版。输入框光标、特殊排版和第三方自绘聊天框仍不完整，产品沿用聊天输入时暂停覆盖的门控。

倒计时：scenario `+11E8/+11F0/+11F4`、Super.ShowTimers `A83D50`、Super `Type+28/Owner+2C/Timer+30/+38/Blink+40`、House blackout `+2A4/+2AC`。以当前原生帧计算剩余值，15 帧为一秒，右侧三像素、行高加二；读取现有闪烁字节，不推进计时器或游戏 RNG。战役全满暂停 SW 的隐藏规则尚未移植。

主要工厂：原生 `70ABEA` 的 primary 字节 `Techno+3D3`；选中、存活、可见列表内、友军对象才处理。Vera 的 `Building43E7B0 -> Techno70AA60` 对照明确：文字锚点来自 `459EF0` 的原 Location.x/y 各减 128，不是 foundation 中心，也不是缓存 body bounds。基础宽度决定 CSF 键，`0x149` 居中和四／二像素黑底边距。这里只实现 primary 文字；电力建筑的 power/drain 状态字尚未移植。

集结线：HasRallyPoint `455DA0` 使用 `BuildingType+EB8` 的 Factory=16／40，或 `+16A9` UnitRepair、`+16AC` Cloning。起点 `GetCoords447AC0` 是 Location 加基础中心偏移；ArchiveTarget `+218` 只接受当前 Map 数组中实际匹配的 Cell 指针。读取基本 level／bridge，采用上游 pattern `[1111100011111000]`、`(INT32_MAX-frame)%15`、Y+2 起黑／色／色三遍、递减 Y，逐遍复用被裁切的端点。横竖线包含末端，斜线不含末端；反向端点调整相位。坡面高度、对象目标、原 ABuffer 两遍分区和自定义标志仍未完整实现，不能称像素一致。

## 产品与性能边界

`native_ui_font.h` 缓存字形覆盖及 CSF 标签索引；字体身份改变时重建，预算淘汰后仍可重新解码，不作为对象名额。`local_ui_renderer.h` 只产出四组自身的有色像素矩形，不接受原生 framebuffer，也不生成 cutout。字体、文字背景和线条由工具箱绘制；黑色文字衬底只贴住文字。

`local_ui_task.h` 使用私有 ReadSession 和 Latest worker，读取／查表／布局／栅格化不运行在 UI 线程，只提交必要的建筑身份。消息、计时器、状态字、集结线分别缓存紧致纹理，内容不变复用共享像素引用，避免用一个透明大纹理连接屏幕两角。进程、对局、代次、视角／视口、Context token 不符或超过 150ms 拒绝显示。只加入主视图合成组；副相机和小地图不读取这些本地 UI。

这里没有删除既有激光、采矿、pips、放置网格等精细原生 stencil，也没有补齐所有动画。不能把四类矩形删除泛称为所有原生像素依赖均已消除。

## 证据、拒绝方案与未决项

`assist/tests/native_local_ui.cpp` 使用完全合成的只读内存：真实 FNT 结构、中文／空白／缺字、缓存更换、CSF、普通／编辑消息、SW／任务／停电计时器、primary 坐标和友军、实际 Cell／HasRallyPoint、裁线相位、四组透明层、后台阻塞与旧代次拒绝。地址／未定义行为检查通过；受环境限制未启用 LeakSanitizer，不声称检查泄漏。

`assist/tests/render_feedback_host.cpp` 检查最终主视图接入，`tools/validate_native_feedback.py` 检查只读约束及完整七层原子组。Hotfix13 的 196 组原生 UI 字段／锚点、48＋6 Runtime 路由／guard 结果属于既有证据，不是此版新增执行。

拒绝继续扩大宽松透出区域；拒绝将 cnc-ddraw 的整表面上传当成 RA2 UI 语义重建；拒绝仅用宿主字体近似中文；不发布私有 EXE／字体／raw dump。尚缺 Windows 目视验证、原生字形逐像素对照、物理 GPU 性能及 FPS 实测。最终构建、回归与输入摘要见交付包 Build-info.json。
