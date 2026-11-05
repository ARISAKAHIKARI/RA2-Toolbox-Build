# 副相机：基地车身份转换、完整容器链与实时固定点

基线为 `v1.0.41`，提交 `513aeb4cf03238cce956bd35e0c9381c2a7cc484`。本轮只修改外部只读 watch 跟踪／呈现代码；不新增游戏写入、Hook、命令或 wire protocol。

调查前已检索 `INDEX.md`、`2026-10-25-watch-passenger-spatial-host.md`、`2026-11-01-watch-lifetime-reader-performance.md`、`2026-11-02-render-ui-followup.md`、两份 2026-10-29 建造过渡笔记及 `2026-10-23-orders-display-136.md`。本机同名持久笔记 `E:/Download/RA2-Reverse-Engineering-Notes.sqlite` 只读检索了 13 条记录；Library 服务当前不可用，本轮条目交由发布协调者写回同一文件。

## 原生证据与根因

本轮私人静态 PE 样本 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。仅做静态字段／指令核对，没有启动、注入或执行该游戏。固定一手结构参考为 YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 `UnitClass.h`、`TechnoTypeClass.h`、`MissionClass.h`、`GeneralDefinitions.h` 和 `BuildingTypeClass.h`。

| 原始 VA / 字段 | 本轮核对结论 |
| --- | --- |
| `UnitClass::TryToDeploy 0x7393C0`，`0x739698..0x7396C8` | 分配新 Building，以 Unit 的 Owner 和 Type.DeploysInto 创建，不沿用原 Unit UID。函数为 thiscall，无显式参数，正常末端 `ret`。 |
| `0x7399DA..0x739A64` | 结束旧 Unit 并移除对象；没有向新建筑复制 Abstract UID `+0x10` 的指令。 |
| Unit Type `+0x404`，Building Type `+0x408` | 分别为 DeploysInto / UndeploysInto。`0x449E2E..0x449E49` 的反向路径分配新 Unit，仍不转 UID。 |
| `0x45EC90 / 0x45ECA0` | Foundation 枚举 `Type+0xEF0`，宽／高查表 `0x8192B8 / 0x819310`。 |
| `0x739460..0x7394BE`，`0x7396DF..0x739711` | 超过 2×2 的展开地基先加 `0x89F6A4` 两个 int16 原生格偏移，落点为该格精确中心。 |
| `0x449E84..0x449F0E` | 反向超过 2×2 时加 `0x89F6F0 / 0x89F6F4` 世界坐标偏移，再对齐到格中心。 |
| Unit `+0x68C` | TryToDeploy 旋转等待置 IsDeploying=1；失败／结束路径清零。 |
| MissionName 表 `0x816CAC` | 直接读取 PE 原生字符串指针：11=`Area Guard`、15=`Hunt`、16=`Unload`、18=`Construction`、19=`Selling`。生产判断使用十进制 16/19，夹具另有 15/11 不得授权转换的负例。 |
| Building `+0x218`、Type `+0x16B9` | ConstructionYard 的反向分支还要求 ArchiveTarget、非 campaign session `0xA8B238` 与原生 redeploy 开关 `0xA8B320`；普通 Sell 不能充当 yard undeploy 证据。 |

原 watch 永久绑定 pointer+UID，且只有最初选中组可以接替死亡焦点。MCV 展开会生成另一身份，旧目标两次终止样本一致后，原实现清空槽位。Transporter 既有路径只接受可直接绘制的单层宿主，也未处理坦克碉堡双向 `BunkerLinkedItem+0x2E4`，所以多层入舱／碉堡缺少有效相机锚点。

## 转换策略与保守边界

只有己方对象、明确配置的互转类型、实际观察到 Unload(16)/IsDeploying 或合格 Selling(19) 状态、原生地基预计 XYZ、完整转换前库存、两帧内唯一新对象、两次独立 fresh 候选核对，以及最终仍一致的原目标终止状态，才推断并接续新 UID。库存记录整个对象数组的 pointer+UID，不按目标格过滤；已存在的任何地址都排除，邻格旧 MCV 移入目标格、旧地址换 UID、近邻同类或多个候选不能接替。

这是受限的出生差分推断，**不是原生持久 successor backlink 或唯一 provenance 证明**。静态核对未找到这样的持久双向联系；本轮不为此新增 Hook。完整库存上限 8192 对象，只有明确观察到转换时才采集；相同逻辑帧、类型／所属方／预计落点不变时重用已验证库存。数组超限／失读只失去新身份授权，不把空库存当作“原先不存在”。

对已确认 `DeploysInto` 指向 ConstructionYard 的己方 Unit，平时仅保存轻量固定点资格，不扫描全对象数组。若对齐的 MCV 在两次轮询之间瞬间完成转换，没看见 Unload 状态，仍保留最后核对地点的**实时世界画面**，不猜绑定建筑。这个窄语义变化也使该类 MCV 真正死亡后可能保留原地点；用户可 Shift＋右击关闭。普通其他单位的死亡、原组选取的五格最近接替及手动关闭保持原语义。

Unknown 不是 Dead：失读／未知 vtable 时保留原 pointer+UID 等待恢复，不调用 successor 推断；有转换／MCV资格与最后可靠坐标时，仅将本轮空间锚点的 Target 清空，用当前共同世界继续绘制该地点。不会重复旧 bitmap，也不会把失读解释成一次转换成功。

## 容器、迷雾与生命周期

最多解析八层身份不同的宿主。每一跳重新检查 Transporter 与完整乘员链，或 Infantry 与稳定 Occupants vector，或车辆与带原生 Bunker 标记建筑之间双向 `+0x2E4`。定位最后宿主后再次核对每一跳及原始焦点生命样本；循环、深度超限、损坏列表、UID 改变与失读只保留最后可靠地点。原始被跟踪对象 UID 不被载具／驻军宿主替换，离开容器后自动恢复原对象坐标。保存并每次重验中间驻军建筑，避免第 65 棟之后的宿主每次重新有界发现。

容器终点仅在己方宿主，或原生 Cell 当前无黑幕／迷雾遮挡时取得新坐标；使用既有 shroud selector 的 `Cell+0x12C` / `+0x140` 与相同 nativeFog 开关，不引入新的可见性 ABI。未知或边缘遮挡保持旧地点，点击导航不能借容器解析取得隐藏敌方新坐标。共同 SceneBank 的进程／世界／epoch、地图 bounds、当前 Marker 核对和最终实时 shroud 合成继续生效；固定点也执行世界边界发布检查。

## 回归入口与未决

`tools/test_watch_tracking.py --zig <zig.exe> --baseline-source <frozen-v1.0.41-source> --out <directory>` 编译真实 production watch headers、锁定 SHA 的旧版函数负控，以及真实 watch Renderer。`watch_tracking.cpp` 覆盖双向身份转换、瞬时未观察转换的固定点、不完整库存、同帧不重复扫描、16 类拒绝出生、原组选取接替、运输／驻军／碉堡完整链、可见性、循环与读失败。`watch_tracking_renderer.cpp` 用受控素材边界检查多个新世界发布在固定点继续改变像素、单目标 Unknown 不冻结世界、读恢复、关闭的零工作路径及 reveal policy 隔离。最终通过状态和来源 SHA 以[本轮发布回归摘要](evidence/watch-tracking-20261105.json)及随发布保存的 `watch-tracking/result.json` 为准。

Source.zip 已附 `assist/tests/fixtures/watch_tracking_baseline/` 的五个原始头文件及 `manifest.json`。从解压后的 `source` 目录运行时，使用 `--baseline-source assist/tests/fixtures/watch_tracking_baseline`；manifest 逐文件列出 513aeb4 来源路径、Git blob 与 SHA-256，负控字节保持不变，不需要另行下载旧源码。

受控内存和 Win32 fixture 不等于真实游戏、桌面视觉、MOD 所有进出舱规则或多人同步验收。本轮没有执行游戏现场的 MCV 展开／收起或长时间跟踪。私有 EXE、完整反汇编及原始内存不随源码或发布包分发。
