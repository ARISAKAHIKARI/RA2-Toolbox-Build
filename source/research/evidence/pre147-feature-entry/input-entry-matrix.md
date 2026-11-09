# pre.5 输入入口审查与修复

基线 commit：`18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9`。所有源码快照和远程定位见 `source-manifest.json`。

本轮只审入口、共享门控、队列与恢复；“执行验证”指编译执行实际生产函数，Win32输入和最终动作端点受控，不代表Windows或游戏实机通过。

## 已证实问题

- **F1 — formation WM fallback priority/epoch**：Current match preview is rejected by runtimeMiddleOwnsNow before formationHotkey; an old match can bypass that guard and steal the command. fixed in hotkey_host.h with current identity, binding, age and independent context watermark
- **F2 — unrelated mailboxes block local actions**：All pending actions waited for command/selection queue and ACKs, including persistent local toggles and read-only pins. Four busy forms reproduce eventual expiry without dispatch. fixed with localHotkeyAction and Pending.popReady; game FIFO/age preserved
- **F3 — formation lease bypass for immediate WM actions**：WM entry checked runtimeMiddleOwnsNow only, while sampler also checked formationInputOwns; an active post-gesture lease with inactive preview failed to protect immediate WM actions. fixed by keeping formationInputOwns in ordinary owner refusal
- **F4 — old context message can revive after newly reachable formation handoff**：The intermediate priority fix alone allowed <=150ms old messages after reregister, short focus gap, or epoch change. three intermediate failing cases reproduced; fixed by context watermark distinct from owner cancellation

- **F5 — 只读退休期间及快速恢复后的物理配对丢失**：本轮开发中间态的新只读入口会提前丢弃已接管DOWN的所有权，实际formation reset也会清掉内部owner。中央ledger现仅记录实际被WM_NULL消费的新DOWN，所有原功能仍先处理正常输入，未处理的旧UP仅作无动作配对退休。此项不是已发布pre.5的问题，也不是用户现场根因的证明。

## 入口矩阵

| Family / action | 入口 | 门控 | 恢复 | 结论与验证级别 |
|---|---|---|---|---|
| 消息泵 / 全部WM快捷键 | main message loop → IsDialogMessage → WM_HOTKEY → readOnlyWatchHotkey/handleHotkey | 窗口仍存活、关闭流程未开始；随后进入各业务门控 | 消息计数不随registerKeys清除；模式由tick重新计算 | 静态审查；没有发现吞掉全部WM_HOTKEY的源码分支；非Windows现场验证 |
| 普通命令 / AutoLoad等1001–1011 | RegisterHotKey和Registry.sample → dispatchHotkeyGroup → Pending → executeHotkey → send | enabled、游戏前台、真实命令通道、非聊天、fresh<1500ms；命令/选兵邮箱必须可用 | 失焦/聊天/暂停会cancel，恢复同步已按住键；释放后新按键重新触发 | 生产函数执行验证；正常命令保持串行，未引入缺ACK猜测重放 |
| 普通键物理兜底 / 全部命令族 | Registry.sample(GetAsyncKeyState) | 精确Ctrl/Alt/Shift；不接受额外Win；初次采样抑制已按住主键 | match变化、cancel后重新同步；原WM去重水位保留已有语义 | 受影响回归44场景193检查通过；不能证明现场GetAsyncKeyState一定有权限/一定看到极短按键 |
| 松开 / 双击快捷键 | WM按下+物理sample释放+expired单次到期 | 合法修饰键须持续；双击窗口350ms；普通WM最大2500ms | 取消清手势；恢复后不把旧第一次按键升级成双击 | 现有消息回归和本轮type release/double实际执行通过 |
| 注册冲突 / Win32 1409 | registerKey → Registry.add | OS失败记录fallback，物理组仍保留；Hold模式不能注册成命令 | 绑定/模式变化registerKeys重建，立即同步当前键 | 静态+既有context执行验证；没有发现因一个1409导致全部组丢失 |
| 自动功能开关 / 13项目录，当前AutoCS禁用 | automaticHotkey → 实际UI owner WM_COMMAND | 现有控制存在、ready且IsWindowEnabled；仍受前台/聊天/上下文约束 | 本轮按local动作执行，可越过无关command/select busy；原Pending年龄不刷新 | 发现并修复F2：pre5四类busy均导致自动开关2.5s后丢失；修后各1次 |
| 载员逃生1696 / 自动维修1000 | 各自原绑定 → automaticHotkey | 与自动UI owner相同；不走游戏命令邮箱 | same local取出策略；当前可用owner不重建Registry | 执行端点验证+console agent核对owner无同步registerKeys |
| AutoCS1699 / 高级Hold旧保留ID | registerKeys(featureavailability::AutoCS) / pointer授权 | AutoCS当前编译门为false；高级键由Hold+鼠标入口处理 | 开关/绑定变化刷新；本轮不重新启用不可用功能 | 静态审查；非“注册遗漏”或全部快捷键根因 |
| 同键fanout | Group → firstPerGesture或全部 → Pending.popReady | 本地动作按一次消费；游戏动作仍等command/select ACK | 延期游戏动作保持原tick和FIFO；上下文/epoch/超时会取消 | 发现并修复F2；同组自动开关+watch+AutoBoard执行一次且不互相饿死 |
| 只读跟踪1730–1732 / 主控制启用 | executeHotkey → watchhost::pin | 配置watch；只读world有效、非聊天、前台、无formation占用 | 本地动作可越过无关命令邮箱；world失败仍不盲选目标 | 发现并修复F2入口耦合；执行验证只到pin端点，不声称验证渲染/采集算法 |
| 副屏跟踪 / 主控制暂停(mode1) | readOnlyWatchHotkey/serviceReadOnlyWatchKeys → dispatchReadOnlyWatchKeys | 前台、renderStatus1、readStatus1、watchSecondary、非聊天 | 手势同步/epoch正常；生产者暂停后恢复路径由lifecycle agent专项负责 | 静态路径已审；不将本轮Host端点通过当副屏已可渲染 |
| 侧栏1734 / 视角缩放1720–1722 | executeHotkey改设置 → syncAdditional | 相应功能已开启；仍只接收合法新输入 | 本轮按local分类绕过无关command ACK，不改原配置owner | 侧栏busy反例执行通过；缩放同分类静态审查 |
| 类型1100–1611 / 合并选兵 | firstForKey → groupMask → sendSelection(-3) | 每个规范化组合注册一次；当局类型存在；显式gesture或legacy double规则 | 匹配epoch、取消typeTap；priority selection保留1500ms上限 | 新增执行验证合并两类型、legacy双按、松开、显式双击均通过 |
| 变体1700 / 空闲飞机1701 | executeHotkey → sendSelection / selectNextIdleAircraft | 变体手势；飞机另需新鲜外部单位数据和可见战场 | 选择意图过期或身份变更丢弃；新输入可恢复 | 静态审查；飞机数据年龄时钟问题交root专项修复，未在此测试算法 |
| 超级武器1020+ / 铁幕1009 / 超时空1010 | 注册去重复 → executeHotkey查当局超级武器 → send | 确实拥有目标超级武器；常规命令上下文和邮箱 | 无目标给提示；恢复后重新按键；无缺ACK重放 | 静态审查；未发现全部命令阻断的新门控 |
| 喝彩1710 / 嘲讽1711–1718 / Sticky1723 / 普通任务模式 | executeHotkey → send对应命令 | 常规上下文与命令邮箱；Runtime能力/冷却在内部另验 | 当前match、有限队列年龄；不因local旁路绕过游戏提交 | 静态审查；命令序列保护保留 |
| 攻击脚下 / 所属方自毁 / 原生事件放置 | dispatchHotkeyGroup即时分支 | 必须独立/允许的绑定、输入<=150ms、当前selection/viewport和空闲通道 | 过期/碰撞/忙直接提示重新按，不延后解析变化后的选择 | 发现并修复F3：formation有效租约active0时WM即时路径此前可越过owner；修后拒绝 |
| 编队队形WM转交 | handleHotkey → formationHotkey → formationKey mailbox | 当前match且active、完整物理绑定、当前context水位后、消息<=150ms | owner每tick手势cancel不重置独立context水位；失焦/换局/再注册拒绝旧消息 | 发现并修复F1：pre5当前preview分支不可达，旧match反而吞普通动作；新增回归通过 |
| 编队 / QuickFormation原生MSG | AssistRuntimeFilter → formationMessage | formationContext；当前游戏前台/聊天/心跳/preview状态；无额外传统feature门 | Esc/松键/失焦/换局reset或cancel；publish写当前match | 实际formation入口、25单位preview、reset、5鼠标键/传统Alt/只读及快速resume配对已受控执行；算法与游戏事件仍非实机验证 |
| 中键/侧键/其他配置手势 | AssistRuntimeFilter → moOrdersMessage；Host runtimeMiddleOwnsNow拒绝重复捕获 | 精确配置手势、当前preview match、runtime context；冲突声明仍保留输入所有权 | 释放/Esc/策略/身份变化reset；输入所有权释放后新按键可恢复 | Host当前/旧preview及恢复执行验证；Runtime内部作静态入口审查 |
| 增强鼠标 / 协同 / 卸矿 / 卖车等 | Host pollPointer → servicePointer → pointer mailbox | 100ms捕获时效、epoch/viewport/cancelSerial、聊天/前台/owner排斥 | context变化disarm并清fanout，保留已提交ACK语义 | 静态审查；发现主控制暂停保留Shared会失去原间接门，已交lifecycle补显式enabled/readOnly |
| 原生建造标签 / 原生Planning按住 | nativekeyhost::current/load + gameinput::keySnapshot | 从原生只读CommandClass表取绑定；Host pointer context时效和前台 | 读取失败250ms后重试；PID/match变更重新取绑定；key tracker随context清理 | 静态审查；没有发现单个空绑定令全部命令被停用 |
| 按钮 / 基础命令 | proc WM_COMMAND → send → commandQueue | 按钮可接受工具前台；参数/epoch/功能能力仍检查 | 队列12s上限、当前PID/已知match；UnknownMatch仅允许RevealMap | 静态审查+console agent互核；未认证epoch须重新触发。candidate存在的误导反馈列低优先级，不放宽epoch |
| 优先选兵邮箱 / 所有选兵来源 | sendSelection → serviceSelection | 已知match、类型hash、前台可用、fresh、非聊天；selectRequest必须ACK | pending1500ms取消，旧match/类型列表变更立即取消；不会重放缺ACK | 静态审查；该邮箱仍串行游戏动作，但不再拖死local动作 |
| 总开关/聊天/暂停/失焦/换局恢复 | commandHotkeyContext → registerKeys/cancel → synchronize | 真实command ready及readonly许可，不依赖展示renderStatus | 重新同步按住键；新release→press恢复；local与formation分别验epoch/年龄 | 本轮恢复用例通过；Runtime/OS实机焦点行为仍需现场日志 |
| 原生消息所有权 / 暂停与恢复后的成对输入 | AssistFilterMessage central pair ledger → original owners → release-only fallback | 只记录实际被WM_NULL消费的物理DOWN；只读时不采样物理键、不进入动作owner；normal owner先处理UP | 真正新DOWN/DBLCLK清旧所有权；repeat不清；feature reset与已ACKresume不丢原UP；新完整pair仍按配置恢复 | 执行验证：实际formation capture/layout/reset+中央filter，45场景188检查全过；开发中间态编译成功且37个预期失败。不是pre5现场根因证据 |

## 实际结果

| 验证 | 场景 | 检查 | 结果 |
|---|---:|---:|---|
| 新生产入口回归 | 33 | 77 | 全过 |
| pre.5 负对照 | 15 | 30 | 编译成功、17个正确性断言失败 |
| 中间版旧上下文负对照 | 33 | 77 | 编译成功、3个旧消息断言失败 |
| 既有Registry消息恢复 | 44 | 193 | 全过；pre.3负对照仍失败 |
| 既有Host context | 35 | 104 | 全过；pre.3三负对照仍失败 |
| 既有formation ownership | 13 | 78 | 全过；旧表达式5反例仍失败 |
| 实际formation与只读配对 | 45 | 188 | 全过；冻结开发中间态37个预期失败 |
| 暂停键配对 | 未单独报告 | 45 | 全过 |
| 普通增强鼠标配对 | 未单独报告 | 69 | 全过 |

新专项命令：

```sh
python3 source/assist/tests/hotkey_entry_recovery.py --out /workspace/scratch/c1b87d8550cd/entry-audit-pre6/input/current
```

只读配对附加阶段命令（并入原生命周期门禁）：

```sh
python3 source/assist/tests/native_readonly_pairing.py --out /workspace/scratch/c1b87d8550cd/entry-audit-pre6/input/readonly-pairing-final --negative-control
```

源快照负对照明确标为本轮开发中间态，固定 SHA-256 `959b707cedaca064e1b3c44030b152da14d697035093e1e0007d22bd07bf8569`；与 pre.5 负对照分开记录。

证据目录保留 `production.cpp`、编译日志、原始运行日志、JSON断言与源哈希；新驱动也绑定真实编译依赖。

## 保留边界

- OS message delivery, UIPI and game-process execution were not exercised.
- The command/watch/UI owner seams observe Host dispatch, not game completion or image rendering.
- Missing ACKs still intentionally block subsequent game-changing commands; no speculative resend was added.
- Native feature algorithms and multiplayer semantics are outside this entry audit.
- Root and lifecycle/console agents separately own clock, pause/producer, explicit read-only protocol and UI control audits.
- Final unified validation must rebind the fully settled source tree; this task did not freeze or commit a release.
