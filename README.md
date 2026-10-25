# RA2 Toolbox

用于《红色警戒2：尤里的复仇》的 Windows 工具箱，整合高刷光标、单位统计、战术控制和战场信息，并提供独立的小地图与单位跟踪窗口。

## 功能

| 面板 | 主要功能 |
| --- | --- |
| 画面设置 | 高刷光标、显示器刷新率自适应、显示缩放和布局 |
| 单位统计 | 玩家单位统计、类型图标和分栏显示 |
| 战术控制 | 自动装车、建筑维修、矿车倒矿、智能进驻、快捷选兵、队形和路径点 |
| 战场信息 | 载员与危险乘员提示、铁幕和超时空计时、生产提醒、示意／实景小地图、最多三个单位跟踪窗口 |
| 实验功能 | 地图显示模式、科技生产方案和扩展建筑指令 |

小地图、单位统计和小窗跟踪支持独立的第二显示器显示开关；副屏自动选择游戏屏幕之外的显示器。副屏窗口可以直接拖动，小地图可拖动角落调整大小，位置和地图宽度各自保存。小地图缩放范围为 5%～200%，默认 10%；实景模式以原生投影尺寸为 100% 基准。

各功能可独立启用。信息显示支持跟随游戏所在显示器的刷新率，也可手动设置；设置修改后自动保存。

## 环境要求

- Windows 10／11，目标游戏为 32 位《尤里的复仇》进程。
- 原版主要适配 YR 1.001；可识别 `gamemd.exe`、`gamemd-spawn.exe` 和 `gamemd-ares.exe` 等进程。扩展版本的功能可用性取决于实际游戏映像。
- 工具箱目录需要可写，用于保存设置、缓存和日志。

## 下载与更新

从[发布页面](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases)下载最新更新包。

## 设置

界面分为画面设置、单位统计、战术控制、战场信息、实验功能五个面板。快捷键和鼠标键在对应功能中配置，修改后即时保存。

配置保存在程序目录中的 `Toolbox.ini`、`Assist.ini` 和 `settings.ini`。更新时保留这些文件即可沿用已有设置；首次运行会建立默认配置。

## 编译

需要 **Zig 0.13.0** 和 **Python 3**。源代码按 C++17 构建，输出为 Windows x86。

Windows 命令提示符：

```bat
cd source
set ZIG_EXE=C:\Tools\zig-0.13.0\zig.exe
BUILD_ZIG.cmd
```

在 Linux／WSL 中交叉编译，从仓库根目录执行：

```bash
ZIG=/path/to/zig-0.13.0/zig bash source/build-zig.sh
python3 source/tools/verify_windows_outputs.py
```

构建结果为 `source/RA2Toolbox.exe` 和 `source/GameRuntime.dll`。Windows 构建脚本会在完成后校验这两个文件；发布包使用同一次验证的配套产物。

## 目录结构

| 目录 | 内容 |
| --- | --- |
| `source/suite` | 工具箱主窗口和统一设置界面 |
| `source/cursor`、`source/unit_monitor`、`source/assist` | 光标、统计和战术／信息功能 |
| `source/external_reader` | 统一读取、资源解析和外部渲染 |
| `source/game_runtime`、`source/shared` | 单一 Runtime、公共协议和入口管理 |
| `source/tools` | 构建、校验和打包工具 |
| `source/research`、`source/verification` | 研究记录和验证材料 |
| `packages`、`docs` | 各版本说明和项目文档 |

## 参与贡献

欢迎提交问题或改进。反馈画面问题时，请注明工具箱版本、游戏／平台版本、所用功能，并附同一场景的截图或日志。

[研究索引](source/research/INDEX.md)记录已核实的引擎行为和未决问题；[历史说明](docs/CHANGELOG-archive.md)保留原 README 中的版本记录。

## 鸣谢

感谢 [YRpp](https://github.com/Phobos-developers/YRpp)、[Phobos](https://github.com/Phobos-developers/Phobos)、[Ares](https://github.com/Ares-Developers/Ares)、[ccmaps-net](https://github.com/zzattack/ccmaps-net) 和 [FA2sp／HDM Edition](https://github.com/handama/FA2sp) 提供的接口资料、资源格式和渲染参考。

具体用途、原作者和第三方许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。

## 许可

本项目源码以 **GNU General Public License v3.0** 分发，全文见 [LICENSE-GPL](LICENSE-GPL)。第三方代码和资料保留各自的版权及许可声明。游戏程序和美术资源归原权利人所有。
