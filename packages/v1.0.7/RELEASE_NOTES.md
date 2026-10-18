# RA2 Toolbox 1.0.7 测试版

整体审视 1.0.6 后修复：

- 逃生：队列满或发送额度用完时仍观察部署动画/移动，避免漏掉中断后的重试。首次部署仍按乘员独立处理。
- 实验功能：菜单、对话框或战术视图未就绪时立即回落原版；停止桥接后在有效游戏上下文中请求一次遮罩重绘。
- UI：修复科技目录和新增风险说明重叠；绘制测试状态加入日志。
- 截图诊断：更新协议 45 及请求时刻偏移。

未增加 Hook 位置、私有入口或 OutList 改写。智能卸载、科技全开、敌方建筑变卖仍停用；原生飞机路径点和任意建造仍默认关闭、标记联机危险。

验证：624 项实际 DLL/原版构造器模拟用例（含 54 项双端按帧模型）、62 个 C++ 测试程序、ASan/UBSan 压力测试、38 项入口 ABI/原版绘制调用者检查，以及 212 个控件/12 种窗口大小的布局检查通过。没有真实 Windows 游戏、LBW、双机平台联机、像素截图或端到端延迟验证，不承诺联机安全。

[整体审视报告](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/main/source/REVIEW-1.0.7.md) · [源码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/main/source) · [汇总记录](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/main/source/REVIEW_VERIFICATION.json)

下载 `Windows.zip` 为完整运行包；已有 1.0.2 或更新完整目录可使用 `Update.zip`。升级前退出游戏、关闭工具，配套替换 EXE/DLL；保留配置与图标缓存。附加文件只有运行包，源码和测试在仓库。
