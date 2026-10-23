# pre.9 快捷键通信修复验证

[根因与修复](research/2026-10-24-hotkey-mapping-137-pre9.md)。仅修复工具箱通信名称和本地热键排队，没有新增游戏地址/Hook/事件或修改 Shared 布局。

`verification/review-137-pre9/`：

- mapping-pre8-before：真实旧 EXE 的创建名称与 DLL 不一致，4种模式稳定断开。
- mapping-lbw / mapping-zhanwang：真实新 EXE 前缀提供给 Win32 映射替身，编译 DLL 的命令桥身份、指针指令与普通命令回执通过，各4案。
- hotkey-host：未连接/过期连接、140次按键不误报满、命令/选兵 ACK 等待时128槽队列清理、恢复、组合和双击。
- hotkey-routes：FIFO、对局/期限、满队列过期回收及原有鼠标路由。
- pe-verify：EXE/DLL实际导出协议和映射前缀一致、PE32与依赖检查通过。

以上为编译二进制及生产 Host 分派器受控执行，不声称执行了完整 Windows Host 主循环或双机联机。安装需完全退出游戏和工具，再同时替换两文件，重新启动后复测。
