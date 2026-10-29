# 64 位外部宿主与 32 位游戏 Runtime 的边界

## 问题与范围

用户允许工具箱改为 64 位，以继续 GPU 与 DLSS 5 接入。需要保留游戏的 32 位指针、共享协议、统一命令入口和不改游戏代码段的基线。本项调查针对工具箱源码、实际 Windows 交叉编译产物和受控窗口，不新增游戏地址、函数 ABI 或游戏可执行版本假设；游戏 PE SHA 不适用。

调查前查询了研究索引和持久笔记 v81／98条。新结果记录于本文件，最终与同一源码的验证记录写回原笔记。

## 分层与固定协议

`RA2Toolbox.exe` 的 UI、读取、计算与渲染改用 x64。`GameRuntime.dll` 继续 x86。原 `host_game_input.h::runtimeModule()` 在宿主 LoadLibrary 后取得 DLL Hook 地址；x64 进程不能直接沿用这条 x86 模块路径。

新增 `GameRuntimeHost32.exe`，通过匿名共享映射及指定继承句柄接收安装／卸载请求，在 x86 进程加载和验证同目录 Runtime，安装原有 WH_GETMESSAGE 与 WH_CALLWNDPROCRET 回调。它不接管游戏命令、刷新节拍或输入计算。Runtime Control 的 owner、唤醒消息中的控制器 PID、Assist 映射和心跳仍来自真实 x64 宿主。

新 Broker Packet 独立版本1，148字节。检查宿主进程句柄、目标 PID／线程／窗口、x86 架构、Runtime Control 映射身份和 DLL 导出协议。token 与 broker generation 防止旧连接卸载新 Hook；宿主退出、目标退出、显式卸载和崩溃恢复都有清理路径。健康帧只读进程状态，不进行同步 broker RPC。

现有协议保持：

- Assist：80，Shared 4,169,840字节。
- Runtime：4，Control 176字节。
- Entry Manager：3，Request 108字节。
- Neural Frame：独立版本1，Header 256字节，不含游戏指针或 SDK 对象。

`verify_crossbit_abi.py` 用 Zig 的真实 Windows x86／x64 目标编译常量表，比较632项版本、尺寸、对齐和成员偏移。全部相同，宿主指针尺寸分别为4／8字节。游戏对象字段仍固定为32位；HostWindow 发布前检查 Win32 handle 的往返身份。

## 已排除的误判

曾误用 tracked `WINDOWS_BUILD_ARTIFACTS.json` 中历史 Shared 4,165,136 字节，与新 x64 的4,169,840比较，得到4704字节差异。直接读取 Fix3 CI、当前x86和x64实际PE导出后，均为80／4,169,840。没有据此改 packing、加字段或提升协议。最终包依据实际产物导出和新生成清单，历史清单不能代替当前二进制。

## 证据与边界

- `tools/verify_crossbit_abi.py`：真实目标编译；不执行生成 PE。
- `tools/pe32_image.py`、`verify_windows_outputs.py`：默认仍严格 x86；显式 x64 模式分别验证 GUI、Runtime、Broker，检查清单、版本、导出和依赖。
- `assist/tests/mixed_architecture_verifier.py`：实际 PE 的架构、导出、协议、命名空间、manifest、依赖篡改反例。
- `assist/tests/host_input_test.py`：旧 x86 管理器行为回归。
- `assist/tests/runtime_broker_*windows.*`：x64 控制器、x86 broker、x86 窗口与严格元数据的测试 DLL；两类 Hook 分别使用 posted／sent 消息验证，并检查卸载、错误身份、重启和宿主死亡。

初始完整 x64 预构建已链接并通过实际三文件 PE 校验。真实 Windows 夹具、最终冻结构建与同提交 CI 结果单独附记；交叉编译和源码审查不等同于真实游戏、所有平台或联机验收。

隔离 Windows broker 验证 run `40822040010`、commit `123923f38b6d3d83539b3d0c05ce7a2da96d9f91` 已实际执行并通过：私有 IPC、错误身份拒绝、两类 Hook 回调及卸载、正常／崩溃重启、版本不匹配和宿主死亡清理。更早一次仅因 PowerShell 未对 `-Wl,--subsystem,windows` 加引号在编译前失败，未将其当成产品运行缺陷。最终主分支 CI 仍会重跑夹具。
