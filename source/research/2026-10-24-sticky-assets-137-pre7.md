# v1.0.37-pre.7：实机日志定位 Sticky 任务锁死与 ImageFile 偏移

调查前检索 INDEX、pre.5/pre.6 笔记和持久 SQLite v24。基准为 GitHub `0b53b0ff654da2e0c35d19a300bdd30b2528af01`。原始日志及私有 EXE 不提交。

## 证据范围

- 用户 `Runtime(4).log` SHA-256：`a7ff3eb977369612be5cca43ab2a54fe3936cd2f12bfc47d66f85829544a9fc4`。其中 pre.6 有371行、66条磁电状态；下列时间是日志原值。
- LBW EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网 EXE SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 实机证据来自用户的旧版日志。新修复的验证分为原版机器码受控执行、实际 DLL 发令与纯 C++ 解析/规划测试；均不冒充完整实机寻路、渲染或联机验证。

## 为什么射程内能打，射程外不走，关闭功能仍不恢复

问题：后续攻击是否卡在武器选择、残留吸取关系，还是任务切换？

| 日志阶段 | 帧 | 观察 |
| --- | --- | --- |
| 02:27:40–42 | 5168–5293 | 当前任务 Attack=1，待处理=-1，存在移动目的地；距离逐渐缩短并再次开火 |
| 02:27:46–02:28:06 | 5463–6739 | 当前任务 Guard=5，空闲；全局磁电=0 |
| 02:28:08 | 6886 | 全局磁电变1，发令计数6→7，当前任务变 Sticky=6 |
| 02:28:51以后 | 9413–10658 | 当前始终为6；普通 Move=2、Attack=1反复进入 +B4，未取代当前任务 |
| 最后静止段 | 10397–10658 | +AC=6，+B4=1，目标非空、目的地为空、位置不变 |

采样开始时已经有早期射击记录，不能说此日志完整捕获了“出场第一次攻击”。所有磁电状态行的 toolEnabled 都是1，也未捕获关闭全部功能后的完整选中采样；关闭不恢复的结论由用户复现与以下原版分支共同支持，不伪称该段日志存在。

两份 EXE 的 Unit 虚表 `0x7F5C70+0x200` 指向 `Unit::ReadyToNextMission 0x744270`，thiscall、无栈参数、布尔返回 AL。`0x744277..0x744280` 读取 this+AC，等于 **6 时直接跳到 `0x744383` 返回 false**，尚未查询移动器。不是普通 Guard，也不是一种可随时撤销的暂停。

`QueueMission 0x5B35E0(mission,start)`（thiscall，ret8）通常写 +B4；即使 start=true，也必须先经过该 readiness。`NextMission 0x5B3570()` 才将 +B4 提升为 +AC 并清空队列。因此日志中目标和目的地可以变化、直接移动器可以走动，但当前任务仍永久停在6；不会正常进入 Attack 的追击阶段。工具取消自动任务记录、关闭开关并不改写这个引擎状态。

生产代码确有两处来源：磁电规划器结束/原地保持，以及普通高级移动到点/超时，都曾发送 MegaMission Sticky=6。改为 **Guard=5**；更新内部停驻记录及公共准入，拒绝旧 Sticky 提案。两种计算位置共用同一规划器，不另设私有执行路径。没有修改单位任务字段、武器、移动器、原版 readiness 或客户端 RNG。

排除：66条采样中 source+2AC/+2B0 均为空，不支持“旧吸取关系没释放”的解释；+6B6 在正常移动和停步时也变化，不能据此当成损坏标志强清。两把武器仍由原生选择，未用主副武器最大射程混合替代。pre.6 的对象接受0个是另一处已修输入缺口，不能替代此次根因。

已卡住的旧对局/存档中 Sticky 状态不会因换 EXE/DLL 自动消失。本版不对来历不明的 Sticky 单位强制本地 NextMission；应退出游戏、更新两文件，**开新对局**验证。旧档无损恢复仍未实现。

`sticky_mission_native.py` 在两份 EXE 各执行8案：真实 QueueMission、ReadyToNextMission、NextMission；对 Guard/Sticky 分别排入 Attack/Move/Guard/Stop，验证 Sticky 的负例及时间流逝、start=true 仍不解除。空闲移动器/radio/map 查询为明确替身，不包含完整游戏AI。

`magnetron_guard_runtime.py` 在实际 DLL 验证内/外计算、单机/联机事件队列、Guard 负载、旧 Sticky 拒绝、关闭后停止续发。相同新测试在 pre.6 DLL 首个 Guard 准入即失败，记录于 `guard-before.log`。事件入队的“联机队列”测试不是双机同步验证。

## 资源名少了前四字节

实机日志反复出现 actor=TREE25，而资源错误为 `sprite-load/decode:25`，TREE21→21 等同类情况共63行。`ObjectTypeClass` 原构造器 `0x5F7128` 使用 `this+0x1F8`，`0x5F7146` 初始化字符串首字节；后面 AlternateArcticArt/ArcticArtInUse 位于 +211/+212。[YRpp ObjectTypeClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ObjectTypeClass.h) 的结构也吻合。

生产 `game_assets_host.h` 三条 SHP/VXL 磁盘回退路径误读 **+1FC**，跳过 ImageFile 的前4字符，现统一改为 **+1F8，长度25**。SHPReference 中存在的真实文件名仍优先。`game_assets_host_test.py` 使用原生偏移存放与类型ID不同的长图像名、仅在嵌套MIX提供该资源，验证未选中未加载步兵的八朝向；不再只用 ImageFile 为空的 E1 夹具掩盖偏移错误。

这能解释日志中的树名截断，并修复使用该回退的步兵/载具资源查找；日志未提供具体失败步兵条目，不能声称所有MOD步兵问题均已实机消除。

## 地形：确认解码阶段失败，尚不能宣称实机全修

观察窗找到56个档案，调色板可用，但 terrain=0/1681；`CLEAR01.SNO`、`SHORE03.SNO` 各子块反复报告 tile-format，说明文件查找已成功。旧日志未记录失败的头字段，无法单凭名称判断究竟哪个检查触发。

对照 [OpenRA TmpTSLoader](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs) 及本地原版 `0x547187`：附加图只由 flags bit0 决定。修正不应有的“未知 flag 位直接拒绝”，并只在存在附加图时验证其坐标和尺寸。继续保留目录、尺寸、Z偏移、跨度和并发读取边界。补充没有附加图却含未初始化附加字段的夹具与具体诊断（头字段、Z偏移、附加范围、像素读取、内存回退）。

**尚无用户当前 SNO 文件来证明这两处检查就是全部地形缺失的唯一原因。** 新日志将指出准确失败位置；不将人工 TMP 像素测试称为实际地图已恢复。

验证文件位于 `verification/review-137-pre7/`。历史 `advanced_regression.cpp` 在初始目标断言已失败，缺少后来引入的 CombatProbe 夹具，与本次停驻分支无关；保留单独失败记录，不把它计入通过项。当前 `external_planner.cpp`、`advanced_range.cpp` 和编译 DLL 范围/攻击测试覆盖本次修改。
