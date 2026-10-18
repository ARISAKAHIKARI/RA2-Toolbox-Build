# 1.0.12：高级进驻的碉堡、维修厂和船厂分配

基于 1.0.11，文件版本 1.0.12.0，Assist 协议仍为 46。

## 问题与修复

高级进驻把坦克碉堡与 UnitRepair 设施作为容量为 1 的目标，但给车辆扣除 Size 运输体积。普通 Size=3 的车辆因此无法被分配。另一个独立问题是把 RadioLinks 的数组长度当作占用数；原版可为未占用建筑预先分配空联络槽，从而导致空建筑也被判满。

修复共用 `external_reader/source/game_view.h`：

- 按非空 RadioLinks 元素计占用，限制读取长度，异常数据不作为空位。
- 碉堡按一辆车计容量，并读取 Bunkerable 限制；UnitRepair 设施按 NumberOfDocks 的泊位数计容量。两者每辆车/船占一位。
- 普通运输载具继续按 Size 计乘员空间；可驻军建筑继续按人数计。
- 共用在途目标读取支持 Enter 与 Repair 任务，且排除已经上车或入堡的单位。
- 外部规划和 Runtime 最终校验都避免把同一个实际联络与在途预留重复计算。
- 完成回执识别坦克与碉堡的双向 BunkerLinkedItem，避免只等待运输载具的 Transporter 字段。

分配仍在宿主执行；Runtime 重新检查对象、代次、新鲜度、容量、名单和原生 MouseOverObject 返回动作，再通过统一 nativeMission 提交 Enter 或 Repair。没有新增 Hook、没有直接写任务或乘员、没有改原版的入堡或维修合法性。

高级进驻帮助文字已补上设施适用范围。分散范围仍是点击时屏幕内的同类目标，配置的快捷键和鼠标键不变。

## 字段核对

对照 YRpp 类属性与现有 CnCNet 参考 EXE：

| 字段/行为 | 参考 |
| --- | --- |
| RadioLinks 数据/长度 | 对象 +0xE4/+0xE8；0x65AE30 逐项检查非空联系 |
| BunkerLinkedItem | Techno +0x2E4；双向坦克/碉堡关联 |
| Bunkerable | TechnoType +0xD2E；INI 读取写入在 0x71501E |
| Bunker / UnitRepair | BuildingType +0x16AB / +0x16A9 |
| NumberOfDocks | BuildingType +0x1780；INI 读取写入在 0x464945 |

这是现有布局的修正，不是所有平台或扩展版本的通用证明。船厂适用的是具有 UnitRepair 的维修设施路径，最终是否允许维修仍取决于游戏原生查询。

## 验证

1. 旧版本复现：同样的两辆坦克请求在 1.0.11 DLL 中接受 0、拒绝 2；旧外部规划在“一个已分配空联络槽仍应为空位”的断言失败。
2. 外部规划 ASan/UBSan：Size=3、空/非空联络、每堡一车、多个维修泊位、已入堡、在途 Enter/Repair、敌方/屏外排除、联络与在途去重、Bunkerable 限制、运输容量与驻军人数回归。当前执行环境不支持 LeakSanitizer，已关闭泄漏检查；ASan/UBSan 保留。
3. 实际新 DLL 的 20 个 x86 Runtime 用例：单机与网络会话模式的输入采集、协同邮箱提交、原生 Enter/Repair 事件载荷；同目标争用、占位变化、原生拒绝、过滤撤销、双泊位、维修在途预留与去重、入堡回执。
4. 原有 cooperative 分配和 external_reader 回归通过。
5. Zig 0.13.0 构建、PE32 x86、1.0.12.0 资源、Common Controls v6、导出/协议/依赖及源代码架构边界核对通过。

Runtime 测试使用真实 DLL 和原生事件构造/入队机器码；Win32、世界、点击查询及原生合法性返回使用夹具。入堡关系由测试环境提供，不能据此声称已验证实际路径运动或维修效果。没有本轮真实 Windows 游戏、LBW 或双机联机验收。历史报告只代表其对应历史版本。

测试入口：`assist/tests/tank_bunker.cpp`、`assist/tests/tank_bunker_runtime.py`；结果：`verification/review-112/`。
