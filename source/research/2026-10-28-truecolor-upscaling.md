# 全视口渲染实验：24-bit 调色盘与原生像素 2× 插值（第一阶段）

日期：2026-10-28。分支：`experiment/render-upgrade-v139pre6-20261028`。
只修改 **外部读取 / 场景呈现 / 工具设置** 路线；并未修改原版 `gamemd*.exe` 的代码；但这条功能分支的原有 `GameRuntime.dll` 仍含其它旧功能的可执行补丁，**不等于全工具箱严格无代码段修改**。正式版、latest 和严格无补丁分支未修改。

## 实装

1. 新增 `external_reader/source/pixel_upgrade.h`：独立的调色盘全范围映射、RGBA 安全的 2× 最近邻 / 轮廓条件混色（受 Scale2x 邻域启发的原创简化版，**不是 xBRZ，也不是 Scale2x 源码**）。
2. `asset_decode.h` 支持两种 256 × RGB6 `.PAL` 转换：
   - 原始兼容模式：DAC 各通道 `c*4`，值域 0..252。
   - 24-bit 全范围模式：`(c*255+31)/63`，值域 0..255；输出依然是 `0xAARRGGBB`，index 0 透明。**原素材仍然只有最多 256 种颜色，扩大值域并不能创造真实纹理。**
3. 只在 `camera_assets::Context` 的渲染升级 worker 选择 `FullRGB`；原有小地图、单位预览默认 `OriginalDAC`，不修改对象的实时 owner/remap 色、运行时 LightConvert 色表和光照。
4. `NativeCanvas::presentRegion` 若最终画面恰好是原生画布的 2× 输出（两轴均是 0.5 源像素/目标像素，整数裁剪起点），可选最近邻或邻域条件混色。其他比例 **保持原有呈现**；切换模式清理场景呈现缓存，但不切换地图视野或逻辑相机缩放。
5. 设置：`[Display] RenderPaletteRange=1`（默认完整 RGB）和 `RenderPixelScale=0`（默认不特殊处理）。面板额外增加两组下拉，沿用自动保存。开关时触发异步 worker 更新、校验上下文、旧图失效。

## 特效完成度（明确标记）

已在这条分支接入/保留的功能：基础 SHP/VXL 帧、建筑零件/动画、炮弹画面、阴影、原有调色盘 owner remap 与光照、迷雾遮罩、原生选择/血条与原帧软件光标；电弧为**近似重建**，现追加在独立软件画布内的有界半透明柔光晕（按帧确定性扰动，不调用游戏 RNG；仍不能声称与原版等价）。

仍未实现的原版完整重绘：多种激光、波纹、粒子、所有特殊附加状态，以及严格原生逐像素对齐；第三方 xBRZ 尚未集成。此次修正的是普通 SHP/PAL 色值精度与可切换 2× 输出算法，**不把这些未完成特效宣称已补齐**。

## 公开方案对照

- xBRZ（Zenju，v1.9，GPLv3）：高品质像素边缘放大、可用 alpha；适合后续高品质 2× 方案，官网：https://sourceforge.net/projects/xbrz/files/xBRZ/ 。
- Scale2x（Andrea Mazzoleni，GPLv2-or-later）：轻量/经典 2× 邻域法，官网：https://www.scale2x.it/ ，源码：https://github.com/amadvance/scale2x 。
- hqx（LGPL-2.1）：可选 2×～4×，色差阈值/边缘模板方法，参考：https://github.com/grom358/hqx 。

本轮 `pixel_upgrade.h` 是独立实现，没有复制上述项目源码或捆绑外部 DLL。若要引入 xBRZ 原始代码，需要保留版权与许可声明并通过独立测试；不能仅改名字就声称是 xBRZ。

## 限制与安全检查

- 本轮是软件 CPU 合成、16M 像素上限，不建立新的游戏 Hook，不调用主游戏绘制函数，不修改网络事件或探索状态。
- 只在 2:1 整数输出像素采样时调用 2× 插值，不等于放大镜头 Zoom 2×；其它实际比例如常见 1:1 或 1.25:1 仍走原有像素面积采样。
- 缩放由已有视口呈现路径完成；HUD/光标仍由独立原生坐标逻辑定位，不随纹理放大错误偏移。
- 透明邻域不产生伪造不透明边缘；原生未渲染 backing 外回退透明。遮罩仍由原有最终输出逻辑施加。
- CI 加入 `pixel_upgrade.cpp` 与源码契约测试；构建成功也不等于视觉等价、稳定帧率或双机联机安全。真实游戏与原版截图对比需要实际采集。

## 下一步

优先对真实 SHP/VXL/电弧与阴影截帧做 SSIM/边缘与色差对比，同时测量 2560×1440 全视口 CPU 耗时；评估正版 xBRZ 的稳定许可集成和离线缓存。之后逐一补激光与粒子特效对应的可验证读取资料。

## A/B 同源同帧对照（已接入工具和 CI）

渲染分支新增纯本地、不访问游戏进程的 `source/tools/shp_palette_ab.py`。对**同一份 SHP 文件、同一个帧号、同一个 PAL 文件**只解码一次索引像素，再在相同裁剪锚点计算 DAC(6-bit×4) 与 RGB888(6-bit→8-bit)，以及由这**同一 RGB888 索引图**生成最近邻与轮廓条件半混色的严格 2× 像素图。两次对照无时间推进、无相机位置变化、无切换设置时的跨帧误差。

Windows 上在仓库根目录执行（请指向自己的游戏安装资源，勿上传版权素材）：

```powershell
py source/tools/shp_palette_ab.py --shp "D:\\assets\\unit.shp" --pal "D:\\assets\\unit.pal" --frame 0 --out "D:\\RA2_AB"
```

没有实际素材时运行验证用的版权无关合成图案：

```powershell
py source/tools/shp_palette_ab.py --demo --out "D:\\RA2_AB_demo"
```

输出 `01_dac_1x.png` / `02_rgb888_1x.png`、`03_palette_diff_x64.png`（每通道差值扩大 64 倍，仅用于看差异）、`04_nearest_2x.png` / `05_edgeblend_2x.png`，以及 `06_palette_AB_same_frame.png`、`07_upscale_AB_same_pixels.png` 两张同尺度左右对比图。 `AB_manifest.json` 内含原始素材 SHA-256、帧号、原始画布大小、裁剪偏移、各对照输入一致性证明，以及差异数量/强度。图片保留透明 Alpha；拼接图使用棋盘底色，只用于预览。

注意：这是**单个原始 SHP 帧+指定 PAL**的可复现色彩/放大效果测试。只有用户提供对应的 `.SHP`、`.PAL` 才能得到真实单位素材差异。它不包含 VXL/TMP、玩家色 remap、运行时 `CustomPalette`、`LightConvert`、现场光照与动画叠加；也不等同于同一游戏现场全视口的双版本截图。不能据此宣称完整游戏视觉等价或联机已验证。

CI 在 Linux/Windows 使用不含版权素材的构造 SHP 自动测试 RLE 行、透明索引、色盘满量程、不同分辨率、PNG CRC 和同帧记录，并提供合成预览图 Artifacts。用户实战素材的校验可用同一命令重现，不需要改游戏代码段。

