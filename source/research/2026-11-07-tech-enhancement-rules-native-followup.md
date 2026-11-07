# 总统／校车：规则模式的原生路径复核

## 问题与结论

用户在 pre.14 目录修复说明之后反馈“规则模式好像也没有”。本轮没有取得用户这次运行的版本、配置、日志或实际 Type 数据，因此不能假定已经在 pre.14 上复现，也不能把规则模式的现场问题认定为已修复。

先查询 INDEX、[pre.14 目录修复](2026-11-07-tech-enhancement-special-catalogue.md)、[中途刷新](2026-11-07-tech-enhancement-midgame-refresh.md)、[原生侧栏](2026-10-22-tech-131.md)、[工厂枚举](2026-10-23-fa2-drop-sidebar-137-pre10.md)与同一持久 Notebook v155。冻结产品基线为 `572f46861c3c58a410e62ea8211862734ab5d277`。

本轮新证据排除了一个过宽的解释：**不能把规则模式的缺图直接归因于原生枚举再次排除 Insignificant。** 支持 PE 上，合成总统／校车在实际 TechLevel=1、相应条件满足时，即使保留 Insignificant=1，也能通过原生 CanBuild，并由原生 Building 和 House 重查路径到达 AddCameo 调用点。该结果不证明用户平台已可见或可购买。

pre.14 修掉的工具箱目录过滤缺陷仍成立。规则模式只请求原生重查而没有主动补图，是设计路径差异；在这次反证后，不能仅凭缺少主动补图函数把它称为产品缺陷。

## 支持映像与静态证据

重新取得并核对私人支持 PE，SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。没有打开用户当前游戏进程。公开的[静态证据](evidence/tech-enhancement-rules-native-static-20261107.json)仅保存地址、ABI、函数段 hash 与结论，不含原 EXE、指令字节或反汇编转储。

| 路径 | 已核对的行为 |
| --- | --- |
| Building `0x4456D0..0x44583F`，thiscall、无参数 | 先检查当前玩家所属、Limbo、`+0x41B`、`+0x660`，再按 BuildingType `+0xEB8` 选择工厂类别；四种类型枚举未见直接读取 Type `+0x232`。 |
| Infantry 枚举 `0x4457CA` | 在 `0x4457DE` 调 CanBuild；返回非零时于 `0x4457EF` 调 AddCameo(16,index)。 |
| Unit 枚举 `0x445789` | 在 `0x44579D` 调 CanBuild；返回非零时于 `0x4457AE` 调 AddCameo(40,index)。 |
| CanBuild `0x4F7870..0x4F8363` | thiscall(House,Type,buildLimitOnly,includeProduction)，ret 0x0C；保留等级、前置、国家与数量判断，未见直接读取 `+0x232`。 |
| FindFactory `0x5F7900..0x5F7A8B` | thiscall(Type,allowOccupied,requirePower,requireCanBuild,House)，ret 0x10；仍检查实际工厂、所属掩码、海陆及所选状态要求，未见直接读取 `+0x232`。 |
| RecheckCameos `0x6AA600..0x6AACAE` | FindFactory(true,false,false,CurrentPlayer) 成功后调用 CanBuild(factory.Owner,type,false,true)；返回非零便保留图标。 |
| House 重查块 `0x4F9265..0x4F9301` | 读并清除 House `+0x1FC`，按建筑存活、健康、使命和所属筛选，再调用虚表 `+0x4E0`。 |

InfantryType 虚表 `0x7EB610` 与 UnitType 虚表 `0x7F6218` 的 `+0x94` 均指向 `0x5F7900`。`Insignificant` 位于 Type `+0x232`，TechLevel 位于 `+0x634`，Owner mask 位于 `+0x6CC`。直接字段检索不能证明所有传递调用或任意 MOD 都不存在同类条件，结论限定于上述支持映像与实际执行的对照。

## 原生指令对照

使用 [audit_tech_enhancement_native.py](../tools/audit_tech_enhancement_native.py)，复用既有 `audit_tech_data_native.py` 的合成对象与映像装载。PRES/BUS × Insignificant 0/1 × TechLevel -1/1，共八组通过。[机器可读结果](evidence/tech-enhancement-rules-native-20261107.json)绑定原 PE、原夹具与本轮脚本 SHA-256。

| 类型 | Insignificant | TechLevel | 完整原生 CanBuild | Building／House 到达 AddCameo |
| --- | --- | --- | --- | --- |
| PRES 与 BUS | 0 或 1 | -1 | 0 | 否 |
| PRES 与 BUS | 0 或 1 | 1 | 1 | 是 |

实际执行 CanBuild、FindFactory、完整 Building::UpdateConstructionOptions 和 House 重查片段。两类型采用真实虚表；Owner 组、House、建筑与前置为合成数据，BUS 的合成前置索引 0 已满足。malloc/free、合成建筑 mission getter、AddCameo 终端以及 House 末尾三个 UI 更新调用为明确替身。**AddCameo 仅记录被调用，未执行真实插图；没有运行 Windows 程序、用户平台、实际画面或生产队列。** 所有对照均保持 Type 数据字节和原生代码 hash 不变。

## Ares 与诊断边界

固定 [Ares 4f1d929 的 Hooks.CameoList.cpp](https://github.com/Ares-Developers/Ares/blob/4f1d929920aca31924c6cd4d3dfa849daa65252a/src/Ext/Rules/Hooks.CameoList.cpp)在 `0x6A63B7`、`0x6A8710` 等位置使用 `RulesExt::TabCameos[4]` 动态数组并同步 CameoCount。因此当前工具箱 `techCameoInventory()` 的固定内联条目／75 项假设不能直接用于证明该 Ares 实现下“总统不存在”。该兼容性差异不是用户规则模式缺图的已证明根因。

既有 Runtime.log 有版本、连接与模式状态，但没有 PRES/BUS 的逐类型实际等级、写入所有权、原生 CanBuild／FindFactory 返回。UI 的“已启用 N 项”只表示规则修改状态，不等同这 N 项全部已出现在原生建造栏。

## 排除项与未决项

- 已排除在本支持映像上，仅因 Insignificant=1 就使规则模式原生枚举拒绝的假说。
- 未添加盲目补图、全局清除 Insignificant、跳过 CanBuild、修改前置或工厂等产品改动；本轮只有研究记录和可复现的只读原生夹具。
- 用户当前版本、加强的实现选项、是否与科技全开规则数据／参数模式协调、PRES 实际 TechLevel／身份／修改所有权、当前玩家及工厂、平台原生判断和图标链仍未知。
- 后续需先核对现场版本和状态，再取得实际字段／原生判断证据，将“等级未生效”“规则拒绝”“工厂或刷新未到”“图标链问题”分开。不能把受控指令通过写成现场问题已修复。
