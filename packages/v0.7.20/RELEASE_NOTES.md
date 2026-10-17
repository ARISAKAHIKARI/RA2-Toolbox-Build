## RA2 Toolbox v0.7.20

### 更新
- 单位统计：图标和调色板支持磁盘缓存，按游戏路径及资源指纹隔离；后台读写缓存，复用绘图位图。
- 自动倒矿：矿车、矿场类型可分别配置；保留自动识别，可启用自定义 ID 列表。执行前重新检查配置及原版 Dock 兼容性。
- 自动维修：新增“允许维修不可维修的建筑”，默认关闭；筛选及实际提交时均校验原版 ClickRepairable。开关同样适用于维修快捷键。
- 配置即时生效并自动保存。保持单个 GameRuntime.dll 和统一游戏指令入口，无新增功能级 hook。

### 下载与源码
上传的唯一附件 `RA2-Toolbox-v0.7.20-Windows.zip` 是完整运行包，包含 EXE、DLL、资源和运行依赖，不含项目源码。完整解压后运行 RA2Toolbox.exe 或 Start-Message.cmd。

源码已单独提交：[浏览 v0.7.20 源码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/v0.7.20/source)。[完整开发包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/v0.7.20/packages/v0.7.20/development.zip) 保存在仓库中，包含构建脚本、架构说明和测试记录。

运行包 SHA256：`78b92e48d4e9c27be802f52d401f56df10975377def3214e98faa51b9565f015`。

### 验证范围
670 项 x86 DLL 模型用例、56 个便携测试程序、23 项源代码/UI 检查、2 项模拟 Win32 读取测试通过。UI 几何检查覆盖 189 个控件与 2172 项可达性检查；EXE/DLL 均已编译，文件版本为 0.7.20.0。

这些结果不替代真实游戏联机验证。本版尚未完成 Windows 游戏实机、平台联机、屏幕捕获和实际帧率/延迟测试，发布为预发布版本。科技全开、随处建造、敌方建筑变卖保持停用；地图全开仍是默认关闭、需主动许可的实验性功能。
