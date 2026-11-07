# pre.8：自动维修归组、独立快捷键与异步取消

日期：2026-11-07（Asia/Shanghai）。基线提交：`59ba074331e3758e152587aff6ddbe04221d0090`（pre.7）。

## 问题与证据边界

用户要求将自动维修移到自动功能、配置维修频率，并让自动页各功能分别设置开关快捷键。本轮检查 Host 设置/控件/快捷键路径和 Runtime 维修调度，发现并修复关闭自动后晚回计算仍能发出维修命令的问题。

本轮没有取得或执行新的用户游戏 EXE、Ares/Phobos DLL 或内存转储，没有新增受支持的可执行文件 SHA-256。源文件 SHA-256 仅用于绑定受控验证，不作为游戏二进制身份。沿用原生 Repair 事件 `0x15`、已有对象字段和游戏规则；没有新增引擎函数地址、字段偏移或协议布局。Assist100 / Shared23162400 保持。

## 自动功能与持久化

`assist/source/automatic_features.h` 统一声明 13 个持久功能开关。自动维修的控件 100、旧快捷键控件 200 和间隔控件 312 归入 Automatic；普通控制页不再重复创建维修开关。

| 功能 | 功能控件 | 按键控件 | 动作 ID | 配置来源 |
|---|---:|---:|---:|---|
| 自动维修 | 100 | 200 | 1000 | 保留旧 Hotkeys 配置 |
| 载员自动逃生 | 455 | 456 | 1696 | 保留旧 Eject 配置 |
| 磁电全局自动摔落 | 899 | 9502 | 1802 | AutomaticHotkeys/MagnetronDrop |
| 生产预定 | 799 | 9503 | 1803 | AutomaticHotkeys/ConstructionQueue |
| 矿车自动卸矿 | 470 | 9504 | 1804 | AutomaticHotkeys/OreReturn |
| 自动点杀蜘蛛 | 9496 | 9505 | 1805 | AutomaticHotkeys/MinerDefense |
| 防御建筑优先攻击渗透单位 | 804 | 9506 | 1806 | AutomaticHotkeys/AntiTheft |
| 自动躲弹 | 885 | 9507 | 1807 | AutomaticHotkeys/Dodge |
| 防空火控 | 880 | 9508 | 1808 | AutomaticHotkeys/MissileDefense |
| 提前休眠留枪 | 9181 | 9509 | 1809 | AutomaticHotkeys/MissileReserve |
| 过滤低伤害自动目标 | 9182 | 9510 | 1810 | AutomaticHotkeys/LowDamageFilter |
| 磁电全局无限吸 | 9042 | 9511 | 1811 | AutomaticHotkeys/MagnetronGlobal |
| 自动移动攻击 CS | 446 | 447 | 1699 | 保留旧 CS 配置及不可用策略 |

10 个新绑定默认留空。已有维修、逃生、CS 和 AutoBoard 绑定保留；AutoBoard 是针对所选单位的命令，仍使用 action 1011，不改为持久开关。类型清单、范围、计算模式、游戏规则选项属于参数或独立规则，不误加入自动开关目录。

`automatic_hotkeys_host.h` 先检查控件可用性，只设一次勾选值，再直接调用 owner `proc(... WM_COMMAND/BN_CLICKED ...)`。这样继续经过原有可用性、前置条件、设置更新和保存路径，并避开 modern UI 父窗口子类对点击的再次切换。前台游戏、聊天与当前对局上下文检查沿用 `hotkey_host.h`。

注册冲突检查补齐侧栏、视野缩放、驻守、任务模式和副屏独立启用时的观察快捷键；对应旧编辑器也检查新增自动键，避免反向修改抢走自动功能绑定。仍按实际手势区分按下/松开/双按，保留明确启用 allHotkeys 时的多动作语义、自毁键物理独占和观察窗口三槽同键设计。

## 维修间隔的实际含义

`repairMs` 默认 2000 ms，Host 与 Runtime 使用 500–30000 ms 范围。它控制扫描新的维修候选，原生回血速度由游戏规则决定。启用立即扫描；运行中改值后，根据上次扫描时间和新间隔判断下一次到期。`syncTypeConfig()` 同步 `repairMs`，自动页编辑不依赖原控制页的保存按钮。

已建立的候选按原有每帧事件预算提交；空扫描也推进扫描计时。同帧多次进入不会取得额外预算。DWORD 差值继续支持常规时钟回绕，跨局重置按新对局身份重新扫描。

## 关闭后晚回任务的缺陷与修复

旧流程在自动关闭的下降沿只清候选数量，没有撤销外部计算的 pending 状态和 FrameHistory。稍晚返回的结果仍能建立候选并提交维修。`command(Repair)` 又先于 `repairTick` 执行，所以同一消息关闭自动再请求手动维修，还可能被旧 pending 状态拒绝；关闭再开启也会沿用旧任务。

新增 `cancelAutomaticRepairWork()`：在自动关闭时先取消尚未提交的自动计算、候选及历史，再轮询结果；手动 Repair 分支也先执行同样清理。`repairAckPending` 明确保护手动工作及其 ACK/超时。新任务沿用既有 task/match/tick/frame 身份校验，晚回旧任务不能满足新请求。

`pendingRepairs` 保留。Repair 是切换事件，已入游戏队列的事件不能靠关闭自动撤回；保留去重记录可防止快速关开后又提交一次，把维修反向关闭。去重仍依靠对象身份、观察到正在维修或满血等证据。

## 验证与未证实部分

- `assist/tests/automatic_controls_test.py`：795 项受控检查通过，分为 Standalone 与 RA2_UNIFIED UI 各 348、真实分发/注册 68、类别 17、创建与布局源码合同 14。执行生产设置处理器与分发器，Win32 控件/键盘/文件/共享内存边界使用替身。
- 上一版冲突函数的 StickyGuard 抢键反例实际编译并失败，作为独立负对照，不算成功断言。
- `assist/tests/auto_repair_runtime.py`：旧 pre.7 原文三个反例均复现预期失败；修复后生产函数提取验证 24 场景、93 断言通过，覆盖晚回、手动叠加、关开代际、跨局、去重、实时改间隔、边界/时钟回绕与预算。
- `assist/tests/repair_delivery.cpp`：既有去重/身份/健康/容量/reset 回归通过。
- 本轮结果与精确源码摘要见 [整合验证](evidence/automatic-controls-release-validation-20261107.json)。旧 broad UI/热键测试替身本身已过期，本轮没有把未执行的旧测试体算作通过。

最终同提交构建、PE/版本/ABI 与四个交付文件校验由 release-manifest 记录。受控边界验证和 Windows 目标编译没有执行真实 Windows 界面、游戏 Hook、Ares/Phobos MOD 或多人会话；没有完成浏览器视觉/DOM 验证。
