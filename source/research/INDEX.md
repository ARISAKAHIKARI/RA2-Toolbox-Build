# 逆向研究索引

从 2026-10-21 起记录新研究，不补录旧工作。

持久笔记：`RA2-Reverse-Engineering-Notes.sqlite`。表 `notes` 保存结论、依据和未决问题；`notes_fts` 可按英文关键词检索，中文可用 `LIKE`。后续更新同一文件，避免每版另建副本。

| 主题 | 新笔记 | 关键词 |
| --- | --- | --- |
| 己方磁电吸己方坦克的阵营过滤、实验性入口 | [pre.11 研究](2026-10-23-friendly-magnetron-drop-137-pre11.md) | AutoDrop nativeEnemy friendly allied neutral 2AC 2B0 Owner |
| FA2真实SHP解码、覆盖图色盘、朝向、碉堡自身预留、摔落租约、队形续走与侧栏 | [pre.10 研究](2026-10-23-fa2-drop-sidebar-137-pre10.md) | SHP Format2 RLE 4863D0 MagnetronDrop 2E4 TAB frame2 4456D0 RecheckTechTree |
| pre.8 快捷键全失效：Host/DLL 通信名称与队列误报 | [pre.9 研究](2026-10-24-hotkey-mapping-137-pre9.md) | v065 v066 MappingPrefix commandMatch Pending ACK |
| FA2 资源规则、共用队形渲染、显式 Sticky、全局水陆摔落 | [pre.8 研究](2026-10-24-fa2-sticky-drop-137-pre8.md) | ForceMission 5B2FD0 MovementZone 5B4 LandType 89EA40 Palette NewTheater |
| Sticky 任务锁死、ImageFile 偏移与地形解码诊断 | [pre.7 研究](2026-10-24-sticky-assets-137-pre7.md) | 744270 ReadyToNextMission Sticky Guard 1F8 ImageFile SNO |
| 队形右拖、增强对象攻击、SHP格式2、关闭功能后追击停滞诊断 | [pre.6 研究](2026-10-24-input-shp-137-pre6.md) | WM_RBUTTONDOWN ApproachTarget 4D5690 2AC SHP Format2 Ready magnetron-state |
| 普通左键接管、停止后重获目标、队形续走、碉堡到达、加密 MIX | [pre.5 研究](2026-10-24-input-mix-137-pre5.md) | FollowNative LeftClick 458E50 459301 RSA Blowfish LOCALMD |
| 50–150% 实验性视角缩放、原生分辨率重建与 CnC-DDraw | [Zoom 研究](2026-10-23-experimental-view-zoom.md) | 560BF0 DDEnableZoom DDGetProcAddress 8A00A4 8A00A8 ScreenWidth |
| 全体单位观察图像、独立朝向、气泡刷新、剧场MIX发现 | [pre.4渲染研究](2026-10-23-watch-assets-137-pre4.md) | 4C93D0 518D80 7EAEFC PrimaryFacing SecondaryFacing TMP ISOTEMMD |
| 攻击停滞追击、120%条件留枪、持续队形、大师科技与悬停进驻 | [1.0.37-pre.4 研究](2026-10-23-pursuit-137-pre4.md) | 4D74E0 4D769F ClickedMission 646E90 VoiceMove CoveragePercent FormationMemory MasterTech |
| 原版攻击走位、生产暂停、MIX 磁盘资源、小地图导航、科技采集 | [1.0.37-pre.3 研究](2026-10-23-input-assets-137-pre3.md) | NeedsReposition IsSuspended 4C9EE7 MIX ABEFD8 44574A 6AA788 6D6000 |
| 强制重绘、DDraw、真实 TMP/SHP 局部观察窗 | [1.0.37-pre.2 研究](2026-10-23-refresh-watch-137-pre2.md) | FlagRedraw 4F42F0 DDraw TMP 547020 11A 11B ABBED0 SHP Watch |
| 防空预留、地面障碍、外部 VXL/HVA 和队形 | [1.0.37-pre.1 研究](2026-10-23-fire-formation-137.md) | Sleep Guard IsInAir 7563A8 5BD730 ColorScheme VXL HVA Formation |
| 路径点坐标、坦克碉堡旧 Move 推断（pre.4修正）、磁电落地、载员 hover | [1.0.36 研究](2026-10-23-orders-display-136.md) | ProcessClickCoords PlanConnect TankBunker 74021E 70FEE0 54CA90 Idle passengers 431 |
| MRGA 信标替换入口导致全局启动拒绝，v1.0.35 修复 | [指纹、ABI 与验证](2026-10-22-platform5-135.md) | 431410 MRGA1 12FB60 startup signatures ClearBeacons SellAvailable |
| 平台实机 Hook 已回调但启动校验拒绝 | [实机日志与单次采集](2026-10-22-platform5-live-signatures.md) | status=-1 signatures self ReadProcessMemory RA2MasterProbe 0.2.1 |
| 新平台随机名称识别、只读成功但 Hook 失效 | [1.0.34 识别与诊断](2026-10-22-platform5-134.md) | random process name Silvana MRGA1 B73550 WH_GETMESSAGE callback |
| 1.0.33 共用控制实现、原属方查询与逐功能采集器 | [实现与验证](2026-10-22-control-133.md) | PlayerControl 50B6F0 6339BA 637E40 RA2MasterProbe 0.2.0 |
| 修改大师 2.80 有效控制差分、共用判定及疑似错位补丁 | [回游戏后采集](2026-10-22-ra2master-280-active-capture.md) | ControlledByPlayer 50B6F7 6339BA 6378E0 637E40 PID |
| 修改大师 2.80 实机映像、VMProtect 与 explorer PID 槽位疑点 | [2.80 采集分析](2026-10-22-ra2master-280-live.md) | ControlAll 631C84 explorer PID 577B3A heap VMProtect 2.80 |
| 修改大师模拟执行与 gamemd.exe 实机只读采集 | [采集研究](2026-10-22-ra2master-capture.md) | RA2MasterProbe VMProtect gamemd spawn CreateFileW reversible diff |
| 跨玩家手动事件恢复、切换禁用和修改大师加壳样本 | [1.0.32 研究](2026-10-22-player-132.md) | PlayerSelection native queue SwitchAvailable Ra2Master 2.90 2.11 2.10.3 |
| 直接发令、原版普通事件接收和出售所属方检查 | [执行层研究](2026-10-22-direct-orders.md) | OutList DoList MegaMission QueueMission SetTarget SetDestination Sell HouseIndex |
| 科技全开旧档案、侧栏生产事件与经典兼容候选 | [1.0.31 研究](2026-10-22-tech-131.md) | Tech CanBuild FindFactory AddCameo Abandon AbandonAll 0.3.2 |
| 原生快捷键与无可用单位时切页 | [pre.9 研究](2026-10-21-pre9.md) | CommandClass Hotkeys PlanningMode InfantryTab |
| 建筑就绪时预定 | [pre.9 研究](2026-10-21-pre9.md) | Factory progress ready reservation |
| IFV 射程及停步状态 | [pre.9 研究](2026-10-21-pre9.md) | IFV SelectWeapon IsCloseEnough pursuit hold |
| 步兵强制进入建筑格 | [pre.9 研究](2026-10-21-pre9.md) | C4 CanEnterCell Capture Sabotage CanC4 |
| 手动命令应答语音 | [pre.9 研究](2026-10-21-pre9.md) | VoiceMove VoiceAttack QueueVoice |
| 步兵 C4 移动能力、安放、计时与到期伤害 | [pre.12 研究](2026-10-22-pre12.md) | C4 CanEnterCell Sabotage C4Timer ChargeDamage |
| 原生路径点追加、提交与步兵进入 | [pre.10 研究](2026-10-22-pre10.md) | PlanningAdd PlanCommit PlanningToken Enter MegaMission |
| 弹体方向、反向两格与跨帧连发合并 | [pre.10 研究](2026-10-22-pre10.md) | Bullet Velocity CanEnterCell reverse dodge MergeFrames |

此前已有 Scatter 研究继续保留在 [SCATTER_RESEARCH.md](../SCATTER_RESEARCH.md)，不因建立本索引重做或重新宣称完成原版躲弹复现。

