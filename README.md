# RA2 Toolbox

当前版本：**v0.7.20**（文件版本 0.7.20.0，Assist 协议 40）。

- [下载 Windows 运行包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v0.7.20)：完整解压后运行 `RA2Toolbox.exe` 或 `Start-Message.cmd`。
- [当前完整源码](source/)：源码已展开，可直接浏览和比较提交。
- [完整开发包](packages/v0.7.20/development.zip)：包含源码、构建脚本、文档、测试记录及第三方资源源码归档。
- [架构和功能说明](docs/FULL-0.7-Architecture-and-Features.html)
- [测试报告](docs/TEST-REPORT.html)与[验证记录](verification/)
- [更新说明](packages/v0.7.20/RELEASE_NOTES.md)与[发布校验清单](packages/v0.7.20/manifest.json)

Release 上传附件只包含 EXE、单个 GameRuntime.dll、资源及运行依赖，不包含项目开发源码或测试报告。engine/runtime 下的 Python 文件属于运行依赖，请勿删除。GitHub 自动提供的 Source code 下载是仓库快照，并非额外上传的 Release 附件。

0.7.20 增加单位图标和调色板磁盘缓存、可配置矿车/矿场类型，以及默认关闭的“允许维修不可维修的建筑”开关。设置即时生效并自动保存，游戏指令继续统一经过 GameRuntime。

验证：670 项实际 x86 DLL 模型用例、56 个便携测试程序、23 项源代码/UI 检查、2 项模拟 Win32 读取测试通过。尚未完成本版 Windows 游戏实机、屏幕捕获、平台联机和真实延迟验证，因此标记为预发布版本。

构建入口：`source/build-zig.sh` / `source/BUILD_ZIG.cmd`。开发包和运行包应解压到同一目录，以提供完整资源和运行依赖；所需编译工具请按脚本准备。仓库内历史 payload/release 目录用于资源重建，当前代码以 source/ 为准。

发布工作流校验展开源码与开发包一致，逐文件核对资源与二进制 SHA256，上传后重新下载验证运行包，再公开该版本。仓库访问权限保持不变。
