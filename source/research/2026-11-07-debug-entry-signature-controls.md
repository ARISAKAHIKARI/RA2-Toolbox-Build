# 调试入口页、逐入口签名开关与真实安装状态（pre.7）

## 问题与证据范围

用户希望在同一调试页查看各个游戏入口的用途、关联功能、签名检查和实际 Hook 状态，并能分别选择是否执行参考签名检查。本轮以 pre.6 的 `ae5bb316eb340491e450ab90bb919e3943b02c4f` 为功能基线，目标版本为 `1.0.43-pre.7`，共享协议升级为 **Assist100**。最终源码提交、完整构建和交付文件由后续 release manifest 绑定，本记录不嵌入自己的提交或文件摘要。

调查前已查询本索引和持久笔记 v148（180 条 notes、179 条 FTS；已有的一条 FTS 漏项未改）。延续 [pre.6 依赖隔离与恢复](2026-11-07-runtime-dependency-isolation-and-recovery.md)、[pre.5 Ares 普通调用与内部断点分离](2026-11-07-ares-native-entry-compatibility.md)以及既有 owned Hook 恢复记录。本轮没有新增游戏 EXE、Ares 模块或现场线程转储的完整 SHA-256，也没有执行真实游戏或多人会话；不把历史日志摘要当作本轮游戏二进制身份。

这里记录的是源码中的消费者、固定地址、协议、持久化和受控验证。不保存原始用户日志、私人游戏路径、游戏程序或 dump。页签中的“跳过”表示用户选择跳过该入口的参考模式判断，不等于已证明当前机器码的 ABI 正确，也不等于已经安装 Hook。

## 一、目录身份与签名策略

[debug_entry_catalog.h](../shared/source/debug_entry_catalog.h) 收录 161 行：58 个 Call、95 个 Patch、6 个 Breakpoint 和 2 个 System。95 个 Patch 中有 13 个只作历史诊断的退役站点，当前写入白名单不接受新安装；另有两个旧 C4 站点和 ScrollMap `0x4A9840` 保持产品功能禁用，签名选项不能重新启用它们。

目录按稳定身份 `id = (kind << 24) | (address & 0x00ffffff)` 区分入口，固定地址和类别共同构成配置键。运行时拒绝以高位地址截断后冒充已注册站点；未知身份不自动获得放行。`0x4F7870` 的普通 Call 与 Full Breakpoint、`0x50B6F0` 的 Patch 与 Control Breakpoint 各自独立；切换一类不能隐式放宽另一类。

Call 行同时包含真正调用入口和明确标注的调用约定依赖，例如 `0x6D2B3F`、`0x6A9316`、`0x6F4803`。这些行没有可被称为“已安装”的补丁。WH_GETMESSAGE/WH_CALLWNDPROCRET 系统 Hook 没有固定游戏机器码签名，显示“不适用”，不能用“未知”或“已跳过”代替它们的实际含义。

[debug_entry_policy.h](../shared/source/debug_entry_policy.h) 统一保存配置和观测，所有 Runtime 编译单元共享同一份 inline 状态。配置真正改变时递增进程内 `policyRevision`，让入口缓存立即失效；不只依赖宿主传来的 generation，因为重新连接后宿主可能再次使用相同 generation。只清除实际变更条目的签名观测，保留无关入口已有状态。

参考签名与其它条件分别判断：不可读内存仍拒绝；普通 Call 的手动放宽还要求目标可执行；固定原目标、必须重放的指令形状、对象和线程身份、地址范围、长度、所有权、冲突与恢复规则继续生效。既有原生 ABI、参数布局和入口地址未被重写。原始 `nativeEntryCallable()` 保持严格，普通 Call 的开关由消费者包装层应用，硬件断点可以继续调用原始严格判断。

## 二、保存设置与跨进程发布

[additional_settings.h](../assist/source/additional_settings.h) 增加固定布局 `debugentries::Config`；[extra_settings_io.h](../assist/source/extra_settings_io.h) 在 `Assist.ini` 的 `[DebugEntries]` 节逐个保存 `Skip_XXXXXXXX`。只接受精确文本 `1` 为跳过；缺失、`0`、其它文字或截断值均恢复严格。未知键被忽略，目录排序变化不迁移用户选择到另一入口。

页面复选框表示“执行校验”，缺省选中。取消该项才保存 skip=1，再次选中会恢复严格检查。保存仍沿现有设置写入和错误处理路径；运行时同步在用户操作时立即发生，不等待原有 300 ms 自动保存计时。`syncAdditional()` 对同一配置不重复发布；新建共享映射仍发布一次初始值。配置和观测各有独立 seqlock，避免宿主或 Runtime 读取到一半更新的数据。

Assist100 在 Shared 末尾追加以下字段。真实 Windows x86/x64 目标布局检查均已通过，验证脚本将这些数值固定在 [verify_crossbit_abi.py](../tools/verify_crossbit_abi.py)；新 wire 数据没有宿主指针、动态容器或依赖本机宽度的字段。

| 字段 | 字节偏移 | 作用 |
| --- | ---: | --- |
| `debugPolicySeq` | 23154172 | 配置 seqlock |
| `debugPolicy` | 23154176 | 1032 字节 Config：generation、count、256 个稳定 ID |
| `debugSnapshotSeq` | 23155208 | 观测 seqlock |
| `debugSnapshot` | 23155212 | 7180 字节 Snapshot；最多 256 个 28 字节 Observation |
| `debugVisible` | 23162392 | 只读探查需求，不启用任何游戏功能 |
| `sizeof(assist::Shared)` | 23162400 | Assist100 两种目标位数一致 |

Runtime 在维护硬件功能前接收配置，宿主显示的 snapshot 必须与当前配置 generation 相符。旧观测、无连接、待执行与确认结果分别呈现，不能因为按钮刚被点击就展示已安装。

## 三、六个硬件断点入口的独立接线

[tech_ui_breakpoint_bridge.h](../assist/source/tech_ui_breakpoint_bridge.h) 和 [single_player_fps_runtime.h](../assist/source/single_player_fps_runtime.h) 按实际六个入口消费 Breakpoint 策略，四个物理 DR 槽及原有共用预算不变。

| 逻辑 DR 地址 | 功能 | 原有冷检查依赖与本轮行为 |
| --- | --- | --- |
| `0x4F7870` | Full 科技参数 | 两个 UI caller `0x6A97C3` / 20 字节、`0x6AA777` / 15 字节，以及 CanBuild 入口 16 字节。普通 Call 开关不控制本项；严格已知 Ares 调用可标为 Platform。 |
| `0x6D88FC` | 单槽黑幕显示 | 捕获 `0x6D88F4` 的 10 字节前缀；回调仍只按原有条件修改相应标志位。 |
| `0x4F7951` | Exact 建造上限 | `0x4F794A` 的 13 字节上下文与 `0x4F7870` 原生入口；与第二个 Exact 站点分别配置。 |
| `0x4F7B66` | Exact 可选等级比较 | `0x4F7B5C` 的 16 字节上下文与相同 CanBuild 入口；只有启用比较等级时才成为该组的必要依赖。 |
| `0x50B6F0` | Control 参数 | 三个 mastercontrol 原始站点、各自上下文、入口尾部和 `0x50B710` 的叶函数区域，完整读取后判断。 |
| `0x55D7BC` | 单机 FPS | `0x55D79E` 起 36 字节；原计时器、世界/会话、心跳、停止和寄存器语义保留。 |

新增 [tech_ui_breakpoint_debug.h](../assist/source/tech_ui_breakpoint_debug.h) 先捕获实际可读图像，再决定是否准入。允许跳过参考模式时，Full、Exact、Map 和 FPS 的已有热路径仍逐字节比较**本次实际准入的图像**；之后图像再变化，回调仍停止参数修改。Full 还在严格原生验证前后比较入口图像，变化或不可执行仍属结构拒绝。没有把每次异常回调改成配置搜索、ReadProcessMemory 或 FNV 重算。

Control 使用 `mastercontrol::compatibleStrict()`，不能经过会消费 Patch 开关的普通 `compatible()`。因此修改 `0x50B6F9` 的上下文、并放宽 Patch `0x50B6F0` 时，Control Breakpoint 仍严格拒绝；只有自己的 Breakpoint 开关才影响这一层参考判断。所有所需区域必须可读，早先一处不匹配不会掩盖后面的不可读区域。

切换某个 DR 入口时，仅停止相应组的参数权限并使该组准入失效，随后沿既有维护/退休流程重查。Exact 两个入口共享一个功能组；无关 Call、Patch 或其它 DR 的变更不撤掉已确认健康的 Full 组。lease、pending transaction 和 uncertain 记录不会因用户切换而被抹掉，关闭放宽不能绕过尚未完成的恢复。

Ares 绕过原版内部 Exact 比较路径仍是独立的 `PathUnreachable`。跳过签名不会让执行流经过不存在的原版内部位置，不能据此显示该 Exact 功能工作。pre.6 的 Full/control 500 ms 有界失败重查、宿主 2 秒/30 秒完成时退避和 broker 1 秒退避保持原有语义。

## 四、实际观测、安装与恢复所有权

页面分列参考签名与 Hook 状态。签名可为未观测、匹配、已知平台、差异、已跳过或不可读；Hook 可为不适用、未安装、等待中、已安装、冲突或不可用。原因继续区分未请求、等待 Runtime、owner 待退休、外部占用、产品禁用、执行路径不可达和结构条件不满足。

调试页可见时，每 500 ms 做受限只读探查：原有 21 个 Call 入口和六个 DR 冷检查可产生新观测，不能为诊断安装 DR、创建 FPS 计时器、授予参数权限或覆盖现有热快照。其余消费者在各自真实路径中更新观测；manager 的默认 Inspect 不读现场，明确请求的只读 inspect 才检查实际字节。

DR 的“已安装”来自 Service 已确认的 activeTags/readback；请求布尔值、单纯保留的 lease 或还在等待中的工作线程都不构成成功。外部 DR、uncertain 和未完成退休继续显示冲突/等待。已经关掉功能但物理槽尚未确认退休时保留实际已占用/待恢复信息，不把 UI 意图当作清理完成。Full 的非执行页或验证中图像变化显示 Structural，不误报普通参考签名差异。

Patch 的恢复继续依赖原注册记录：当前内容须仍等于本工具实际写入的 patch，恢复目标须等于该记录保存的 actual original；owner、大小、冲突和身份规则不放宽。5 个 viewport DataOperand 允许在相同指令形状下接受不同原操作数，保存并恢复实际原值。其余固定重放指令、虚函数原目标和 master native 严格叶仍可作结构条件拒绝。

曾在放宽模式安装、随后恢复到 inactive slot 的记录，不能成为以后永久跳过检查的凭据：新的 Install 必须重新消费当前策略。再选中校验后，未知原图像仍须拒绝重新安装；Restore 本身继续依据保存记录，不以新参考签名阻止旧 owned 补丁清理。`0x4F4480` 等多个消费者共用的同一物理 Patch 仍是同一个所有权实体。

## 五、验证、编译与剩余边界

[设置与 DR 分支证据](evidence/debug-entry-signature-controls-20261107.json) 汇总本分支最后一次执行的六套验证，共 **3937** 项成功检查，40 个已记录生产/测试文件摘要与最终输入一致。它是发布验证的一个子集，不与其它分支重复相加。

| Runner | 成功检查 | 重点 |
| --- | ---: | --- |
| `debug_entry_settings.py` | 189 | 生产加载/保存/同步函数、稳定 ID、精确值、默认严格、写入失败、seqlock 和即时发布 |
| `debug_breakpoint_policy.py` | 234 | 六入口策略、同地址不同类别、不可读/非执行/图像变化、真实观测、热快照、只读探查和 owned lease 保留 |
| `tech_ares_entry_compat.py` | 463 | 实际 Full 回调、既有 Ares 准入与原有局部拒绝 |
| `tech_parameter_recovery.py --after-only` | 208 | Full/control 恢复、健康兄弟组、旧 lease、pending/uncertain 和持续失败 |
| `control_parameters.py` | 2752 | 实际 Control 回调、严格 caller、原始指令与不写数据边界 |
| `tech_enhancement_midgame.py --after-only` | 91 | 中途开启、目录更新、Exact 路径不可达与在产保护 |

这些测试在 Linux 执行抽取的生产函数；内存、操作系统、原生调用、线程和 DR worker 的外部边界由明确替身提供。已有恢复和中途开启的旧版反例本轮没有再次执行，不计入当前成功项。

生产 `bridge.cpp`、Map Windows fixture 和 FPS Windows fixture 已用 Zig 0.13.0 编译为实际 Windows x86 对象文件。宿主 x64 对象和真实 x86/x64 wire 布局由整合分支分别验证。对象编译没有执行 Windows Hook、硬件断点或游戏指令；先前 `-fsyntax-only` 产生的 driver FileNotFound 结果不作为通过门槛，已改用实际 `-c` 编译。

另外的目录/UI、普通 Call/二次依赖、Patch manager/registry、布局和 store 验证由 [整合证据](evidence/debug-entry-release-validation-20261107.json) 独立汇总。UI 完整目录 JSON 为 161 行，长诊断样本 184192 字节，低于 262144 字节限额；目录编码检查 327 项，16 个新增 debug/store 场景与 30 个既有 store 场景单列。并发模型运行的调度相关检查数不硬凑成固定总计。没有把本机缺少浏览器执行的 DOM/截图验证描述为已经完成。

冻结提交后的全量编译、PE/ABI 与安装包仍是独立门槛，以最终构建和 release manifest 为准。DR 工作线程的 INFINITE rendezvous、存活 poisoned broker 的恢复限制和产品禁用的玩家上下文切换路径未在此轮重写；理由和证据边界沿 pre.6 记录。本次没有宣称消除所有挂死、通过真实 Windows 游戏验收或支持任意未知平台机器码。
