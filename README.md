# RA2 Toolbox

用于《红色警戒2：尤里的复仇》的 Windows 工具箱，整合高刷光标、单位统计、战术控制和战场信息，并提供独立的小地图与单位跟踪窗口。

## 功能

| 面板 | 主要功能 |
| --- | --- |
| 视频 | 高刷光标、渲染升级、显示缩放；监控面板、副相机、小地图 |
| 控制 | 快捷键、快捷选兵、鼠标与操作方式 |
| 自动功能 | 自动装车、维修、倒矿、智能进驻、队形与规划 |
| 游戏规则 | 科技全开等修改生产与规则的功能 |
| 实验性功能 | 迷雾素材替换、扩展功能及实验开关 |

小地图、单位统计和小窗跟踪支持独立的第二显示器显示开关；副屏自动选择游戏屏幕之外的显示器。副屏窗口可以直接拖动，小地图可拖动角落调整大小，位置和地图宽度各自保存。小地图缩放范围为 5%～200%，默认 10%；实景模式以原生投影尺寸为 100% 基准。

建造栏的原生名称、价格提示框可在价格下显示建造时间；建造中同时显示剩余时间，供电、生产倍率、暂停与缺钱会影响显示。开关位于视频的监控设置，默认开启。

各功能可独立启用。信息显示支持跟随游戏所在显示器的刷新率，也可手动设置；设置修改后自动保存。

## 环境要求

- Windows 10／11，目标游戏为 32 位《尤里的复仇》进程。
- x64 控制台需要 Microsoft Edge WebView2 Evergreen Runtime；请完整解压配套的 `WebView2Loader.dll`。
- 原版主要适配 YR 1.001；可识别 `gamemd.exe`、`gamemd-spawn.exe` 和 `gamemd-ares.exe` 等进程。扩展版本的功能可用性取决于实际游戏映像。
- 工具箱目录需要可写，用于保存设置、缓存和日志。

## 下载与更新

从[发布页面](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases)下载最新更新包。

## 设置

界面分为视频、控制、自动功能、游戏规则、实验性功能五类，取消原三大板块总开关。快捷键和鼠标键在对应功能中配置，修改后即时保存。

“控制全部”默认使用不改游戏指令的硬件模式，最少占 1 槽。科技全开参数占 1 槽，精确科技加强的 -1 检查占 1 槽；“按等级 1 比较玩家科技等级”独立开关默认关闭，开启另加 1 槽。三项默认组合共 3 槽，启用额外比较共 4 槽。界面显示主线程总计 4 槽、本工具占用、其他占用和空闲；检查未完成或槽位不足时禁止新开启和切换到硬件实现，运行端再次事务校验，不覆盖其他工具，不自动回退代码补丁。实际联机效果待验证。

参数组可单独展开或收起，主开关保持可见；错误状态会自动展开。`Ctrl+F` 搜索当前页，`Esc` 清除搜索；视频页签支持方向键和 `Home`／`End` 切换。右上角显示正在编辑、应用、保存或保存失败的状态。

配置保存在程序目录中的 `Toolbox.ini`、`Assist.ini` 和 `settings.ini`。更新时保留这些文件即可沿用已有设置；首次运行会建立默认配置。

## 编译

需要 **Zig 0.13.0** 和 **Python 3**。源代码按 C++17 构建；支持 x86 主程序，或 x64 主程序搭配 x86 Runtime／broker。

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

x64 混合架构版本使用 `source/build-zig-x64.sh`，默认构建 WebView2 控制台；输出目录通过 `RA2_OUTPUT_DIR` 指定，包含 `RA2Toolbox.exe`、`GameRuntime.dll`、`GameRuntimeHost32.exe`、`WebView2Loader.dll` 和配套许可说明。x86 构建结果为 `source/RA2Toolbox.exe` 和 `source/GameRuntime.dll`。Windows 构建脚本会在完成后校验这两个文件；发布包使用同一次验证的配套产物。

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

主视图、副相机、实景小地图共用世界快照、绘制器和缩放前素材结果。普通下令线、选框、光标、消息、计时器、主要工厂和集结线仅在主视图处理。Hotfix14 使用游戏当前字体与本地化文本独立绘制后四类，删除对应的宽松原生保留矩形。Hotfix15 增加多帧 HVA、独立碎片、火焰／像素粒子和共享光束／波形／AlphaShape 绘制路径；队形预览复用公共素材缓存与 CPU／GPU 画布，按耗时轮转加载。动画／特效的已接入类型和剩余缺口见[覆盖表](source/ANIMATION_RENDER_COVERAGE.md)。

## 参与贡献

欢迎提交问题或改进。反馈画面问题时，请注明工具箱版本、游戏／平台版本、所用功能，并附同一场景的截图或日志。

[研究索引](source/research/INDEX.md)记录已核实的引擎行为和未决问题；[历史说明](docs/CHANGELOG-archive.md)保留原 README 中的版本记录。

## 鸣谢

感谢 [YRpp](https://github.com/Phobos-developers/YRpp)、[Phobos](https://github.com/Phobos-developers/Phobos)、[Ares](https://github.com/Ares-Developers/Ares)、[ccmaps-net](https://github.com/zzattack/ccmaps-net) 和 [FA2sp／HDM Edition](https://github.com/handama/FA2sp) 提供的接口资料、资源格式和渲染参考。

具体用途、原作者和第三方许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。

## 许可

本项目源码以 **GNU General Public License v3.0** 分发，全文见 [LICENSE-GPL](LICENSE-GPL)。第三方代码和资料保留各自的版权及许可声明。游戏程序和美术资源归原权利人所有。
