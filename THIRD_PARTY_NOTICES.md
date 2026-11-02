# 第三方声明

本项目感谢以下项目及其贡献者。

## 引擎与渲染资料

| 项目与归属 | 本项目中的用途 | 上游许可与声明 |
| --- | --- | --- |
| [YRpp — Phobos-developers 及贡献者](https://github.com/Phobos-developers/YRpp) | 引擎类、字段与接口名称的对照资料 | 保留所引用源文件的声明，参见[上游项目说明](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/README.md) |
| [Phobos — Phobos-developers 及贡献者](https://github.com/Phobos-developers/Phobos) | 引擎扩展行为与实现研究参考 | [GPL v3 原文](https://github.com/Phobos-developers/Phobos/blob/develop/LICENSE.md) |
| [Ares — All Ares Contributors](https://github.com/Ares-Developers/Ares) | 引擎扩展行为、模块检测与共存参考 | 保留 [上游原始声明](docs/licenses/Ares-LICENSE.txt)，包括其署名和广告鸣谢条款；[原始来源](https://github.com/Ares-Developers/Ares/blob/master/src/LICENSE) |
| [ccmaps-net — zzattack 及贡献者](https://github.com/zzattack/ccmaps-net) | 地图资源、法线／VPL 表及渲染选型参考 | [GPL v3 原文](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/COPYING) |
| [FA2sp／HDM Edition — handama 及贡献者](https://github.com/handama/FA2sp) | SHP 格式、剧场资源与光照实现参考 | 上游采用 **AGPL v3**，参见保留的 [原始许可](docs/licenses/FA2sp-LICENSE.txt) 和[固定版本来源](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/LICENSE) |
| [Vera20k — YuriPlanet 及贡献者](https://github.com/YuriPlanet/vera20k) | Hotfix14–15 的 GAME.FNT、文字定位、集结线、HVA／碎片姿态、粒子、光束与波形规则参考与适配；不包含游戏素材 | **GPL v3**；固定提交 `0b6a073d0928453036c4a53d117bb7a2d8d573a7`，保留[上游许可](https://github.com/YuriPlanet/vera20k/blob/0b6a073d0928453036c4a53d117bb7a2d8d573a7/LICENSE-GPL)，本仓库同许可全文见 LICENSE-GPL |
| [Vera20k — YuriPlanet 及贡献者（Hotfix18–20）](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f) | 不可变素材／姿态图集、Terrain 阴影与 Z、有序 GPU 特效、保留世界实例／当前相机／战场裁切、小地图直接呈现、PIPS2 和矿车采集 SHP 的 C++／GLSL 适配；保留外部只读状态采样及现有 RGBA 策略 | **GPL v3**；修改边界见 [Hotfix18](source/research/2026-10-30-scene-hotfix18.md)、[Hotfix19](source/research/2026-10-30-world-pipeline-hotfix19.md)及 [Hotfix20](source/research/2026-10-30-world-ui-minimap-hotfix20.md)，原许可见 [LICENSE-GPL](https://github.com/ARISAKAHIKARI/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/LICENSE-GPL) |
| [Vera20k — YuriPlanet 及贡献者（Hotfix21）](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f) | 索引素材/现场色盘分离、只读Z区间裁减、RGB565原生混合、原尺寸GPU页合成、保守读写足迹依赖批次、PIPS载员/资源规则和限定正整数输入PC53/chop计数的C++/GLSL适配 | **GPL v3**；出处与修改边界见[总表](source/research/2026-10-30-vera-render-migration-map.md)和[实施记录](source/research/2026-10-30-vera-render-integration-hotfix21.md)；保留同上游提交的LICENSE-GPL；不包含游戏资源 |
| [Vera20k — YuriPlanet 及贡献者（Hotfix22）](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f) | PlanMgr图投影、共享前驱/回环、任务连接、先外边后内部的线/节点、阵营插值、鼠标任务徽标及后置高亮的C++只读适配 | **GPL v3**；`planning.rs`、`planning_lines.rs`、`surface_line.rs`、`native_surface_format.rs`，细节与边界见[Hotfix22](source/research/2026-10-31-native-planning-render-hotfix22.md)；不包含游戏资产 |
| [Vera20k — YuriPlanet 及贡献者（Hotfix23）](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f) | FoggedObject保存图、draw_state/sinking、Smudge、LineTrail/surface_line、beacon/radar事件、cell_bodies/bridges、building_rubble、pixel_fx与ChronoScreen的C++/GLSL适配；另读取原生PixelFX/三角表和Rubble资源 | **GPL v3**；保留LICENSE-GPL。修改、原生静态依据及非等价部分见[Hotfix23](source/research/2026-10-31-vera-scene-extras-hotfix23.md)；不含游戏素材或私有EXE数据 |
| [CnCNet yr-patches / YRpp — CnCNet 及贡献者](https://github.com/CnCNet/yr-patches/tree/0318b1f1bddcc9c664c50ebee820cde74a662f47) | Hotfix25 现场隐形补丁识别、原生字段与枚举核查；保留已确认规则的外部只读适配 | yr-patches **GPL v3**，许可全文见本仓库 LICENSE-GPL；YRpp 固定提交 `d9795546950fcc75a94325ffc83b44d16ec34ce2`，保留上游声明 |
| [Vera20k — YuriPlanet 及贡献者（Hotfix25）](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f) | tether 顺序、特殊 PIPS/PIPS2 和范围提示的 C++ 适配；保留已知数值/Z/AA 差异 | **GPL v3**；LICENSE-GPL；详情与固定来源见 Hotfix25 渲染研究记录 |
| [Phobos — Phobos-developers 及贡献者（Hotfix25）](https://github.com/Phobos-developers/Phobos/tree/f06797ec39f2b60b9fa5b6da69288c36d189b712) | 矿车工作中/总数、电量净余量、阈值和停电状态的外部只读显示；原生单位/奴隶矿车提供者 | **GPL v3**；保留 LICENSE-GPL 与固定源码路径。未复制 Phobos 扩展内存布局、未包含其 DLL 或游戏资源 |
| [Khronos Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/b379292b2ab6df5771ba9870d53cf8b2c9295daf) | 未修改的 C API 声明，含 Win32 外部显存共享；不捆绑驱动 | 保留 [MIT／Apache-2.0 声明及文件校验](source/third_party/vulkan/UPSTREAM.json) |
| [cnc-ddraw — FunkyFr3sh 及贡献者](https://github.com/FunkyFr3sh/cnc-ddraw) | 现有表面像素格式、调色板、透明键与呈现流程比对；未移植其代码 | 固定提交 `279a057ee7e1e4d584b56141c014ab01f3c6ee6d`；参见[上游许可](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/LICENSE) |
| [xBRZ — Zenju](https://sourceforge.net/projects/xbrz/files/xBRZ/xBRZ_1.9.zip/download) | SHP／TMP 与原生画布当前颜色的像素边缘放大，使用未修改的 xBRZ 1.9 C++ 实现 | **GPL v3**；保留[上游许可](source/third_party/xbrz/License.txt)、文件内版权和特殊链接例外，来源与 SHA-256 见[集成说明](source/third_party/xbrz/README.ra2toolbox.md) |

引用到具体源文件、版本或算法的记录见 [研究索引](source/research/INDEX.md)。移植或修改上游代码时，原文件中的作者、版权、许可及变更说明应一并保留。

Ares 原始声明要求的鸣谢：

> This product includes software developed by the Ares Contributors.


## 构建与研究工具

- [Zig](https://ziglang.org/)：固定使用 0.13.0 编译 Windows x86 程序，使用其随附的 C／C++ 工具链。
- [Python](https://www.python.org/) 和 C++ 编译器：构建校验及回归测试工具。
- [Unicorn](https://github.com/unicorn-engine/unicorn) 与 [Capstone](https://github.com/capstone-engine/capstone)：用于原版指令的受控执行和反汇编研究。


## 游戏内容

Command & Conquer、Red Alert 及游戏内容的相关权利归 Electronic Arts 和其他原权利人所有。

Hotfix26：Vera20K `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`（YuriPlanet及贡献者，GPL-3.0）的 native_z、foot_depth、palette_light、terrain/overlay/building_zshape、surface_line/surface_line_aa、surface_line_cubic_table.bin、laser、radiation_beam、wave_geometry、combat_light/building_light、sim/world/spotlight 适配到C++/GLSL。`native_line_cubic.h`保留257项公开原生cubic数据；没有提交游戏EXE或游戏素材。字段参考CnCNet/YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2`。修改及验证边界见[Hotfix26](source/research/2026-10-31-native-surface-hotfix26.md)。


2026-11-02 行为参考：MagicShiba / MO_ExperienceEnhancements，固定提交 `33ed558bd38f36b5012eddcc9c0fb109cc68e38a`，https://github.com/MagicShiba/MO_ExperienceEnhancements/tree/33ed558bd38f36b5012eddcc9c0fb109cc68e38a 。本轮在既有 GameRuntime、读者、覆盖层和托管补丁框架内重新实现对应操作与视野行为，未将上游工程或二进制作为依赖导入。对应功能、修改范围与验证边界见 docs/MO-CONTROLS.md 及2026-11-02研究笔记。
