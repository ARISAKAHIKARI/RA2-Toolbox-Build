# RA2 Toolbox

用于《红色警戒2：尤里的复仇》的 Windows 工具箱，整合高刷鼠标、单位统计、战术控制和战场信息，并提供独立的小地图与单位跟踪窗口。

## 功能

| 面板 | 主要功能 |
| --- | --- |
| 对局工具 | 键鼠与选兵、战术操作、进驻与装卸、自动功能、生产 |
| 显示 | 鼠标、渲染、监控视图 |
| 高级 | 游戏规则、实验功能、调试 |

## 环境要求

- Windows 10／11，目标游戏为 32 位《尤里的复仇》进程。
- x64 控制台需要 Microsoft Edge WebView2 Evergreen Runtime；请完整解压配套的 `WebView2Loader.dll`。
- 原版主要适配 YR 1.001；可识别 `gamemd.exe`、`gamemd-spawn.exe` 和 `gamemd-ares.exe` 等进程。扩展版本的功能可用性取决于实际游戏映像。
- 工具箱目录需要可写，用于保存设置、缓存和日志。

## 下载与更新

正式版下载见 [GitHub Latest](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/latest)。本分支目标版本为 1.0.53.2（测试版）。首次安装下载 Full 完整包，已有安装下载 Update 更新包；退出游戏和工具箱后成套更新，保留已有配置。

Full 保留主程序、游戏 Runtime、Broker、鼠标原始素材和三套主题，不包含生成后的插帧或预览页面。默认图案从当前游戏读取原图和色盘，实际遇到缺失的动画帧对时在低优先级后台生成，并按素材内容保存本地缓存；已有效的缓存直接复用。Python、NumPy、SciPy、OpenCV 仅在生成时缺失才从固定公网来源下载，并验证 SHA256。正常启动和原帧显示不需要下载。

发布页提供独立的 Dependencies 包，包含 Python 科学依赖和原始鼠标素材，供离线安装；已有依赖可只下载 Update。

1.0.53 以 PR #96 的 `28fc384f` 为基线，重点改进世界采样、渲染资源重试和显示更新。详见 [1.0.53 发布说明](packages/v1.0.53/RELEASE_NOTES.md)。1.0.53.1 增加预定堆叠指定项取消、鼠标初始化重试退避与前台切换诊断，详见 [1.0.53.1 发布说明](packages/v1.0.53.1/RELEASE_NOTES.md)。1.0.53.2 增加仅单机的扩展扳手与战车变卖，详见 [测试版说明](packages/v1.0.53.2/RELEASE_NOTES.md)。真实游戏和联机平台表现仍待实机验证。

## 设置

界面分为对局工具、显示和高级三个导航分组；其中鼠标、渲染、监控视图，键鼠与选兵、战术操作、进驻与装卸、自动功能、生产，以及游戏规则、实验功能、调试均为独立页面。快捷键和鼠标键在对应功能中配置，修改后即时保存。
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

## 参与贡献

欢迎提交问题或改进。反馈画面问题时，请注明工具箱版本、游戏／平台版本、所用功能，并附同一场景的截图或日志。

## 鸣谢

感谢 [YRpp](https://github.com/Phobos-developers/YRpp)、[Phobos](https://github.com/Phobos-developers/Phobos)、[Ares](https://github.com/Ares-Developers/Ares)、[ccmaps-net](https://github.com/zzattack/ccmaps-net) 和 [FA2sp／HDM Edition](https://github.com/handama/FA2sp) 提供的接口资料、资源格式和渲染参考。

具体用途、原作者和第三方许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。

## 许可

本项目源码以 **GNU General Public License v3.0** 分发，全文见 [LICENSE-GPL](LICENSE-GPL)。第三方代码和资料保留各自的版权及许可声明。游戏程序和美术资源归原权利人所有。
