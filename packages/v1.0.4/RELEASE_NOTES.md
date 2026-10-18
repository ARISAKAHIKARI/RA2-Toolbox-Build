新增两个默认关闭的实验开关，均已列入战术助手的 **Hook 修改游戏判断清单** 并标注 **联机危险**：

- **任意建造**：修改 YRpp `PassesProximityCheck` 的基地距离/邻接判断，保留地形和占用检查；不修改 Prerequisite。
- **飞机原生路径点**：接通原生路径点资格和到点续行。原版类型判断不支持飞机，因此不能归为“仅构造事件”的功能，也列入危险清单。使用游戏路径点键（默认 Z）。

所有补丁由同一个 GameRuntime 管理，严格限制三个位置；回调复用绑定游戏线程租约。没有恢复旧帧调度、CS、侧栏或 OutList 拦截。开启原生路径点会暂停并清理旧外部路径队列，避免重复下令；冲突或签名不支持时显示失败，不覆盖其他插件。

通过构建、PE32/ABI/依赖检查、架构边界检查、原始游戏字节校对及 12 个实际编译回调的 x86 仿真用例。**尚未执行 Windows 游戏实测或真实双机联机验证**。飞机采用原版到达目标格判断，未包含平滑转弯、水面飞行等其它扩展补丁，特殊 MOD 表现需确认。统一管理不等于联机安全。

下载：

- `Windows.zip`：完整运行包，含运行所需资源，可直接解压使用。
- `Update.zip`：EXE/DLL 更新包，覆盖已有 1.0.2 或更新完整目录。

升级前关闭工具并退出游戏，防止进程继续使用固定驻留的旧 DLL。

Release 附件为运行包；源码及详细清单分别位于 [source](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/v1.0.4/source) 和 [Hook 修改游戏判断清单](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/v1.0.4/source/HOOK_JUDGMENT_CHANGES.md)。本版基于 1.0.3，未宣称完成此前列出的其它未完成项。
