# Hotfix25：CnCNet/YRpp/Vera 渲染补充与 Phobos 指示器

## 范围与依据

问题：继续核查 CnCNet/YRpp 与 Vera20K 的渲染逻辑，补充现有重绘，并增加 Phobos 矿车/电量指示器。调查前已检索研究索引、Hotfix15/21/23/24、两轮渲染缺口及同一持久 SQLite v100，未把新发现回填到早期版本。

固定上游：`CnCNet/yr-patches@0318b1f1bddcc9c664c50ebee820cde74a662f47`、`CnCNet/YRpp@d9795546950fcc75a94325ffc83b44d16ec34ce2`、`ARISAKAHIKARI/vera20k@3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`、`Phobos-developers/Phobos@f06797ec39f2b60b9fa5b6da69288c36d189b712`。读取文件与 blob 见 [上游证据](evidence/render-hotfix25-upstream.json)。Phobos/CnCNet 为 GPL-3.0，保留本仓库 GPL 全文及第三方署名，不捆绑上游游戏素材。

此次静态反汇编的私有样本 SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。13个地址范围及本地反汇编内容摘要见 [原生证据](evidence/render-hotfix25-native.json)。私有 EXE 与原始反汇编不入库、不交付。未执行任何原生指令。

## 已接入生产路径

| 领域 | 原生/上游依据 | 实现与边界 |
| --- | --- | --- |
| 卸载关联重绘 | `Building43DA80` 在本体前重绘 radio 联系人；`Unit73B0B0` 根据当前/排队 Unload 隐藏普通本体。tether `+418`，radio `+E4/E8`，任务 `+AC/+B4`，Unload=16 | `tether_draw_order.h` 采集关联，按建筑位置先联系人的整组本体，再建筑。Unit 普通本体隐藏，Infantry/Aircraft 保留原槽及重绘。血条、状态格不随本体转移。主视图保留全部原始本体组，最新采样重新编排；副相机/实景小地图用相同规则 |
| 特殊状态格 | `709A90`、`716290`、`6B7D50`、`4726C0` | 生成物 Type `+D5C`、SpawnManager `+2D0`、节点 `+38`，状态0/6算停靠；驻军 Type `+1580/+1584`、Occupants `+684`、InfantryType.OccupyPip `+E00`；吸收 Type `+16AE/+16AF` 按链表正向一人一格、Unit帧4；心控 Type主武器Damage、CaptureManager `+2BC`、节点 `+24`、Max `+3C`、Infinite `+40`、OverloadPip `+44`。增加悬停、盟友及建筑公开状态格准入，保留资源/普通载员/弹药分支；修正步兵+11X与建筑(4,2)斜向排列 |
| 范围提示 | `4566B0`、`456750`、`456980`、`6DBE20` | 读取已选建筑的 HasRadial `Type+238`、在线 `Building+660`、本机可控阵营、真实心灵/裂缝/隐形/当前武器范围；普通双椭圆和旋转四射线、同心扩圈；武器选择沿 Type+808、当前+138、精英武器回退。超武 Type.Array `A8E330`、选中 `8809A0`、Range/LineMultiplier `+F8/+FC`、原生脉冲 `B0E64C/842954`、当前格 `88095C`。范围半径按(r+0.5)/sqrtf(2)*60。读取时序列/帧变化丢弃该生产者，不修改脉冲计时 |
| 建筑隐形与 CnCNet 准入 | `Building4544A0` 使用有符号 `+6ED`，零时回退 `Techno703860`；CnCNet `cloaked_patches.asm` | 补建筑的渐隐/全隐形显示角色。只有实际 `703A09/45455B` 分支跳到文档中的 CurrentPlayer/Observer 比较指令时，才采用 observer 和单向盟约规则。样本这两处仍是原版指令；不能因为文件名/会话类型就假定补丁已安装 |
| 矿车/电量指示器 | Phobos `Hooks.UI.cpp`、House/Techno扩展和 Side 默认值 | `sidebar_indicators.h` 外部只读采样与字形/图标生产，`sidebar_indicator_host.h` 独立 Latest 工作线程；原生侧栏空隙内合成，关闭渲染升级仍可显示。按游戏帧取样，暂停时100ms重新核对，结果过期隐藏；客户区/原生分辨率/侧栏变化使旧图失效。左右侧栏均按实际战术区空隙定位 |

## 指示器口径

矿车显示**工作中/总数**。总数为己方已放到地图或正在运输的普通矿车/奴隶矿车；生产占位、死亡对象不计。活动判断保留 EMP、时空、禁用、奴隶管理器状态与可用矿厂筛选，区分采矿、卸载、入厂、工厂 radio 联系和无用途矿车；Guard 带矿返厂的移动边界仅适配已核对的 Drive locomotor `4AFB80`。

Phobos 默认阈值是活动比例 >99% 正常、>50% 黄色、其余红色。电量显示 `PowerOutput - PowerDrain`，House 字段 `+53A4/+53A8`；耗电/发电 <75% 绿色、<=100% 黄色、其余红色。停电计时 `House+2A4` 活跃时灰色 OFF，无己方建筑不显示电量。采样前后核对当前房屋、帧号和 Techno 数组，不把读取失败显示成0。

设置为 `Display.HarvesterCounter`、`Display.PowerDelta`，默认关闭，支持自动保存。工具箱视频/监控页与既有原生设置入口均可修改。另把既有 `RenderHighRefresh` 补进统一视频设置页，避免只有隐藏的原生控制项。

这不是 Phobos 扩展区内存的二进制适配：`Harvester.Counted` 覆盖、任意自定义现金建筑、Side偏移/自定义标签/颜色与未知 locomotor 不假定可用。没有安装 Phobos 也能显示普通矿车/奴隶矿车与电量；不能据此宣称支持全部 Phobos 模组。旧版本每次启动时已经存在、又完全藏入自定义 limbo 状态的矿车也可能无法恢复其“曾放到地图”扩展位。

## 保留差异与未决项

范围提示已形成独立值到像素的生产入口，但本轮采用宿主中点椭圆、数学 tan 与预乘覆盖率射线，经现有 UI 合成；尚未逐条复刻 x87 chop/tan表、原生 AA 的16位目标混合和 -500 Z 读写。当前建筑放置时的独立半径预览仍沿用原生放置路径；这里新增的是已选建筑和已选超武范围，不宣称所有放置预览都已重写。

特殊状态格的主分支已接通，扩展护盾、所有自愈/炸弹标记和全部原生 Extras 前后重绘时机仍非完整等价。完整 unsigned16 Z/A、cloak word-offset位移/坡面Z、全波形/辐射光束2/战斗灯光、混合SHP/VXL特殊件与RGB555整链路仍保留首轮路线中的缺口。拒绝用普通 alpha、单一深度或大块原生透视孔把这些差异标成完成。

本轮原生地址仅用于外部读取和实际补丁识别，没有增加游戏代码写入、引擎渲染调用、计时器或 RNG 消费。辅助计算的统一帧采样另见 [采样说明](2026-10-31-coherent-compute-frame-hotfix25.md)。

## 验证

按用户要求只进行 Zig0.13生产交叉编译、实际Windows声明的x86/x64 ABI比对、PE架构/版本/导入/导出/manifest静态检查和源文件一致性核对。本轮无着色器修改，沿用既有生成产物。最终哈希见本轮最终证据。

未运行 CPU 测试套件、Vulkan/D3D、Windows loader、原生代码、游戏、联网或帧率测试。编译与静态证据不能证明画面像素等价、运行时稳定性或 FPS 收益。
