# 第三方声明

本项目感谢以下项目及其贡献者。

## 引擎与渲染资料

| 项目与归属 | 本项目中的用途 | 上游许可与声明 |
| --- | --- | --- |
| [YRpp — Phobos-developers 及贡献者](https://github.com/Phobos-developers/YRpp) | 引擎类、字段与接口名称的对照资料 | 保留所引用源文件的声明，参见[上游项目说明](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/README.md) |
| [Phobos — Phobos-developers 及贡献者](https://github.com/Phobos-developers/Phobos) | 引擎扩展行为与实现研究参考 | [GPL v3 原文](https://github.com/Phobos-developers/Phobos/blob/develop/LICENSE.md) |
| [Ares — All Ares Contributors](https://github.com/Ares-Developers/Ares) | 引擎扩展行为、模块检测与共存参考 | 保留 [上游原始声明](docs/licenses/Ares-LICENSE.txt)，包括其署名和广告鸣谢条款；[原始来源](https://github.com/Ares-Developers/Ares/blob/master/src/LICENSE) |
| [ccmaps-net — zzattack 及贡献者](https://github.com/zzattack/ccmaps-net) | 地图资源、法线／VPL 表及渲染选型参考 | [GPL v3 原文](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/COPYING) |
| [FA2sp／HDM Edition — handama 及贡献者](https://github.com/handama/FA2sp) | SHP 格式、剧场资源与光照实现参考 | 上游采用 **AGPL v3**，参见保留的 [原始许可](docs/licenses/FA2sp-LICENSE.txt) 和[固定版本来源](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/LICENSE)；本项目的 GPL v3 声明不替换该许可 |

引用到具体源文件、版本或算法的记录见 [研究索引](source/research/INDEX.md)。移植或修改上游代码时，原文件中的作者、版权、许可及变更说明应一并保留。

Ares 原始声明要求的鸣谢：

> This product includes software developed by the Ares Contributors.

## 文档结构参考

- [springy — A-small-C4](https://github.com/A-small-C4/springy)：项目简介、环境、构建、配置、目录和鸣谢的组织方式参考。它不是本项目的运行依赖或注入器。[上游 GPL v3](https://github.com/A-small-C4/springy/blob/main/LICENSE.md)。
- [VERA20K — YuriPlanet 及贡献者；ARISAKAHIKARI 仓库](https://github.com/ARISAKAHIKARI/vera20k)：项目目标、下载、设置、编译和贡献说明的组织方式参考。原项目的名称和版权保留其上游归属。[上游 GPL v3](https://github.com/ARISAKAHIKARI/vera20k/blob/main/LICENSE-GPL)。

## 构建与研究工具

- [Zig](https://ziglang.org/)：固定使用 0.13.0 编译 Windows x86 程序，使用其随附的 C／C++ 工具链。
- [Python](https://www.python.org/) 和 C++ 编译器：构建校验及回归测试工具。
- [Unicorn](https://github.com/unicorn-engine/unicorn) 与 [Capstone](https://github.com/capstone-engine/capstone)：用于原版指令的受控执行和反汇编研究，不是运行工具箱的必要组件。


## 游戏内容

Command & Conquer、Red Alert 及游戏内容的相关权利归 Electronic Arts 和其他原权利人所有。工具箱的 GPL v3 许可不覆盖游戏可执行文件、美术、声音或用户自己的 MOD 资源。
