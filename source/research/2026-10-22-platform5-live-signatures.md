# 平台实机回传：Hook 已回调，启动校验拒绝

问题：1.0.34 在另一平台仍表现为只读功能正常、操作功能无效。用户回传 `Runtime(3).log` 与任务管理器截图。

## 证据

- 用户日志 SHA-256：`896a50e84f397fa37ef088ec3b9e9a5faee9e454a7bc482593283192f30845b0`。仅分析其中 v1.0.34 段，避免混用早期版本记录。脱敏摘录见 `source/verification/platform5-live/runtime3-evidence.json`；不提交原始日志、用户路径或游戏文件。
- 用户提供的磁盘样本 SHA-256：`ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`，PE32，基址 `0x400000`，映像大小 `0x794000`。此样本的 21 项启动签名均匹配；不能由此推定运行时仍匹配。
- 日志 1177/1200/1238 行记录 hook-installed，后续 connected 的 callbackTid 与 expectedTid 一致。PID 13604 回调从 45 增长到 570；PID 7276 也有回调，达到 234 次。DLL 入口、消息回调和 IPC 已工作。
- 日志 1180 等行 `status=-1`，`postError=0 runtimeError=0 faultSerial=0`。约八秒后宿主因未初始化释放连接；释放后的零回调不能反证此前没有注入。
- 日志实际进程名为 `gamemd-spawn.exe`。截图随机字符属于显示/窗口标题；本次证据不支持“随机实际文件名导致注入失败”。

## 源码对应与结论

`source/assist/source/bridge.cpp` 的首次 `signatures()` 调用使用 `ReadProcessMemory(GetCurrentProcess(), ...)` 比较 `source/assist/source/signatures.h` 中的 21 项签名。读取失败/短读或 `memcmp` 不等都会返回 false，并发布 `status=-1`。这是 Assist 初始化前的整体门槛。`checked` 使结果缓存，不会在后续 Tick 自动重新校验；当前日志不能区分读取失败与字节不匹配。

这次已定位到启动校验拒绝，而非 Hook 安装失败。未证明具体哪处签名不匹配，也未证明平台拦截了进程内读取。日志另有多处 E9 改写，但这些并不等于 21 项校验中的失败地址。没有据此放宽兼容检查或修改游戏内函数调用。

## 下一步采集

RA2MasterProbe 0.2.1 增加模式 3 / `--platform`：仅查询和读内存，支持 gamemd-spawn.exe，也可明确选择其他名称的 PID。保留完整主映像、逐字节可读/双读稳定掩码、21 项预期和实读指令、入口 E9 目标区域（各至多 64 KiB）、完整模块列表、窗口与原生 HWND `0xB73550` 的 PID/TID、受限私有可执行内存。原修改大师四阶段模式保留。

外部采集不是原子快照，也不复现 Runtime 内部的自读 API 路径。如果外部 21 项均匹配，仍需追查首次校验的暂态差异或内部读取失败。若发现入口跳转，需要继续分析跳转后语义与 ABI，不能仅凭 E9 放行。

## 验证范围

工具已构建为 Windows x64 独立 EXE，带 0.2.1.0 版本资源；比较逻辑测试覆盖匹配、变化、不可读掩码、地址边界和向后 rel32 跳转。差分算法原有测试继续通过。PE 导入和构建记录见 `source/verification/platform5-live/probe021-build.json`。这些不是 Windows 实机流程或平台功能修复证明；等待本次单次采集回传。Toolbox 版本仍为 1.0.34。
