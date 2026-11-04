# 蜘蛛自动防护：暂停时的研究检查点（未实现）

用户提出两项自动功能后，随后明确暂停新功能。本笔记仅保存暂停前已完成的查阅和方案；**没有新增 backend、开关、Shared 字段、事件或 Hook，待用户恢复后再继续**。

## 问题与证据范围

需求一：敌蜘蛛进入矿车武器射程加 N 格时点杀，N 默认 1；离开射程加 N+1 格后恢复先前采矿、卸矿或等待卸矿工作。需求二：被寄生单位到己方维修厂，寄生解除即离开；矿车再采矿。此前已查阅 INDEX、同一 SQLite v125 的寄生标记／任务记录，以及 `2026-11-03-native-unload-harvest-handoff.md`。

- 本轮采集 ZIP SHA-256：`39b10d1e2a4338ce9b2f92449429b28c5e365ce3859a1b47220217240f19fe71`。
- 采集 `session.json` 报告映像基址 `0x400000`、大小 `0x794000`，磁盘文件 SHA-256 为 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`。本代理没有取得另一份磁盘 EXE 独立复算该文件 SHA；不能将报告身份等同完整运行时映像未修改。
- 私有映像、原始内存、临时反汇编缓冲均不进入仓库或发布包。本轮没有运行游戏、执行原生函数或验证自动功能。

## 已核对与未核对的字段／射程

固定 primary source 为 `CnCNet/YRpp@d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 UnitClass、UnitTypeClass、TechnoClass、TechnoTypeClass、FootClass、ParasiteClass；同时对照本项目 `4baa78ba` 的 `raw_combat.h`、`external_orders_bridge.h`、`ore_harvest_bridge.h` 和命令网关。

| 项目 | 暂停前结论与边界 |
| --- | --- |
| 矿车类型 | 既有 `oreMiner(type)` 使用已配置矿车名单或 Type `+0xE0E/+0xE0F`，并要求 Storage `+0x800>0`。可复用，不应强制要求 AutoOreReturn 开启；这是既有源码事实。 |
| 寄生字段 | 既有 raw combat 以 Foot `+0x694` 非零标记 Infested；YRpp 将其命名为 ParasiteEatingMe。其后的 ParasiteImUsing 对应候选 `+0x69C`，文档说明包括 terror drone/squid；后者本轮未做独立原生读写验证。 |
| 蜘蛛识别草案 | 地面 Unit、Naval（既有 `type+0xCCE`）为 false，结合实际武器的 Warhead Parasite 标记（既有 `+0x159`）／寄生控制器，不写死 DRON ID。Naval 与寄生武器可同时覆盖 squid 等对象，最终判别、可目标性和 MOD 边界仍需闭合。 |
| 武器基础范围 | 既有 raw policy 从 Weapon `+0xB4/+0xB8` 取范围／最小范围，从 Type `+0x898/+0xA94` 取普通／精英武器；格坐标按 256 leptons。应复用现有武器选择及伤害适配，不能仅选最大射程或只看单位 ID。 |
| 本轮 vtable 读数 | Unit vtable `0x7F5C70`：`+0x2E4→0x746CD0`、`+0x3A8→0x6F77B0`、`+0x3AC→0x6F7780`、`+0x3F8→0x70E140`。**这四个表项各四字节已逐字节核对 valid/stable 掩码均有效**。 |
| 范围函数候选 | 从本轮 `.mem` 静态解码可见 `0x6F77B0` 调用 `0x6F7220`，后者读取 Weapon `+0xB4`，并有空中、占据／载员及弹道高度附加路径。但本代理尚未逐字节核对整个解码区间的 valid/stable 掩码，未执行函数，故不能将其完整语义或 ABI 作为新调用许可。 |
| 非基础动态范围 | 普通地面工作矿车可排除载员／占据状态；MOD 弹道、高度与虚方法可能改变实际范围。尚未决定是保守过滤特殊弹道，还是明确使用现有 raw 查询口径；不得把基础范围声称为所有 MOD 的完整动态范围。既有 nativeCombatQueries 是显式实验选项，不应由新功能偷偷开启。 |
| 工作字段 | Mission `+0xAC`、queued `+0xB4`、target `+0x2B4`、Foot destination `+0x5A4` 均已有生产读取。ArchiveTarget 候选 `+0x218` 仅由 YRpp 字段顺序和 Owner `+0x21C` 推得，本轮没有独立偏移验证，**不可直接投入 backend**。 |

## 待确认的任务恢复与优先级方案

所有动作拟经既有 `nativeMission → EventClass → SP/MP 统一网关`，不直接写 mission、target、radio 或货舱。既有研究确认原生 Harvest 点击为任务 10、空 target、cell destination/tag11、空 nearest；恢复 Enter 则应使用任务 7、合法同属炼矿厂对象 ID/tag34。原等待格可用于 Move 后再 Enter 的分阶段恢复，但 **Enter 事件不证明恢复了原生或工具队列的原 FIFO 名次**，不得伪写队列／radio 声称精确恢复。

拟保存 actor pointer/UID、type pointer/UID、world/House 身份、manual token、原工作任务与目标／目的地、炼矿厂身份和等待格；对象复用、世界替换、读失败或玩家后续指令都不恢复旧记录。`manualVersions.orePaused` 是现成的输入先于同步执行保护，初次 enrol 也必须拒绝该状态；只比较已存在 job 的旧 token 不够。

建议新增独立的 HarvesterDefense owner（优先级 4）和 ParasiteService owner（优先级 6），追加 enum 以保留旧数值；这仅为草案。寄生短修优先接手，防蛛 job 应丢弃旧攻击恢复，不抢回单位。防蛛活跃期间须在 receipt 观察释放租约后仍维持本地所有权，或在现有矿车规划边界明确暂停该 actor；否则 AutoOreReturn 可能抢回 Enter。按空间索引和有限决策预算扫描，不能每帧做矿车乘全图对象。

维修厂选择、Enter／Repair／退出及“只修掉蜘蛛即退出”的原生 ABI **尚未闭合**，不可据此开始实现或声称可用。矿车短修后的采矿恢复需与既有最近资源搜索接合。

UI/config 草案为两开关默认关、N 默认 1、整数 0–30 格；moFeatures `0x20/0x40` 和 bits8..12 的 N+1 打包仍只是待审核合同，未修改配置或 Shared。`moFeatures&7u` 的现有需求门槛、world index、manual ingress 和挂起／禁用清理也需要在恢复工作时一并核对。

## 验证状态与未决

仅有上述静态读取、既有源码和固定头文件对照；没有本功能代码、测试或编译产物。没有证明新自动点杀、队列恢复、维修退出或联机行为。用户恢复授权前不继续研究／实施；后续须先补掩码、字段、事件依赖及受控行为验证，再讨论交付。
