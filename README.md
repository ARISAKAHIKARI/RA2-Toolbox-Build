# RA2 Toolbox

当前版本：**v0.7.19**（文件版本 0.7.19.0）。

[下载 v0.7.19 Windows 运行包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v0.7.19)

完整解压后运行 `RA2Toolbox.exe` 或 `Start-Message.cmd`。包含 EXE、单个 `GameRuntime.dll`、完整资源和离线预处理运行依赖；Release 附件不含项目开发源码或测试报告。

- [更新说明与验证范围](packages/v0.7.19/RELEASE_NOTES.md)
- [逐文件发布清单](packages/v0.7.19/manifest.json)
- [仓库内开发材料](packages/v0.7.19/development.zip)：当前源码、构建脚本、架构说明、测试证据及第三方资源源码归档，不上传为 Release 附件。

本版将地图全开设为须主动许可的实验性功能，增加独立覆盖层显示方式与隐藏原因日志。使用单个 GameRuntime.dll 统一提交游戏指令。科技全开、随处建造、敌方建筑变卖保持停用。

644 项 DLL 模型用例与便携测试通过，尚未完成 v0.7.19 的 Windows 实机及平台联机验证，发布为预发布版本。

`publish-v0719.yml` 以逐文件大小和 SHA256 校验后的资源及当前二进制组装运行包，上传后重新下载校验再发布。历史发布和构建工作流保留。
