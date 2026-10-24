# Ares 与 spawn 共用进程发现检查（1.0.38-pre.2）

## 问题与结论

用户询问 gamemd-ares.exe 是否漏检。检查基线 8651d146cee81da691232cb081c0b0ba8d25b0a8：external_reader/source/process_discovery.h 的 isGameName 已显式包含 gamemd-ares.exe，并且和 gamemd-spawn.exe 一样不区分大小写。光标、统计和战术模块均使用 discovery::scan；因此未修改生产环境名称白名单。

该结论仅限发现入口。识别进程名称并不证明此 Ares/平台映像兼容命令桥；仍保留原有实际映像、字段和入口特征检查。

## 证据与范围

- 当前仓库过程和调用点检查；未新增游戏反汇编。
- 既有权威窗口字段 Game::hWnd = 0xB73550，32 位 HWND；resolveYrppWindow 使用 readFresh（可用时）并检查窗口所属 PID。
- source/tests/process_discovery_windows.cpp：复制自身测试程序为 Ares、spawn 和混合大小写名称，在真实 Windows API 上覆盖可见、最小化、隐藏、无窗口、窗口 PID 校验、首选 PID、两进程歧义和不相关程序拒绝。
- 测试子进程由测试程序创建和停止；不调用游戏引擎、不注入或修改其他进程。
- validate-v138-pre2.yml 在 Windows 2022 runner 执行此程序；实际结果以该提交 Actions 日志为准。
- 支持游戏映像 SHA-256：本次未新增或更改；没有用户实际 gamemd-ares.exe 样本可验证。测试映像是本项目独立生成的控制程序，不能作为游戏支持指纹。

## 排除与未决

排除“共用列表遗漏 gamemd-ares.exe”的假设。若用户机器仍无法连接，需根据日志区分候选进程/窗口、权限、窗口所有者与 Runtime 特征校验失败。没有真实 Ares 游戏画面、命令执行或联机验证，不据此放宽命令桥兼容性检查。
