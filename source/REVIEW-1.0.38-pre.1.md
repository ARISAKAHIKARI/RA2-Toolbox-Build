# 1.0.38-pre.1 验证范围

本版基于私有仓库 `a46228d99c6376accb7d0cba74b412ad57856a15`。使用 Zig 0.13.0 的现有 build-zig.sh 构建 PE32 EXE/DLL，文件版本 1.0.38.1，协议 70。

## 自动检查

- verify_windows_outputs.py：真实输出的版本、架构、Runtime/Assist 导出、共享结构和映射名称、v6 manifest 及 DLL 依赖。
- direct_modes.cpp：7 个固定代码站点与上下文、原值/持有关系/拒绝冲突、科技完整字写入及身份变化恢复。
- command_delivery.cpp：会话与过期队列、清单哈希随意图保留，清单重排改变哈希。
- auto_settings_ui_test.py、suite_display_test.py、cursor_settings_ui_test.py、persistent_ui_source_test.py：真实设置处理器、直接模式和加强持久化、旧设置强制收敛到仅光标/100%缩放。
- ui_geometry_test.py：406 个控件，12 种视口尺寸，4,704 次可达性检查，三页无重叠。
- audit_architecture.py：Host/Runtime 边界，所有代码补丁由 GameEntryManager 唯一写入。

## 实际 Runtime 受控执行

报告位于 `verification/review-138-pre1/`，各 JSON 记录所测 DLL 和输入测试件的 SHA-256。

- direct-runtime.json：四种会话枚举下的直接/受控切换及停止恢复；地图/建造外部补丁拒绝；科技加强关闭、退出、第三方修改、换局和过期清单拒绝；旧共享设置不能启动高刷/缩放。
- controlled-tech.json：受控与经典兼容模式原生 FindFactory、作用范围、生产报文、建造上限、侧栏容量、冲突回滚及侧栏保留。
- direct-tech.json：四个直接科技枚举入口、栈/寄存器、移除路径、模式切换恢复和既有反例。

这些执行采用真实编译 DLL、从用户 clean 内存映像重建的私有 PE、合成世界及 Win32/部分引擎替身。会话值 0/3/4/5 不等于四次实机或双机测试。原游戏 EXE、内存和堆转储不发布。

## 待实机核对

Windows 原生控件的灰色外观；地图全开的实际视觉和关闭后的探索状态；21 个隐藏类型的工厂/侧栏/生产/完整行为；双方原版接收与长时间联机同步。步兵强行进驻仍等待只放宽下令入口即可完整执行的证据。
