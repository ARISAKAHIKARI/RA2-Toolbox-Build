# RA2 Toolbox Build

当前版本：**v0.6.2 Full BuildFix4（文件版本 0.6.2.4）**。

[下载 Full 发布包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v0.6.2-buildfix4)

完整解压后运行 `Start-Message.cmd`，无需重新编译。
Full 包含 EXE、三个桥接 DLL、完整资源、Windows x64 预处理环境、源码和检查报告。

- [独立修复源码包](payload/ra2-v062-Fix4-source.zip)
- [检查报告](release/v0.6.2-buildfix4/CHECK_REPORT_v0.6.2_BuildFix4.md)
- [发布校验清单](release/v0.6.2-buildfix4/manifest.json)

63 项便携回归及 PE32/协议/依赖静态检查通过；游戏和双人联机尚未实测。
Windows 复编由 `build-win32.yml` 执行；原交付 Full 的上传与重新下载校验由 `publish-full.yml` 执行。
