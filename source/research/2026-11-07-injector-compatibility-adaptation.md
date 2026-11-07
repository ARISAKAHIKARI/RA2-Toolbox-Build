# Ares、Phobos 与其他注入的逐入口兼容边界

## 问题与结论

问题是：已有 Ares 普通调用适配和 Assist100 的逐入口签名设置之后，怎样兼容 Phobos 及其他注入，又不把未知机器码当作已验证的 ABI？本次以 Toolbox `59ba074331e3758e152587aff6ddbe04221d0090` 为已交付基线，只做源码研究。没有新增用户游戏映像、Ares/Phobos DLL 的 SHA-256 或现场运行证据。

结论是按“模块版本、具体入口、调用方式、覆盖区间、续接语义、恢复所有权”建立兼容记录。DLL 名称、可执行 `E9` 跳转和手动关闭签名分别只能提供部分信息，不能合并成全部功能兼容。现有三个 Ares 普通调用入口继续维持既有范围；本次没有把 `Phobos.dll` 存在加入它们的放行条件，没有扩大 Patch 或 DR 的准入。

Phobos 的固定生产源码与本地 161 行目录存在 **7 行精确起点交集**；计入已建模的签名和上下文区间后为 **15 行**。这些是潜在依赖或写入交集，不是 15 个现场故障，也不是已加载 DLL 的 Hook 数量。完整方法、交集和证据边界见 [静态扫描记录](evidence/injector-compatibility-adaptation-20261107.json)。

## 基线已经支持什么

`assist/source/native_entry_policy.h`、`native_entry_runtime.h` 只为普通调用识别 `0x6AA600`（RecheckCameos）、`0x5F7900`（FindFactory）、`0x4F7870`（CanBuild）。原字节匹配直接接受；否则要求加载的 `Ares.dll` 具有预期的 PE32/x86 映像结构，并在最多八层、有循环检测的可读可执行路径内识别入口 `E9` 及后续有限跳转形状。实际调用仍从原始游戏入口开始，使平台回调有机会运行。当前检查没有按 Ares DLL 文件哈希、版本或回调归属建立档案，不能写成“所有 Ares 版本已验证”。

`bridge.cpp` 的 Call 策略包装层与严格原生检查分开，依赖结果局部缓存 250 ms，策略实变立即失效。普通 Call、Patch、Breakpoint 的签名开关互相独立。关闭某一签名不取消不可读/不可执行、固定重放结构、已有补丁所有权、线程/world 约束及实际路径可达性检查；恢复仍使用安装事务保存的原值和实际写后值。详见 [既有逐入口设置记录](2026-11-07-debug-entry-signature-controls.md) 与 [Ares 原生入口记录](2026-11-07-ares-native-entry-compatibility.md)。

现有诊断也并非空白：`main.cpp` 已记录加载模块名称和基址，并在 21 个启动签名差异中解析第一层 `E9` 目标、内存属性及模块名/RVA。它尚未在调试页提供完整版本身份和按入口分类的兼容结论；落入 `MEM_PRIVATE` 的加载器跳板不等于已确定属于 Ares 或 Phobos。`crc_shadow_bridge.h` 在 Ares/Phobos 存在时仍拒绝其本地 CRC shadow 路径，本次不改这一边界。

## 上游机制与版本要求

研究固定到下列官方源码，避免把开发分支随后的变化混入结论：

| 项目 | 固定提交 | 用途 |
| --- | --- | --- |
| Phobos | `ce864b03e56b15f4c59f78955aee87079e54ce76` | 本次生产 Hook、直接补丁及 Ares 版本适配分析 |
| SyringeEx | `7dbb780dfa7441f8d9b2ae89300540f1cf5f854e` | 加载器回调链、指令搬移及覆盖长度 |
| Ares | `4f1d929920aca31924c6cd4d3dfa849daa65252a` | CanBuild 的输入、结果及跳过原函数内部路径的依据 |

[Phobos 官方说明](https://phobos.readthedocs.io/en/latest/General-Info.html) 将其描述为可独立使用、同时考虑 Ares/CnCNet 共存的扩展。当前 latest 文档要求 SyringeEx 0.1.0.2 或更新版本及对应能力标志；这是当前文档的要求，不能套用成每个历史稳定版的加载规则。稳定、预发布、nightly 和本地构建必须分别识别。

[SyringeEx 官方说明](https://github.com/Phobos-developers/SyringeEx#feature-flags-api) 列出 `ESPModification`、`ZFPreservation`、`ReladdrInstructionFixup` 等能力。其 [固定加载器实现](https://github.com/Phobos-developers/SyringeEx/blob/7dbb780dfa7441f8d9b2ae89300540f1cf5f854e/SyringeDebugger.cpp) 在 486–506 行定义回调包装，在 532–542 行合并同地址回调并取最大覆盖长度，在 646–713 行构造回调、搬移原指令并写入 `E9`/NOP。返回零的回调继续执行链；非零返回目标会跳走，因此不同扩展之间不能仅凭“支持多个 Hook”推断执行顺序可交换。跳板档案需要包含加载器能力、原指令搬移、寄存器/标志/栈恢复和返回目标，不能只有首字节 `E9`。

Phobos 自己也采用有限版本适配：[AresHelper.cpp](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Utilities/AresHelper.cpp) 通过模块资源与 `.syhks00` 辅助识别，按 Ares 时间戳选择已知版本；[Hooks.Ares.cpp](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Misc/Hooks.Ares.cpp) 包含版本相关的模块相对补丁。文件名和时间戳适合诊断，完整适配档案还应绑定所测 DLL 的版本、映像尺寸和哈希，未知构建保留明确的“未验证”。

## 静态交集：精确地址与依赖范围分别记录

扫描官方固定提交 `src/` 内 380 个 C/C++/头文件/资源文件，逐一核对 Git blob SHA。排除 `.agents`、文档、历史 Ares Hook 表和补丁工具本身的宏定义。识别到 2,195 条安装/写入声明或调用：2,077 条固定游戏地址记录（2,028 个唯一地址）及 118 条 Ares 模块相对记录。声明重复、运行时恢复写入和不同版本分支均保留；不对预处理开关、INI、构建清单或实际调用可达性作“已启用”推断。

比较覆盖全部 161 个目录锚点、当前 Patch writer/上下文范围、21 个启动签名和六个 DR 的冷检查范围。其余二次 Call 的全部函数体/上下文和任意数据写入没有被穷举，因此“未交集”不能用作兼容证明。Hook 覆盖宽度按已审查加载器的 `max(5, declared length)` 建模；零长度声明不意味着不改写现场。

| Toolbox 目录行 | Phobos 固定位置 | 交集性质 |
| --- | --- | --- |
| Call `4F7870` | `4F7877`，5 字节 | CanBuild 的 8 字节签名末字节重叠；包括安装和运行时自撤销写入 |
| Call `48DC90` | `48DC90`，5 字节 | 同起点；清空选择前清理 Phobos 状态 |
| Call `6FBFA0` | `6FBFA3`，6 字节 | 原 8 字节签名内部；可分支跳过 LimboDelivery 对象 |
| Call `6A9316` | `6A9316`，6 字节 | 同起点；此目录行是提示分类依赖，并非可独立调用的函数 |
| Patch `478E4A` | `478E4A`，6 字节 | writer 同址；数据操作数形状与 Hook 跳转冲突 |
| Patch `4AC179` | `4AC18C`，10 字节 | writer 本身未重叠，后续上下文依赖重叠 |
| Patch `55D360` | `55D360`，5 字节；`55DED5`，6 字节 | 暂停入口 writer 和退出侧上下文分别重叠 |
| Patch `646EF5`、`646FD2` | 各同址，10 字节 | 控制命令所属玩家 writer 与事件队列容量 Hook 重叠 |
| Patch `6A97D2`、`6AA781` | `4F7877`，5 字节 | 自身 writer 未重叠，共用的 CanBuild 校验依赖重叠 |
| Patch `7E2908` | `7E2908`，4 字节 | 飞机类型航点虚函数表槽同址 |
| DR `4F7870`、`4F7951`、`4F7B66` | `4F7877`，5 字节 | 三者所依赖的 CanBuild 16 字节实际入口图像重叠 |

表中多行合并展示仍合计 15 个目录行；只有 7 行是双方声明起点完全相同。`4AC179`、两个科技图标 Patch 与三个 DR 的条目尤其不能说成对应 writer 已被 Phobos 覆盖。

### 五类优先适配案例

1. **CanBuild 入口内部的生命周期。** [Hooks.Upgrade.cpp 93–125 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Ext/BuildingType/Hooks.Upgrade.cpp#L93) 在 `4F8361` 读取 ECX、三个栈参数和 EAX 结果，与 Ares 的函数结果衔接；另在 `4F7877` 安装 5 字节 Hook，其回调恢复自身以及 `4F8361` 的原字节。Ares 的 [CanBuild Hook](https://github.com/Ares-Developers/Ares/blob/4f1d929920aca31924c6cd4d3dfa849daa65252a/src/Ext/House/Hooks.cpp#L25) 直接返回 `4F8361`，绕过原函数内部路径。Phobos 单独运行时，第一次走到 `+7` 会触发自撤销；撤销前原 8 字节签名可暂态不符，但入口首字节不是 `E9`。可实施的后续方案是为确定版本建立 `+7` 安装/自撤销状态档案，继续调用原入口，分别验证首次调用、撤销后、Ares 共存和模式切换。当前 250 ms 重查可重新观察恢复后的字节，但没有本任务现场证据证明何时恢复。普通调用通过也不能证明两个 Exact 内部 DR 仍可到达。

2. **原生选择调用。** [WarheadType/Hooks.cpp 664–678 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Ext/WarheadType/Hooks.cpp#L664) 在 `48DC90` 清理扩展选择状态后返回零；[Techno/Hooks.cpp 1780–1795 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Ext/Techno/Hooks.cpp#L1780) 在 `6FBFA0+3` 使用 ESI 对象，可跳到 `6FC029`。Toolbox 当前分别按无参 cdecl 和对象 thiscall 调用原入口。后续应分别验证栈平衡、this 指针、返回值和扩展对象过滤，再为这两个调用建立有限 profile；不能把内部 `+3` 当作函数入口直接调用，不能仅增加 Phobos DLL 名称检测。

3. **提示框渲染。** [PhobosToolTip.cpp 213–223、295–302 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Misc/PhobosToolTip.cpp#L213) 会在 `6A9316` 改变 EAX/续接位置，并在 `478E4A` 选择 ESI surface 后跳到后续绘制位置。Toolbox 的后者是 6 字节数据操作数补丁，只允许既定指令形状；把该位置的 `E9` 误解释成可替换的操作数会损坏别人的 Hook。应先确定由哪一方负责尺寸/表面和文本，选择协作接口或独立渲染路径；未建立该方案前局部拒绝。关闭签名仍不能解除这条结构约束。

4. **暂停与逐帧。** [FrameByFrame.cpp 74–95 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Commands/FrameByFrame.cpp#L74) 在 `55D360` 安装 5 字节 Hook；逐帧状态可运行输入/绘制循环并跳至 `55DEDB`，退出侧另有计数 Hook。它与 Toolbox 暂停补丁共享执行边界，单纯按字节跳过不能定义两套暂停计数的组合。后续需要明确单一暂停控制者及进入、单步、退出、失焦的状态协议；没有协作协议时保留冲突状态，不抢写现场。

5. **飞机航点虚函数。** [Aircraft/Hooks.cpp 479–483 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Ext/Aircraft/Hooks.cpp#L479) 将 `7E2908` 的 4 字节指针指向 `__fastcall AircraftTypeClass_CanUseWaypoint`，结果取自扩展配置。Toolbox 现有记录要求原目标 `41CB50`；这是目标指针和行为变化，不是函数首部跳板。后续只有在确认调用约定、目标归属和布尔语义后才可做明确的委托/组合适配，同时保存实际被替换指针。未知或后来变化的目标不能覆盖，关闭时也不能强写固定原版目标。

另需保留 [Event/Hooks.cpp 52–89 行](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/src/Ext/Event/Hooks.cpp#L52) 的控制命令交集：`646EF5`、`646FD2` 是实际 10 字节写入碰撞，`4AC18C` 则是 `4AC179` 的上下文变化。这里涉及事件队列容量和回放，不能用“两个 Hook 都接受相同玩家参数”代替事件语义验证。本次未更改事件协议或发送规则。

## 可以实施的分层方案

**先补可观察性。** 在现有模块与第一层跳转日志上增加稳定的模块身份（原始文件名、版本、PE 时间戳、映像大小、按需缓存的文件哈希）、当前入口字节、目标模块/RVA、私有可执行区域和兼容档案结果。模块文件身份与内存已加载状态分开记录；没有可靠证据时显示“目标位于私有跳板，归属未确认”。可以优先在 Host 侧读取并缓存，避免只为诊断扩大跨进程协议。扫描限定于选中条目或节流的可见调试页，不能通过探测调用游戏函数或安装 Hook 来获得状态。

**再增加有限功能档案。** 每份记录固定游戏构建、扩展 DLL/加载器构建及所需能力，列出准确地址、完整写入/依赖范围、签名形状、参数/返回值、寄存器和 flags、回调次序、续接与卸载条件。分别实现 Call、Patch、DR 适配。优先选择普通调用和可恢复的诊断，先用负对照证明未知构建、错误长度、循环跳转、不可执行页、外部替换仍拒绝；随后用实际 Windows/游戏覆盖有无扩展、扩展组合、功能开关、加载/退出及运行中入口变化。DR 还需要证明实际触发和恢复，不能把安装槽存在当成功。

**有公开接口的功能优先使用版本化 Interop。** [Phobos Interoperability](https://phobos.readthedocs.io/en/latest/Interoperability.html) 公开 `GetInteropAPIVersion`、AE、威胁回调、EventExt 等接口，约定 HRESULT 和结构布局，并声明 major 变化允许破坏兼容。适配端应使用自己验证过的 major/功能版本范围、精确调用约定与导出签名，并处理 HRESULT、游戏线程和对象生命周期；不能只检查导出存在或 `major >= 1`。弃用导出的保留 stub 也可能触发致命错误。该页面没有提供 CanBuild、FindFactory、RecheckCameos 的通用替代，因此不能称三入口已经通过 Interop 适配。

**未知注入继续局部诊断。** 保留可读/可执行、结构、所有权和恢复约束，显示具体受影响功能。用户手动签名设置是排查手段，不自动写成永久兼容名单，也不让一个局部失败重新阻塞全部 Runtime。若确实需要共用一个写入点，应建立明确的协作接口或经过验证的组合实现，不能通用追踪所有跳板后再从中间覆盖。

## 证据与未完成项

本次完成的是固定官方源码检索、380 文件 Git blob 校验、静态宏/调用解析、目录区间比较和上述五类语义人工复核。为读取本地静态表而执行的短 C++ 程序不加载游戏 DLL、不打开游戏进程，也不执行下载的上游代码。条件编译/构建选项、安装时次序、INI 开关、运行时自撤销、第三方加载器和实际 DLL 仍需单独观察；源码条目数量不能代替 `.syhks00`、动态补丁和现场机器码的联合验证。

没有执行 Windows 游戏、Ares/Phobos DLL 或多人验证，没有新增兼容放行。公开接口接入、模块身份页、具体 Phobos Call/Patch/DR profile 均为后续实施建议。代码中任何“已安装”“已兼容”状态仍必须来自实际所有权/入口状态，不能从这份静态交集直接生成。
