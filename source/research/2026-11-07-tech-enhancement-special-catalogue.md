# 科技加强：总统／校车的特殊类型目录缺口

## 问题、已查依据与范围

用户在当前版本观察到猴子 `JOSH`、鳄鱼 `ALL`、北极熊 `POLARB`、化学兵 `WEEDGUY`、榴弹炮 `HOWI`，但没有总统 `PRES` 和校车 `BUS`。此前把“在加强白名单中”表述成“当前可以生产”过于宽泛。本轮冻结已发布 `58f847d3d2616c1c1dce8a55e0b4cddacc027d8e`（1.0.43-pre.13），分别检查目录、旧规则激活和精确模式补图示的实际路径。

调查前已读 INDEX、[中途开启的刷新记录](2026-11-07-tech-enhancement-midgame-refresh.md)、[精确科技等级记录](2026-11-03-tech-enhancement-exact-parameters.md)、[Ares 入口兼容记录](2026-11-07-ares-native-entry-compatibility.md)及现有 SQLite 笔记中的科技加强／中途刷新条目。用户本轮没有提供实际加载的规则文件、运行日志、当前国家、模式选择和平台模块身份。本轮没有执行用户游戏、Windows 程序或平台 Hook。

确认的产品缺口是：工具箱目录继续无条件排除 `Insignificant`，与既有加强白名单中的特殊类型冲突。对精确模式，这会在原生可建造探查之前截断补图示；对旧规则模式，本轮只证明工具箱目录缺项，不能据此认定原生侧栏必定缺项。

## 公开规则候选的具体差异

检查了 [Mirage-Zero-Nine 的 rulesmd.ini](https://gist.github.com/Mirage-Zero-Nine/0c2df4046c279a15ee1f80e6d1608e7d)，并通过 GitHub 文件 API 完整读取 [hamdirizal/red-alert-2-rules 的 rulesmd-original.ini](https://github.com/hamdirizal/red-alert-2-rules/blob/main/rulesmd-original.ini)交叉核对。后者返回 Git blob SHA `4c4b9bc5ecbcf4a528d132d49916d3fbf11fbe87`。七个节的下列字段相同。

这两份是公开用户保存的原始规则候选：第一份标题未声明修改，第二份将 `original` 文件与多份修改版分开保存；没有原光盘／官方分发的校验链，不能称为已认证纯净原版，更不能当成用户平台实际加载的数据。下表的“未设置”只表示该节缺少对应键，不代替运行时缺省值或平台覆盖结果。

| 类型 | TechLevel | Insignificant | Owner 范围 | Prerequisite | RequiredHouses |
| --- | --- | --- | --- | --- | --- |
| 猴子 JOSH | -1 | no | 全十国 | 未设置 | 未设置 |
| 鳄鱼 ALL | -1 | no | 全十国 | 未设置 | 未设置 |
| 北极熊 POLARB | -1 | no | 全十国 | 未设置 | 未设置 |
| 化学兵 WEEDGUY | -1 | 未设置 | 盟军五国 | BARRACKS | 未设置 |
| 榴弹炮 HOWI | -1 | 未设置 | 盟军五国 | GAWEAP | Alliance |
| 总统 PRES | -1 | yes | 全十国 | 未设置 | 未设置 |
| 校车 BUS | -1 | yes | 苏军四国及 YuriCountry | NAWEAP | 未设置 |

七个节均未设置 `Unbuildable`、`ForbiddenHouses`、`Spawned`、`BuildLimit` 或 `Factory`。`Factory` 在生产建筑节设置：三阵营兵营为 `InfantryType`，盟／苏战车工厂为 `UnitType`。因此总统与三动物的可见分组有明确的 `Insignificant` 差异；校车还有苏军战车工厂前置和 Owner 范围差异。看到 HOWI 不等于满足 BUS 的生产条件，也不能在未确认全开／规则覆盖之前由 HOWI 反推用户国家。

## 冻结 pre.13 的源码反例

基线 `protocol.h::validType` 要求 `level` 在 0..100、非 insignificant、非 spawned 且 owners 非零。`bridge.cpp::scanTypes` 即使通过 `techEnhancementCatalogueLevel` 得到有效等级 1，仍把实际 `Type+0x232` 直接传给该过滤器。

源码抽取反例保存在工作区 `analysis/pre14-tech-investigation/fixture-admission/result.json`，绑定上述完整提交及五个生产源文件的 SHA-256。它使用实际抽取的激活／目录函数，对 PRES、BUS 分别执行 legacy／exact、Insignificant=0／1，共八项。除该标志外，其余前置／所属／工厂为可通过的合成模型。

| 模式与输入 | 实际 TechLevel | 目录 typeSlot | 原生重查请求 | probe／factory／cameo 模型调用 |
| --- | --- | --- | --- | --- |
| legacy，Insignificant=0 | 1 | 0 | 1 | 0／0／0 |
| legacy，Insignificant=1 | 1 | -1 | 1 | 0／0／0 |
| exact，Insignificant=0 | -1 | 0 | 0 | 1／1／1 |
| exact，Insignificant=1 | -1 | -1 | 0 | 0／0／0 |

PRES 与 BUS 的四项结果相同。exact 两项均已捕获加强策略；失败时实际类型未进目录，`typeSlot(type)<0` 使受控补图示分支在 `GuardedTechEnhancementProbe`、工厂查询和 `AddCameo` 前返回。因此这不是这些模型调用“返回拒绝”，而是尚未调用。

legacy 直接遍历 `techenhancement::Rules` 和引擎类型数组，修改科技等级并请求 `House+0x1FC` 重查；这个 writer 不依赖 `scanTypes` 成功收录类型。反例明确观察到目录缺项同时等级已写入、重查已请求。不能把 exact 的断路推断成 legacy 原生侧栏同一根因。

此前 `assist/tests/tech_enhancement_midgame.cpp` 的 `exact_filter_insignificant` 主动给白名单 BUS 设置该标志，并断言 `typeSlot(Bus)<0` 且 probe／additions 为零。这一测试把特殊类型被排除当成需要保护的行为。其他旧 BUS fixtures 大多将标志清零、OwnerFlags 设为单一允许值、前置为空或工厂查询建模为通过，所以它们只能证明宽松合成对象的刷新，不能证明真实 BUS／PRES 的全部条件。

## 有限修正

`bridge.cpp::scanTypes` 在传入 `validType` 前，通过 `techEnhancementCatalogueIncludesInsignificant(type,array,index,level)` 判断是否对当前加强目标提供目录例外。只处理既有 21 类白名单中正在生效并通过身份检查的对象，不把普通民用类型全部加入。

- 精确模式要求加强请求有效，并核对规则中的 array、ID、vtable、数组槽 index 和当前数组项身份，同时保留该条规则的等级适用条件。
- 旧规则模式要求当前世界身份有效、同一条 journal 的 `owned && !conflict`、相同 pointer／index、类型身份仍匹配且实际等级为 1。只有 ID 相同、无本人持有记录或外部已经改成 1，均不足以取得该例外。
- 通用目录的 spawned、owners 非零及等级范围过滤继续执行。例外只影响工具箱的目录准入，不写游戏的 `Type+0x232`，也不改 Owner、国家限制、前置、工厂或生产权限。
- 精确模式后续仍依赖已确认策略／硬件状态、原生 CanBuild、真实工厂、图标去重和页面容量。旧规则模式没有新增 AddCameo，不通过额外补图掩盖未确认的原生筛选。

关闭、恢复、冲突及世界变化继续使用既有所有权和目录刷新生命周期；没有新增 Hook、指令补丁或生产队列操作。该修正解决确认的目录漏项，不承诺绕过 BUS 仍可能不满足的所属／前置条件。

### 与科技全开数据模式的交互

既有 `techDataBlocksEnhancement` 阻止 legacy 加强与全开数据／参数实现同时持有规则写入；`applyTechData` 先要求完整 `restoreTechEnhancement`。本次不改这项互斥，也不扩展 `techDataOwns`。legacy 的 owned 记录成功恢复后，本次目录例外失效，不能把它描述为全开 mode3／参数实现暂停 legacy 时仍持续支持特殊类型。

源码审计指出首次 `applyTechData` 仍可能持有加强关闭前包含 PRES／BUS 的旧 `snap`，在恢复 legacy 后、下一次目录扫描前短暂捕获全开数据模式的九字段记录。针对这一新可达组合，在 `tech_data_world.py` 追加一例两类型交接：实际抽取的数据桥与逐字段 journal 捕获原始 -1、临时应用等级 0，提供后续空目录后再次调用会恢复全部九字段并清空 captured／owned／conflict；两个 Insignificant 字节始终为 1。该轮原有 21 项与新增 8 项共 29 项通过，证据为 `analysis/pre14-tech-data-handoff/receipt.json`。legacy 还原和目录移除仍为明确模型，不能据此证明实机短暂窗口内点击生产的行为。

## Ares 与原生证据边界

亲读固定上游 Ares 提交 `4f1d929920aca31924c6cd4d3dfa849daa65252a` 的 [House/Body.cpp](https://github.com/Ares-Developers/Ares/blob/4f1d929920aca31924c6cd4d3dfa849daa65252a/src/Ext/House/Body.cpp)：`RequirementsMet`、`PrerequisitesMet`、`PrereqValidate` 和 `HasFactory` 这些函数体未见 `Insignificant` 判断，但保留 Unbuildable、前置、科技等级、国家／偷科技、工厂及数量条件。`HasFactory` 还检查生产种类、海陆、可用工厂、Owner mask 相交和供电。这个源码观察不能推出原生 Building 枚举或任意平台链路也没有该过滤。

[House/Hooks.cpp](https://github.com/Ares-Developers/Ares/blob/4f1d929920aca31924c6cd4d3dfa849daa65252a/src/Ext/House/Hooks.cpp) 的入口替换仍是既有 `0x4F7870` → `HouseExt::PrereqValidate` → `0x4F8361`。精确模式依赖的原函数内部比较可能被绕过，本次目录修正没有改变既有 Ares `NativeBodyBypassed` 限制，也没有将关闭签名检查作为适配。

沿用既有支持映像 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，本轮没有重新校验或执行该映像。以下均为已存研究和当前源码使用的地址／布局，不作为新反汇编结论：

| 地址／字段 | 既有用途 |
| --- | --- |
| Type+0x24、Type 首地址 | ID、vtable 身份 |
| Type+0x634 | 32 位有符号 TechLevel |
| Type+0x232／+0xD54／+0x6CC | Insignificant／Spawned／Owner mask |
| 0xA8E348／0xA83CE0 | InfantryType／UnitType 数组 |
| House+0x1FC；0x4F9265／0x4F927C | RecheckTechTree 请求、读取与清除 |
| 0x4456D0，Building 虚表+0x4E0 | UpdateConstructionOptions；thiscall、无参数 |
| 0x4F7870 | CanBuild；thiscall(Type*,buildLimitOnly,includeProduction) |
| 0x4F7951／0x4F7B66 | 精确等级相关的既有比较点；配置是否启用第二点沿用当前实现 |
| 0x6A6300／0x4FA350 | AddCameo／Produce 既有原生入口 |

原生 Building 枚举的外层工厂条件见 [既有侧栏研究](2026-10-23-fa2-drop-sidebar-137-pre10.md)。本轮没有完整执行未改写的该枚举来确认总统／校车的真实侧栏行为。

## 验证、排除项与未决问题

修前八项数据、源文件绑定及修后验证汇总保存在 [本轮证据](evidence/tech-enhancement-special-catalogue-20261107.json)。当前 `tech_enhancement_midgame.py` 执行 45 个场景／164 项检查，冻结 pre.13 的 legacy／exact × PRES／BUS 四条反例均命中预定的目录缺项消息；当前四条均通过。覆盖身份、所属／前置／工厂模型拒绝、原值保持、关闭、签名／Ares限制与生产交接。历史 pre.3 对象在此 checkout 缺失，因此明确跳过该旧刷新基线，`before_reproduced=null`，没有计作本轮通过。只把已执行 runner 的结果计为通过；Linux 源码抽取与合成内存检查不等于原生游戏、真实 Windows DR、图形侧栏、平台调用链或联机验证。

已排除“白名单遗漏 PRES／BUS”：二者原本就在名单中；公开候选中的七类也全为 TechLevel=-1，不能用它们默认等级不同解释分组。已确认需要改的是工具箱目录对本人当前加强目标的特殊标志处理，不需要全局清除游戏 Insignificant、放开所有民用对象、忽略 Owner／前置，或给 legacy 增加无条件图标。

未决项是用户实际规则字段、所选加强模式／全开组合、当前国家及工厂、Ares／Phobos／平台的确切构建，以及 legacy 原生侧栏在该现场具体在哪一层拒绝。发布说明只能承诺修复已复现的目录缺口，不能声称总统和校车在所有阵营或平台下已实测可造。
