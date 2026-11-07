# pre.11：游戏发现、追加检测名单与连接恢复

## 问题与现场证据

用户在 pre.10 报告“全部功能都用不了了”，随后明确主程序名为 `gameares.exe`，任务管理器描述为 `Main executable for Yuri's Revenge`。用户要求描述关键词模糊匹配、手动指定并记忆，并进一步明确：记忆应当**追加到检测名单**，其他识别规则继续有效，不能把未来启动锁定到唯一程序。

附件 `Runtime(20261107-051937).log` 的 SHA-256 为 `c8c7b6730b81d8cc17d9324fe98b690437b8c710f20744f73f770e046f4a6011`，73,264 字节、94 行。pre.10 Host 29052 在 13:14:55–13:15:40 有 9 条 health；回退 pre.9 Host 25632 在 13:16:23–13:19:10 有 33 条 health。42 条采样均为 `pid=0`、`subscription=0`、`diagnosticValid=0`，没有 `read-attached`、具体入口签名拒绝或注入失败事件。Host 控制定时循环持续输出，不支持已经持续卡死的解释。

旧日志 `enabled=0` 来自 `gameinput::health(0)` 返回的空 Runtime health，并非 Host 全局启用值；`diagnostic-busy-or-unavailable` 也涵盖根本没有订阅的情形。`detach pid=0` 不证明游戏退出。pre.9 中段六次空清理与暂停路径相符，但旧日志不能证明具体用户点击。

## 确认的缺口与拒绝的归因

旧共享名称列表遗漏 `gameares.exe` 和 `gamemdAres.exe`。未知名称只能通过一个历史磁盘样本的严格发现指纹；它不是所有普通 Ares/spawn 构建的通用身份。探测器又维护了另一份名称列表，导致不同入口的自动发现不一致。用户确认的名称确实落入这个缺口；当前附件未包含 EXE 或磁盘指纹，不能宣称对现场二进制进行了重放验证。

另有两个可独立复现的问题：旧 ID 431 文案始终为“暂停 / 连接”，但默认已启用时点击会反向暂停整个战术模块；真实连接/错误状态仅写入已隐藏的原生标签，`console_adapter::capture()` 没有填入 `Snapshot.status`，WebView 状态栏因而看不到失败原因。

本轮没有证据把故障归因为平台 Hook、运行组件签名拒绝、pre.9 线程恢复事务或旧 broker 退役等待。两个版本均未成功绑定目标，不能以零值 health 推断 Runtime 内部状态，也没有通过关闭所有入口校验来修复发现问题。

## 识别与检测名单

共享发现接受以下并行条件：常见游戏名称、程序描述或名称的关键词、用户追加的程序路径/精确名称、原有精确磁盘样本指纹。补充 `gameares.exe/.dat`、`gamemdAres.exe/.dat`、`gamemd-ares.dat`；探测器与正常模块使用同一识别规则。

默认关键词为 `Yuri Revenge`。关键词按空白分词、大小写不敏感，每个词都须出现在被搜索文本中；它是包含匹配，不是编辑距离纠错。界面搜索还可匹配完整路径，留空显示全部运行进程；空自动关键词不会放行全系统程序。描述来自只读 VERSIONINFO，按语言/代码页查询 `FileDescription`，兼容多个翻译表及常见缺少 Translation 的旧资源。依据微软 [VerQueryValueW](https://learn.microsoft.com/en-us/windows/win32/api/winver/nf-winver-verqueryvaluew) 与 [GetFileVersionInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winver/nf-winver-getfileversioninfow) 文档，不把窗口标题当成程序身份。

`Toolbox.ini` 的 `GameProcess` 节保存关键词和有界检测名单。添加路径或精确文件名是 OR 准入，按 Windows 路径分隔符/大小写去重，允许查看和移除。保存失败不会先更新内存并声称成功。程序当前 PID 与创建时间只作为本次明确选择的临时优先级，**不跨启动保存 PID**。清除本次指定保留检测名单；删除名单记录也不主动拆除已经建立的健康会话。

自动扫描的版本资源读取在有界后台任务中完成，owner 线程只取得身份与缓存结果。描述缓存绑定 PID、创建时间和路径，正负结果可重试；刷新同一身份时保留已知描述，PID 复用不能继承旧结果。UI 运行进程枚举也有独立只读 worker，关闭不等待其完成，迟到结果不会访问 HWND。上述设计避免新加的全进程版本资源读取阻塞主控制循环；原有精确指纹回退中的同步文件读取仍存在，不能声称所有 Windows/文件系统调用绝对无等待。

## 手动指定与三个模块的生命周期

调试页顶部新增“游戏进程连接”：关键词搜索/保存、运行进程列表/刷新、“连接并加入检测名单”、手填路径/名称、已记名单及删除、清除本次指定、连接与保存反馈。显示长标签适当省略，底层保存和身份复核仍使用完整字符串。普通 UI 更新每 250 ms 节流，命令结果及时刷新。

明确选择运行进程时先重新核验 PID、创建时间和路径，读取会话打开后再次核验已打开句柄的创建时间，阻止列表到连接之间的 PID 复用。已有 reader/退役 lease 的 `scan(preferredPid)` 重解窗口不会被新名单编辑破坏。

只有新的、仍活着的明确选择才切换目标。Assist 沿现有 detach/releaseBridge 退休路径取消旧目标队列；Cursor 先停止并恢复旧目标 Hook，再关闭 reader；Monitor 失效旧采样 generation、清理拖拽和主副屏呈现，再重新发现。普通关键词/名单编辑与清除临时优先均保留健康连接。Cursor 在多个候选时接受已经验证的明确选择，保留自动模式的歧义约束。

## 重连、状态与自动日志

ID 431 改为“重新连接游戏”：单向启用 Host 并使正常发现/重试到期。健康连接点击不会拆映射、重发未确认请求、清 ACK、清在途队列或打断手势；已有映射的故障锁存保留。没有映射时才清连接退避，随后仍经过原有 owner 存活、协议、退休确认与入口准入检查。实际成功由目标绑定/回调确认，按钮不直接宣称重连成功。

隐藏状态源通过 `RA2.Console.Status` 与稳定 `std::wstring` 指针传入 `Snapshot.status`；悬浮帮助不覆盖真实连接状态，清空状态会正确清除旧错误。自动日志新增 Host 启用、发现尝试次数、无目标/未订阅阶段、扫描候选/描述/名单/错误与耗时；发现失败和 OpenProcess 错误按变化及低频周期记录。拆连接记录明确原因，Runtime 启用字段命名为 `runtimeEnabled`，不再与 Host 开关混淆。 旧指纹接口仍返回布尔值，日志 `fingerprintMismatches` 表示该分支未准入，也可能包含读取失败，不能将此计数全部解释成已证实字节不匹配。

## 映像、ABI、验证与未决项

本轮没有新的游戏 EXE SHA-256；进程名称和 FileDescription 只用于发现，不证明游戏布局或任意 Hook 兼容。没有新增游戏函数地址、调用约定或字段偏移。原 `Game::hWnd=0xB73550` 只读窗口解析保持；Assist100 / Shared23162400、Runtime4 / Control176、Cursor155、Entry API3 / Request108 以及 161 项调试目录保持不变。

生产路径受控验证覆盖改名/描述/名单/缓存/PID 复用、旧代码漏掉 gameares 的负对照、真实重连动作及旧反向暂停负对照、owner 和退避保护、三个模块切换生命周期、隐藏状态导出与完整选择 UI。源码摘要、具体检查数、成功/失败与编译证据见同提交 [汇总证据](evidence/game-process-recovery-release-validation-20261107.json)，不把不同检查口径相加为一个实机验收数字。

实际产物还须通过冻结提交的 x64 Host / x86 Runtime / x86 broker 配套构建及 PE/ABI 检查。受控 Win32 API 替身、Windows 目标编译和状态序列测试不等于用户 Windows 桌面、当前平台、真实游戏或多人会话已验证。当前游戏的具体功能是否恢复仍以更新后的实机连接与功能结果为准。
