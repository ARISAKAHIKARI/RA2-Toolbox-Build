# 点杀蜘蛛的参与单位独立配置

日期：2026-11-06。目标版本：`1.0.43-pre.3`，Assist 协议 `99`。

## 问题与证据边界

用户在 `1.0.43-pre.2` 交付后要求“点杀蜘蛛的单位可以单独配置”。此前防蛛来源沿用自动卸矿的 `OreTypes` 矿车清单，无法单独选择参与者，也不能选择普通地面战斗单位。

实施前查阅本索引、[矿车防蛛实现与工作恢复](2026-11-06-miner-spider-defense.md)，以及持久笔记 `RA2-Reverse-Engineering-Notes.sqlite` 的版本 142 中对应实现和 11-04 历史暂停记录。本轮主要证据为生产源码、真实头文件的 Linux 合成内存检查和 Windows 目标交叉编译；没有新增真实 Windows、游戏或联机执行证据。

既有字段／原生命令依据仍是前一记录引用的游戏映像 SHA-256：

`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`

本轮没有重新运行或重新校验该私有映像，也没有新增虚调用地址、Hook 或游戏代码写入。类型字段对照沿用固定 `CnCNet/YRpp@d9795546950fcc75a94325ffc83b44d16ec34ce2` 和项目既有 `typeOf`／raw-combat／命令网关。

## 独立配置及默认行为

`featuretypes::SpiderDefense` 追加为槽位 `20`，`Count=21`。使用既有 `FeatureTypes` 配置、类型 ID 解析器及 `featureConfigSeq` 一致性发布：

```ini
[FeatureTypes]
SpiderDefenseCustom=1
SpiderDefense=HARV,HTNK,E1
```

- `SpiderDefenseCustom=0` 时按原生矿车能力自动识别，不读取自动卸矿的矿车名单来限制来源。
- `SpiderDefenseCustom=1` 时只接收列出的己方地面载具／步兵类型；空清单不接收新任务。预填 `HARV,CMIN` 仅作为开启自定义后的初始清单。
- 清单是执行点杀的来源类型，不是敌方蜘蛛类型。敌方地面寄生威胁的识别继续沿用已实现的原生武器／寄生字段检查。
- 仍须满足存活、在地图、当前可操作、非空中／运输／停用／磁电等操纵／被寄生状态，且实际所选武器能攻击目标；名单不会赋予新武器能力。本轮没有另行排除所有已经归属己方的心灵控制单位。
- UI 位于“自动功能 → 自动点杀蜘蛛 → 自定义点杀单位类型”，控件 `9240/9241`。通用折叠类型列表跳过此槽，避免出现两个同 ID 控件。旧配置缺少这两个键时保持默认矿车模式和原有总开关状态。
- 移除旧 `Need::MinerAutomation` 依赖位，自动卸矿设置重新仅依赖其自身开关；`TypeListFirst=43` 加 21 个类型位仍不超过 64 位。

## 来源身份、恢复与手动优先

防蛛私有身份在原 `OreHarvestJob` 字段之外保存 `actorKind` 和 `harvester`，不改变共享 Actor 布局。普通载具／步兵使用共用 Foot 状态读取；矿车保留其矿量、卸矿与资源搜索路径。

| 字段／常量 | 既有来源及本轮用途 |
| --- | --- |
| Infantry vtable `0x7EB058`；类型 `+0x6C0` | 既有 `kind`／`typeOf`；区分步兵身份并校验类型替换 |
| Unit vtable `0x7F5C70`；类型 `+0x6C4` | 既有车辆／矿车路径；不对步兵读取 Unit 专有字段 |
| 对象 UID `+0x10`、HP `+0x6C`、OnMap `+0x74`、Limbo `+0x81`、Alive `+0x90` | 共用存活与对象复用检查 |
| Owner `+0x21C`、Transporter `+0x11C`、停用 `+0x1C8` | 共用归属与可接管状态检查 |
| mission `+0xAC`、queued `+0xB4`、target `+0x2B4`、destination `+0x5A4` | 保留与核对任务元组；不直接写入这些字段 |
| UnitType Storage `+0x800`、矿车标志 `+0xE0E/+0xE0F` | 独立于 `OreTypes` 的原生矿车能力识别 |
| Type Dock 列表 `+0x3E8` | 独立核对矿车的原生兼容炼矿厂，避免自动卸矿名单影响防蛛恢复 |
| Guard `5`、AreaGuard `11`、Move `2` | 复用既有 `nativeMission` 通道；未在本轮执行游戏原生 EventClass 构造函数 |

矿车继续保留采矿、返矿／卸矿和工具箱等待任务恢复，不保证游戏原生队列的绝对名次。自定义名单中的普通地面单位只从没有攻击／移动目标的 Guard 或 AreaGuard 接管；记录原位置和警戒任务，威胁解除后通过原生 Move 返回，再恢复原警戒任务。正在执行其他明确任务的单位不被当作闲置来源。

返回原格受到阻挡时，等待同时达到 90 个模拟帧和 1500ms 后，在当前位置恢复原 Guard／AreaGuard，避免无限往返。对应普通任务／原生命令通道依据还包括 [已选单位任务模式](2026-10-26-selected-mission-modes.md)；本轮没有重新执行这些原生函数。

新的名单只决定新任务准入与是否继续拦截。已接管单位被移出名单，或总开关关闭时，进入既有有界恢复流程；恢复校验不能再依赖“仍在名单内”，否则会把旧攻击留给游戏继续执行。

玩家手动 revision 仍优先取消旧任务。新增按单位身份绑定的本地手动等待记录，处理“手动事件已提交，但快照仍显示旧 Guard／攻击”的窗口；在观察到后续原生任务之前，不得用旧快照重新接管。只对防蛛总开关的重新开启或此功能自身的类型选择变化考虑重新接管；其他功能类型清单、重复同步或距离设置变化不能清除此保护。任务容量、命令预算和恢复截止继续有界。

`indexWorld` 会清理死亡单位的 `ManualVersions` 记录，`copyLive` 可能改变发生哈希碰撞的存活单位所在槽位。新增 `minerDefenseReindexManual` 在赋值之前用固定、非栈上暂存区按存活 ID／token 重排等待记录，避免清理之后丢失手动优先保护。没有被输入 Hook 捕获、但由原生任务变化识别出的手动接管，同样保留独立的零 token 槽位。

防蛛需要枚举原生兼容炼矿厂；自动卸矿仍按自己的矿场名单筛选 `Actor.dockTypes` 和排队等待区采样。这样共享快照增加防蛛所需的炼矿厂元数据，不会把自动卸矿的可选目的地扩大到其名单之外。

## 共享协议及验证

追加一个 `featuretypes::Filter` 使配置增大 `12296` 字节，协议升至 Assist99。Windows x86 与 x64 编译出的 `Shared` 均为 `23154176` 字节、对齐 `8`，800 个布局项目一致；未执行这些 Windows 检查程序。

配置检查执行实际生产加载、保存、事件处理及一致性发布代码，standalone／unified 各 140 项通过，控件依赖覆盖 101 项。原有 15 个矿车队列场景与类型解析／筛选检查通过。运行时真实生产头文件合成内存检查共 59 个场景、2726 项检查通过，包含独立默认／自定义名单、空名单、步兵／车辆身份、警戒恢复、名单移除、手动接管及碰撞槽位清理；运行中及结束后 22 个记录的源文件哈希一致。最终构建结果由本轮交付验证记录固定到最终源提交；不得以旧版本运行结果代替本版结果。

代码／检查路径：

- `assist/source/feature_type_config.h`、`feature_type_bridge.h`、`tactical_controls.h`、`settings_dependencies.h`。
- `assist/source/harvester_defense.h`、`harvester_defense_raw.h`、`harvester_defense_bridge.h`、`external_orders_bridge.h`。
- `assist/tests/miner_defense_settings.py`、`settings_dependencies.cpp`、`feature_type_config.cpp`、`harvester_defense_runtime.cpp/.py`、`harvester_defense_queue.cpp`。
- `tools/verify_crossbit_abi.py`、`tools/verify_windows_outputs.py`；发布附件 `release-manifest.json` 记录完整构建、输入身份和验证范围。

## 排除的做法与未决事项

没有复用自动卸矿名单、把所有选中类型强行标为 Harvester、写原生 mission／radio／队列、扩大实际开火射程、接管普通单位的手动移动／攻击任务，或因为类型移出名单就直接丢弃已提交攻击的恢复工作。

实际路径可达性、被阻挡时返回原位的结果、特殊 MOD 动态武器与原生任务语义，以及 Windows UI、真实对局、联机同步仍需实机验证。合成内存检查只能证明明确边界下的生产代码决定和命令表达，不能证明游戏已执行或成功击杀。
