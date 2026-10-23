# v1.0.37-pre.3：攻击走位、生产状态、磁盘资源和科技采集

调查前已检索 INDEX、pre.9 的 IFV 接近、原版事件执行、pre.2 的 TMP/SHP 和持久 SQLite。私有 EXE、完整映像和原始采集包不进入仓库。

## 样本与证据

- LBW：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 科技采集另为 `gamemd.exe`，SHA-256 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；不能把此包的 VA/字节不经校验移植到其他版本。
- 本轮新增地址核对、格式分析和合成世界中的原生 x86 执行。只有“科技开关在采集时有效”和“磁电射程外不走”来自用户实机描述；没有本轮 Windows 图形、完整寻路、武器效果或双机联机结果。

## 攻击到底是否只写了目标

问题：磁电和其他单位超出最大或小于最小射程后，为什么只转炮塔？

共用入口 `nativeMission` 构造原版 MegaMission，接收路径已由 `2026-10-22-direct-orders.md` 定位：`4C6CB0` thiscall(Event*) 分派，`4C73B9` QueueMission、`4C7467` SetTarget、`4C747C` SetDestination。并非工具直接写 `+2B4` 就算攻击。新核对 `6FFBE0` ClickedMission 的普通分支：`6FFDA5` 起转换目的地/目标，`6FFDE6` 调 `646E90` QueueMegaMission；声效分支不构成另一种寻路接口。战网样本的 `6FFC6C`、`6FFDCE` 已带平台跳转，不把这些位置当作跨平台可任意调用的入口。

发现并修复的工具限制：

1. 磁电选敌和最终准入都要求当前可开火距离；现在只要武器兼容且在最大射程加“额外索敌格数”内，就能发原版 Attack，最小射程不再阻止下令。
2. 等待磁电开火的固定截止会打断接近；现在观察自身位移，每移动至少 16 leptons 延长进度期限。仍只用真实 ROF/抬升观察认定开火，走动不是开火。
3. 全局磁电把任何 `Foot+5A4` 目的地当成手动命令并删除任务；原版 Attack 自身也会设置目的地。已登记任务改由手动命令 token、开关、对象身份/类型等取消；新登记仍只接纳空闲、无目的地的磁电。
4. 普通增强攻击的原版 SelectWeapon / IsCloseEnough 查询包含最小射程，但之前该目标会同时被排出索敌范围。新增 `NeedsReposition` 事实，让兼容且过近的目标走原版 Attack，并保留其走位，不按每次采样反复重发。
5. 普通射程外接近保留 pre.9 已验证的 Move -> 射程内 Attack 逻辑，避免 IFV/AA 的特殊射程退化。移动目标的追击重定向至少间隔 12 帧，进入有效射程仍立即切换。

其他攻击路径审查：飞机分配、智能卸兵攻击、飞碟强制攻击和刷弹恢复目标均经同一原版任务入口；飞机/卸兵攻击没有要求目标先在开火距离。刷弹只在已开火且射程内时启动，离开射程后恢复捕获目标并结束刷新。防空/防偷建筑是定点自动防御，其射程门限保留；不能把全部自动防御变成追击命令。普通手动右键仍保留原版行为。

证据：`assist/tests/magnetron.cpp`、`advanced_range.cpp`、`advanced_range_runtime.py`、`magnetron137_runtime.py`，输出在 `verification/review-137-pre3/`。测试证明事件参数、准入和调度，未证明原版对每一种单位都必然会退出最小射程；这一点需要实机复测。

## 正常生产为何误闪

`FactoryClass` 的 `OnHold +5C`、`IsSuspended +70` 和 `IsManual +71` 含义不同。新反汇编：`4C9E60` Suspend 写 `+70=1` 和手动标志；`4C9EA0` Unsuspend 在 `4C9EE7` 清 `+70`，并不同时清 `+71`。因此 `+71 != 0` 不能作为“现在暂停”。与 [YRpp FactoryClass.h](https://github.com/Phobos-developers/YRpp/blob/master/FactoryClass.h) 对照。

读取当前暂停字段，结合 `progress +24`、`balance +60` 的变化抑制正在生产的提示；完成待放置也不作为闲置。六条生产线独立开关；载具、舰船和飞机共用原版车辆页签，因此该页签有正常生产时压住同页其他闲置提示，避免一个正在花钱的页签仍闪。验证：`review136.cpp` 的暂停、手动标志残留、花钱/进度、完成和共享页签组合。

## 从游戏目录读取 MIX/TMP/SHP/VXL/HVA

原理：只读实际游戏进程 EXE 所在目录。散文件优先；`expandmdNN` 大编号优先，其后 `ecachemdNN`、`ra2md.mix` 和其他基础包。嵌套包继承父包优先级。未加密目录由磁盘解析；加密 MIX 使用游戏已经解密的目录索引，资源正文仍从磁盘读取。未实现离线 Blowfish 解密；若游戏尚未建立该索引，则该包暂不可读并尝试现有内存渲染。

新增字段证据：

- MIX 列表 `ABEFD8`，头结点 next `+8`；普通结点 next `+4`，文件名 `+0C`，项数 `+14`，索引指针 `+20`。`5B3C20` 构造/读取代码在 `5B3C37`、`5B3D7E`、`5B3D95` 等处交叉核对。
- TMP 类型名 `type+2F5` 最多 14 字节，由 `54702C` 使用；图像 `+A4`，`5470A4..5470B6` 在引擎内重定位偏移。磁盘 TMP 必须自行按相对偏移解码，不能把磁盘数据当已重定位内存。
- TheaterArray `7E1B78`，步长 112；当前剧场 `822CF8`；ISO 调色板名 `+68`（十进制），扩展名 `+78`（十进制）。类型 Image 名 `+1FC`；SHPReference `FFFF` 标签后文件名指针 `+8`，数据 `+12`。
- VXL 磁盘头 802 字节、limb 头 28 字节、tailer 92 字节；HVA 头 24 字节、名称 16 字节、每矩阵 48 字节。适配到现有体素投影，不调用游戏绘制器。

格式交叉参考：[OpenRA MIX](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/FileSystem/MixFile.cs)、[名称哈希](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/FileSystem/PackageEntry.cs)、[VXL](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/FileFormats/VxlReader.cs)、[YRpp MIX](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/MixFileClass.h)、[TMP 类型](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/IsometricTileTypeClass.h)、[Theater](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/Theater.h)。实现为独立格式适配器，测试使用人工生成文件；没有复制或发布游戏美术资源。

边界：每次读取最多 8 MiB，MIX 最多 512、嵌套最多 4 层、索引最多 65535 项，验证范围并拒绝目录穿越。VXL/图像缓存各有限额。局部观察窗扩大采样并裁切成 320×256 的纯矩形，无标题/脚注；仍为简化光照、固定单位朝向，动画、桥层和全部 MOD 美术兼容性未实机验证。`game_assets.cpp` 验证覆盖顺序、嵌套继承、加密索引缺失/越界及 TMP/PAL/VXL/HVA 解码。

## 小地图点击与空闲飞机

点击坐标反解与当前小地图绘制使用同一比例/偏移。UI 发送只保留最新位置的 seqlock 邮箱；Runtime 在游戏线程核对局、前台、聊天、200 ms 时效和 `6D6000` 前 9 字节后，调用 thiscall SetTacticalPosition(Point*)。相机调用不进入单位命令队列。`navigation137_runtime.py` 执行该原生函数，检查重复、撕裂、过期、前台/对局变化和签名不符拒绝。

空闲飞机只选择，不再调用 `6D2420` 居中视野。上述脚本还检查选择 ACK、数量、相机位置不变和无居中调用；Win32 鼠标捕获/叠加层命中测试属于尚待实机确认的部分。

## 科技全开 probe 包结论

输入包 `RA2Master-Capture-20261022-225616-12556.zip` SHA-256：`69403b9b81fca271da7c16f11fe334b391283fa43c0829107f16e656ee26f5d5`。四阶段 clean/off/on/off，逐字节重新核验 `.mem`、`.valid`、`.stable`。

315 字节符合启用后改变且关闭后恢复：83 代码字节、189 侧栏/显示区域字节、43 其他静态数据字节。不能据此把全部 315 字节归因于修改器的写操作；正常游戏状态可能相关变化。

83 代码字节对应五处逻辑补丁：

| VA | 长度 | 开启行为 |
| --- | --- | --- |
| 44574A | 21 | 建筑枚举：CanBuild 调用改为 TechLevel != -1 |
| 445795 | 15 | 载具枚举：同上 |
| 4457D0 | 21 | 步兵枚举：同上 |
| 445810 | 21 | 飞机枚举：同上 |
| 6AA788 | 6 | 侧栏移除分支的 JNE 改成无条件 JMP 6AAA68 |

TechLevel 位于类型 `+634`。这直接解释“所有图标出现、不会因常规条件被删”的实现；不等于已验证真实生产会绕过所有工厂/前置条件。四阶段 private regions 均为空，因此无法从该包断言动态堆上的类型规则有没有另被修改。

`577B3A` 的一字节及 `6378E0` 的五字节属于启动 clean -> off 变化，不属于这次科技开关的可逆补丁。后者的错位疑点见此前修改大师控制研究，不能混入本结论。

本工具当前在 draw/prune 的 CanBuild 调用点过滤允许类型，并非无条件跳过全部 prune。本轮仅分析采集，没有照搬修改器补丁。可复现脚本 `tools/analyze_tech_probe.py`，摘要 `verification/review-137-pre3/tech-probe-summary.json`。仍待验证：实际生产事件、堆字段、关闭后已入队对象，以及联机一致性。
