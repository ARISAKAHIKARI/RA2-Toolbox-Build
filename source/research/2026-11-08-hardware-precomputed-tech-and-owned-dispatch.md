# 硬件断点第三轮：科技候选预计算与本方命中处理

## 问题、基线与依据

用户继续强调硬件控制卡顿的重要性，并明确提出：需要开放哪些科技，应该提前计算，断点命中时快速判断。本轮从已交付 `1.0.44-pre.2` 的 `cc3f0117c233284cec1963a11c26d637d55817fe` 开始；先查 INDEX、控制单槽 ABI、第二轮 dispatch 研究及同一持久 SQLite 笔记 v160。

沿用已支持样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的既有地址/ABI 证据。本轮不读取私人 EXE 或 dump，不增加游戏地址或平台支持声明，不修改游戏指令或增加物理断点槽。

| 既有位置 | 本轮依赖的语义 |
| --- | --- |
| Full `0x4F7870` | ECX 为 House；原栈为 return/Type/buildLimitOnly/includeProduction；只接收既定 draw/prune caller `0x6A97D7` / `0x6AA786` |
| Enhancement `0x4F7951` / `0x4F7B66` | tag2 的等级 -1 比较、tag3 的玩家等级比较；tag3 仍使用本次 CONTEXT.Ebx |
| Type `+0`, `+0x10`, `+0x634` | 当前 vtable、UID、TechLevel 身份检查 |
| `0xA8E348`, `0xA83CE0`, `0xA8B218` | 已有增强规则的步兵/单位/飞机类型数组；继续校验 items、count、index、slot pointer |
| 控制 `0x50B6F0` | ECX 为 House；`[ESP]` 为返回位置；strict caller `0x6339BF` / `0x637E45` 仍转 `0x50B716`，其他合格 caller 转 `0x50B710` |

生产源码和原始收据散列见[相邻证据 JSON](2026-11-08-hardware-precomputed-tech-and-owned-dispatch.evidence.json)。最终整包构建、提交和 ZIP 对应关系由交付 manifest 另行记录。

## 一、提前计算候选，命中时保留即时授权检查

Full 本来已经在普通 capture 阶段把 `techAllowed` 放入 1024 槽表，最多 512 项。本轮没有把这张既有名单冒称为新功能。旧回调在查表前先读取世界；现在拆为纯本地 `fullIdentity(pointer)` 与实时身份检查，未入名单者先退出。名单命中仍检查当前世界、Type vtable/UID、原有完整签名，签名失败仍撤销 `policy.enabled`。其他调用点的 `fullType()` 保留原来的即时身份检查。

精确科技加强原先在每次命中中先检查世界与签名，再线性搜索 21 项 `policy.enhancement`。现在在原 capture 完成后建立固定 64 槽 `PointerIndex`，回调先通过 Type 指针查候选；没有候选就结束。索引只保存原记录行号，不保存可以跨命中复用的成功授权。命中后原 world、vtable、UID、TechLevel、数组来源、索引范围、数组槽指针、caller、probe token 和 stop/fault 检查继续执行。

表容量保持不超过一半负载，无堆分配、无惰性初始化。重复 pointer 保留第一条规则，包括第一条身份失效就拒绝的旧行为。重建先使 ready=false，完整填表后才发布；空重建清除旧候选。世界身份变化清空索引，未就绪时原维护入口会立即重捕获，不等待 500 ms 门限。保留 Exact/Rules 及 compareLevel 的在产交接，不能用当前请求字段直接撤掉旧 policy 的生产承接权限。

这不是对整个原生 CanBuild 结果做长期缓存。工厂、建造上限等原生后续判断仍由游戏执行；本轮仅提前定位本工具准备放宽的规则及类型。

## 二、固定比较结果在编译期求值

tag2 始终模拟 `compareFlags(1, UINT32_MAX, previous)`。实际 pre2 Win32 `-O2` 对象中，旧代码先选择 UINT32_MAX 或 EBX，再统一调用通用 `compareFlags`，没有自动消除 tag2 的动态计算。

现在由原算法 constexpr 计算 `LimitResultFlags=0x11`，静态断言仅 CF/AF 置位；tag2 执行 `(previous & ~0x8D5) | 0x11`。TF、RF、IF 等非算术位完整保留。tag3 仍使用当前 EBX 动态计算。实际完整 Runtime 对象确认 tag2 走固定掩码，tag3 的通用比较调用仍在。

## 三、控制世界校验的静态读取器

pre2 的 `controlparams::world` 把读取器作为普通函数指针传入模板，实际 x86 对象仍包含 10 次间接函数调用。本轮用无状态具体 callable 保留读取器类型，使编译器直接生成当前字段读取。没有跨命中缓存 world 成功结果，也没有省略字段。

实际新旧 tag4 分支的 115 组差分确认：成功路径仍为 17 次即时读取，地址、宽度和顺序相同；严格 caller、每个读取失败、世界/House/停止变化、同帧成功后立即失效均保持原结果和 CONTEXT。实际完整 Runtime 对象中，world 校验辅助函数间接调用为 10→0，静态指令为 158→58。这是该辅助函数的机器码工作量，不是 Windows 延迟或 FPS，也不是减少了 10 次 RPM；原读取器本来已在 SEH 范围内直接读取。

## 四、本方 dispatch 的调用上下文扁平化

旧 `vectored()` 的两个引用捕获 lambda 外面再包 `runtimeunwind::Invocation`，body/cleanup 需多层指针才能取得 Service、异常上下文和结果。改为直接保存这些字段的 `DispatchInvocation`，仍使用原 `RuntimeScopedCall`、原 `dispatch()` 和原 finally。

classify、foreign/mixed 分流、深度增减、nested 抑制、uncertain lease、DR6 清位、TF/RF 与计数语义不变。成功命中原本没有 clock、atomic 或维护 trap，不能把收益归因于删掉这些本不存在的每命中工作。GuardedTechUiParameter 的 LastError 与 SEH 边界保持不变。

## 五、执行证据与实际范围

| 检查 | 本轮结果 |
| --- | --- |
| 科技实际源码前后对照 | 当前 322 个 callback 场景、132826 项检查；其中 131136 组 flags 差分；固定 pre2 同夹具也通过 |
| 未入名单 Full / Enhancement | 世界字段读取均 10→0；Enhancement 整段签名核验调用 1→0 |
| 原控制 / Full 回归 | 2752 项检查通过 |
| 中途启用、PRES/BUS、交接及实际冷端索引发布 | 原 45 场景保留，新增 5 场景；共 50 场景、179 项检查通过 |
| 控制读取器实际分支差分 | 115 场景，全部即时读取顺序和拒绝语义相同 |
| 完整生产 Service / 受控 Win32 边界 | 固定前后同夹具均 43 场景、1995 项检查；包含 16 种 foreign/native/mixed 快速分流 |
| Win32 x86 O2 Service wrapper | entry 静态指令 56→46，body 13→9，cleanup 6→4；dispatch 保持 109，归一化指令逐条相同 |
| 完整 Runtime O2 机器码 | 控制 world 间接调用 10→0，tag2 固定 mask、tag3 动态比较均确认 |
| 独立只读审查 | 核对最终生产与收据绑定，没有本轮阻断项 |

Service 另作同 CPU、每轮 300 万次、7 轮交错 native 比较，中位 22.33487→18.65274 ns；6 轮当前较快，另有一次当前 28.19 ns，范围重叠。它执行完整生产 Service，但 Win32/SEH 交付及最终功能 callback 是受控边界。不能把约 16.5% 的子路径中位差写成硬件异常整体或游戏加速。

复现入口：`assist/tests/tech_parameter_lookup.py`、`control_parameters.py`、`tech_enhancement_midgame.py --after-only`、`hardware_control_reader.py`、`hardware_owned_dispatch.py` 及 `tools/audit_hardware_dispatch_codegen.py`。各命令的实际参数、编译旗标和全部日志在交付验证包中。

## 六、未采用的路径与未决问题

实际断点地址和命中次数未变。微软说明异常发生时先保存线程/异常状态再派发；Intel 说明指令断点在执行目标指令前产生 #DB，RF 仅抑制接下来恢复执行的那条指令。因此回调内查表或早退可减少本工具工作，却不能取消已经发生的异常入口与恢复成本。这是机制推论，没有据此推算本机或游戏耗时。

- [Microsoft Exception Dispatching](https://learn.microsoft.com/en-us/windows/win32/debug/exception-dispatching)
- [Microsoft Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling)
- [Intel SDM 文档变更，18.3.1.1](https://cdrdv2-public.intel.com/774475/252046-sdm-change-document.pdf)

黑幕断点从既有 JNE `0x6D88FC` 移到其 fallthrough `0x6D88FE`，成功时跳向 `0x6D8903`，是基于既有机器码笔记推导的少命中候选；本轮没有完成支持 variant、进入路径与实际异常覆盖对照，不实施新地址。控制公共入口也未找到已证明覆盖所有 caller、单槽且更少命中的替代点。

拒绝按帧缓存最终授权、临时关闭/轮换 DR 制造空窗、覆盖外部槽或自动回退代码补丁。SignatureSnapshot 的逐 volatile byte 验证保留；字宽合并需要单独处理未对齐、aliasing、跨块故障与观察时序，本轮不扩大该修改。冷端每 500 ms 的 21 条规则数组扫描保持原样。

本轮没有 Windows 实机、完整 #DB/VEH 延迟、游戏/GPU/FPS 或联机运行。交叉编译和静态 PE 核验不能替代这些验收。
