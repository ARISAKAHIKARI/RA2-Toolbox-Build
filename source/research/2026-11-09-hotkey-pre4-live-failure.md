# 1.0.47-pre.4 快捷键仍失效的实机日志证据

日期：2026-11-09。用户反馈最新版本的主动快捷键仍然失效。本条记录第二份上传日志中的完整 pre.4 会话，并按冻结的 pre.4 源码解释计数含义。**目前没有证明一个能解释全部现场按键尝试的单一根因，也没有确认现场已经修复。** 历史受控夹具不能替代这次实机失败的闭环。

## 来源与分段

原始日志为 1,982,899 字节、2,472 行，SHA-256 为 `4a350e12638be731ead8a98206100694fcbcbad0e4ccb9c87925f111f55d1f34`。原件只保留在工作区，没有复制进仓库。本条与[结构化统计](evidence/pre1475-hotkey-input/runtime-log-summary.json)只保留时间、行号、选定状态字段和汇总，不保存原始用户路径、模块清单或完整日志。时间按日志原文记录，未据此推断时区。

源码解释基于 pre.4 提交 `3cd2f472b24921a3fcbca48a85e09e8a08d6f569`。结构化统计绑定了 `main.cpp`、`hotkey_host.h` 和 `middle_input_host.h` 的原字节 SHA-256；下述源码行号也指该冻结提交。

| 日志行号 | 版本 | Host PID | 会话首末记录时间 | bridge-health 条数 / 非零目标 PID 条数 |
| --- | --- | ---: | --- | ---: |
| 1–836 | 1.0.43-pre.9 | 80464 | 15:15:55.760–15:42:41.356 | 383 / 74 |
| 837–1900 | 1.0.43-pre.9 | 79688 | 15:42:47.103–16:11:09.752 | 439 / 157 |
| 1901–2472 | 1.0.47-pre.4 | 76612 | 16:12:36.630–16:22:04.379 | 133 / 38 |

前两段是 **1.0.43-pre.9**，不是稳定版 1.0.43，不能把它们当作稳定版提交 `bf3fbe88` 的实测对照。本文的输入计数结论只针对最后一个 pre.4 会话。

## pre.4 有效游戏时段

| 时间与日志行号 | 观测 | 能支持的判断 |
| --- | --- | --- |
| 16:12:36.630，1901 | Host 启动；之后一段时间 `pid=0` | 这段尚未发现游戏，不能代表稍后的实际对局 |
| 16:18:34.722，2225–2226 | 发现并读取附加 PID 53692、线程 75764 | 已找到真实目标 |
| 16:18:34.885，2232 | 安装 `WH_GETMESSAGE`；Runtime v4、Assist v102 | 有明确安装记录 |
| 16:18:34.912，2237 | `stage=connected`、`fresh=1`，回调开始，`commandStatus=10` | 连接已工作，战术界面仍未开放命令 |
| 16:18:42.169，2255 | `gate=ready`，match=3 | 对局开始具备命令资格 |
| 16:18:42.180，2260–2261 | 两组系统快捷键返回 1409，记录 `route=foreground-poll` | 这两组走已保留绑定的前台采样回退 |
| 16:18:43–16:18:46，2264、2265、2268、2269 | `hotkeyMode=2`、`hotkeyGroups=9`，显示读/渲染均为 1，chat=0 | 命令热键上下文确实建立过 |
| 16:18:46.401–16:18:47.885，2270–2277 | `specialDialog=8`，随后恢复 0；gate 10→1 | 有一次明确的对话框抑制与恢复 |
| 16:18:48.174，2279 | 再次 mode=2、groups=9 | 恢复后仍未记录输入动作 |
| 16:18:49–16:19:29，2280–2318 | 多条 mode=0；相邻 display 记录 `foreground=0`，显示读/渲染仍为 1 | 与软件判定为非游戏前台一致，不能归因于显示快照长期失效 |
| 16:19:30.171–16:19:31.168，2321–2322 | display `foreground=1`；mode=2、groups=9 | 又一个明确的命令热键窗口 |
| 16:19:35–16:20:01，2325–2346 | display 再次多次 `foreground=0` | 仍缺实际前台窗口身份，不能断言用户是在其他窗口操作 |
| 16:20:05.509–16:20:06.169，2352–2356 | `specialDialog=1`，随后关闭并恢复 ready | 又一次对话框与恢复 |
| 16:20:06.424，2361 | gate=8，`observer-defeated-or-no-command-house` | 命令所属方资格结束 |
| 16:20:11.078，2367 | `target-exited`，退出码 0 | 游戏正常退出；后续 pid=0 属于退出后阶段 |

38 条非零 PID 的 health 包含连接初始化和退出附近状态，不能把全部 38 条都称作命令就绪。其间 36 条为 connected，33 条 fresh=1，23 条 commandStatus=1。所有 23 条命令就绪记录均 fresh=1、commandChat=0、nativeHook=1、clickHook=1、postError=0、runtimeError=0、faultSerial=0。

回调累计数达到 6,920，dispatches 达到 6,927，controlFrame 达到 5,130。退出附近出现的两条 postError=87，不能倒推为活动期间的输入失败。installedCodePatches=0 也不能解释成没有 Hook：本日志同时有安装记录和持续回调。

## 六条命令热键模式记录

以下六条均为 PID 53692、match=3、commandStatus=1、fresh=1、displayReadStatus=1、displayRenderStatus=1、commandChat=0、hotkeyMode=2、hotkeyGroups=9。`groups` 是注册表中的物理组合分组数，不等于全部逻辑动作数，也不是完整的绑定清单。

| 日志行号 | 时间 | controlFrame | callbackCount | dispatches | Messages / Sampled / Actions / Blocked |
| ---: | --- | ---: | ---: | ---: | --- |
| 2264 | 16:18:43.167 | 58 | 190 | 188 | 0 / 0 / 0 / 0 |
| 2265 | 16:18:44.160 | 120 | 291 | 290 | 0 / 0 / 0 / 0 |
| 2268 | 16:18:45.166 | 179 | 390 | 390 | 0 / 0 / 0 / 0 |
| 2269 | 16:18:46.170 | 240 | 490 | 491 | 0 / 0 / 0 / 0 |
| 2279 | 16:18:48.174 | 358 | 686 | 687 | 0 / 0 / 0 / 0 |
| 2322 | 16:19:31.168 | 2932 | 4008 | 4013 | 0 / 0 / 0 / 0 |

这不是仅从 pid=0 阶段得到的“没有输入”结论。整个 pre.4 非零目标时段的 38 条 health 中，Messages、Sampled、Actions、Blocked、Pending 都为 0；request、ack、queueSent、queueFull、commandRejected、lastSubmitError 也都为 0。pre.4 全会话没有 `command-submit` 或 `command-result` 记录。

因此可确认：**日志没有显示任何配置热键进入命令提交链；至少在上述六个状态采样点，阻塞不能简单归结为没有连接、显示渲染不可用或聊天框开启。** 日志没有原始按键时刻，不能把六个状态点当成六次已知按键实验。

## 两个注册冲突及计数语义

七个注册周期都报告同样两组失败，共 14 行，分别位于 2260–2261、2275–2276、2293–2294、2317–2318、2348–2349、2357–2358、2359–2360：

| ID | 日志 key | 编码 | 组合 |
| ---: | ---: | --- | --- |
| 1000 | 1618 | 0x0652 | Ctrl+Alt+R |
| 1727 | 1606 | 0x0646 | Ctrl+Alt+F |

Win32 错误 1409 在 Windows SDK `winerror.h` 中定义为 `ERROR_HOTKEY_ALREADY_REGISTERED`。冻结源码 [main.cpp](../assist/source/main.cpp) 第 590–599 行在失败时保留注册表中的绑定并记录 foreground-poll；所以 mode=2 时 `hotkeyFallbacks=2`、`hotkeyLastError=1409` 与 groups=9 可以同时成立。它证明两组系统注册存在冲突，**没有证明冲突占用者、自动装的实际绑定，或全部快捷键都被冲突禁用。**

必须按实际计数位置解释日志：

- `registerKeys()` 只清 fallbacks 和 lastError，不清 messages、sampled、actions、blocked。输入累计值全为 0 不是重注册清掉历史造成的。
- [hotkey_host.h](../assist/source/hotkey_host.h) 第 68–69 行在采样器产出配置手势后才递增 sampled。它不是轮询次数；Sampled=0 不证明未调用 GetAsyncKeyState。
- 同文件第 79–90 行的 messages/blocked 记录特定消息入口及拒绝分支，actions 在第 12–16 行的分发入口累计。
- 同文件第 57–66 行中，formationInputOwns、runtimeMiddleOwnsNow、命令上下文不满足、连接不新鲜，都可以在没有增加 blocked 的情况下提前返回。[middle_input_host.h](../assist/source/middle_input_host.h) 第 21–26 行还检查 Runtime 预览及鼠标手势所有权。Blocked=0 不能排除这些路径。
- `displayRenderStatus` 才是显示快照状态。health 中旧字段 `renderStatus` 对应 incoming.renderRevealStatus，不能替代前者。

## 已确认的范围与尚缺的证据

已确认的范围是“连接与命令上下文曾正常建立，但未记录配置热键消息、采样手势、动作分发和命令提交”。这使输入识别及输入服务成为下一步重点；它不支持“命令已经发出而被游戏拒绝”，也不支持把整段故障归因于注入一直失败或显示样本一直没有发布。

仍未观测到的关键边界如下：

1. 用户具体按了哪个组合、何时按下，以及当时完整的有效绑定表。groups=9 不能回答这些问题。
2. GetAsyncKeyState 实际返回的目标键及修饰键状态；当前日志没有轮询进入/完成计数。
3. foregroundGame 判定时的实际 foreground HWND/PID。display 的 foreground=0 是软件判定，不能代替窗口身份核验，也不能据此指责操作位置。
4. formationInputOwns、runtimeMiddleOwnsNow 或连接/上下文条件是否让采样提前返回；现有 Blocked 不覆盖这些条件。
5. 两个 1409 冲突的占用者，以及冲突组合是否就是用户尝试的自动装快捷键。

此前[版本源头研究](2026-11-09-hotkey-regression-origin.md)发现的共享采样冷启动循环、跨时钟及同 token 显示状态边界，仍需按各自前提讨论。此处已有真实连接、推进中的游戏帧以及 displayRenderStatus=1 的命令热键窗口，不能把某个历史反例直接宣布为这次现场的唯一根因。

本条是失败证据归档和后续观测边界，不是修复完成说明；没有新增游戏地址或原生 ABI 结论，也没有据此报告新的产品测试结果。
