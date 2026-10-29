# 逆向研究索引

从 2026-10-21 起记录新研究，不补录旧工作。

持久笔记：`RA2-Reverse-Engineering-Notes.sqlite`。表 `notes` 保存结论、依据和未决问题；`notes_fts` 可按英文关键词检索，中文可用 `LIKE`。后续更新同一文件，避免每版另建副本。

| 主题 | 新笔记 | 关键词 |
| --- | --- | --- |
| 独立读取与绘制、相同覆盖层去重、点击分区强制恢复 | [读取与呈现提速 Hotfix6](2026-10-29-independent-scene-reading.md) | Latest two observations separate ReadSession epoch cutouts equal no dirty upload WM_SIZE navigation |
| 最小化恢复空白与基地车展开／收起消失 | [恢复与过渡帧补修](2026-10-29-restore-and-buildup.md) | Z order IsIconic same layout make priority retry sampled Pose |
| 切出游戏后按钮与滚动条失效，真实控件恢复 | [控制台原生输入](2026-10-29-console-native-input.md) | AltTab real HWND no cloak no WM_PRINT cancelled move SendInput hit test |
| 全部分区滚动后控件失效、帧率分项与基地车展开边界 | [滚动输入补查](2026-10-29-scroll-input-followup.md) | WM_PRINT idle timer capture sampling fence readback MCV pending |
| 坡面阴影、光束／采矿与单位 UI、SHP 炮塔、素材动作、拖屏和退出响应 | [渲染与退出补查 hotfix2](2026-10-29-render-hotfix2.md) | Shadow_Matrix Laser Gather Ammo IFV SHP turret sequence partial visible Cadence ASYNCWINDOWPOS modal |
| pre.7 类型清单依赖、队形双击上下文、窗口退役与 CI 启动前失败复核 | [前端补查 hotfix1](2026-10-29-frontend-hotfix1.md) | dependency19 type-list Tap context stale connection HWND retirement CI runner0 no-steps |
| 工具箱独立设置窗口、依赖约束、明确快捷键手势与页面滚轮 | [前端输入与响应](2026-10-29-frontend-input-responsiveness.md) | popup DWM cloak latest processing dependency60 Press Release Hold Double keyboardOnly wheel |
| 64 位工具箱、32 位 Runtime／Hook 辅助程序与真实共享 ABI | [跨位数宿主](2026-10-29-x64-host-runtime-broker.md) | x64 x86 broker148 Shared4169840 ABI632 owner PID Hook cleanup |
| DLSS 5 官方 NGX 接口、独立进程、逐像素引导与历史消费 | [神经渲染接入](2026-10-29-dlss5-neural-integration.md) | DLSSNR NGX RGBA16F motion depth confidence cancellation sequence Reset |
| GPU 原生场景的运动、反向正交深度及保护信息 | [神经渲染引导图](2026-10-29-neural-motion-depth-guides.md) | MotionIdentity camera source lifetime per-pixel guides sequence OOM fallback |
| 覆盖层临时读失败、预定面板完整帧、小地图缓存与拖屏预取 | [宿主闪烁与性能优化](2026-10-29-overlay-panel-render-optimization.md) | ReadLease120ms probe8ms front spare dirty64 prefetch64 same pixels |
| 原生世界绘制与UI阶段分离、关闭绘制的限制 | [原生绘制停止可行性](2026-10-29-native-render-suppression-feasibility.md) | 4F4480 6D3D10 phase2 feedback 47EC90 A9FAB0 no-code-writes blocked |
| 小地图分页GPU面积缩小与变化页积分缓存 | [小地图GPU缩小](2026-10-29-minimap-gpu-area.md) | sparse128pages area4x4 prefix Float64 owner one readback CPU fallback |
| Vulkan GPU 场景、xBRZ、VXL、迷雾与故障回退 | [Vulkan 计算实验](2026-10-29-vulkan-compute-resolve.md) | Vulkan1.0 native1x xBRZ Float64 VXL 128MiB atlas one readback mask sticky CI x86 |
| 原版 xBRZ 1.9 两阶段 GPU 重建、原生 SHP 输入与设备自检 | [GPU xBRZ 算法与缓存](2026-10-29-gpu-xbrz.md) | argbUnbuffered2/3/4 preprocess rotations Float64 NoContraction golden 8167384 weak owner fence |
| 原生 SHROUD/FOG GPU 栅格、整数覆盖及不可变遮罩缓存 | [GPU 原生迷雾](2026-10-29-gpu-native-shroud-raster.md) | SHP127 0xFE area samples bins packed alpha Plan same readback bounded cache |
| VXL GPU 三角形光栅、确定性深度赢家和整帧缓存 | [GPU VXL 光栅](2026-10-29-gpu-voxel-raster.md) | Prepared64 atomicMax Z atomicMin tie VPL CPU pose deferred raster retained atlas |
| CnC-DDraw 表面/色盘更新、常驻纹理和最终 GPU 呈现参考 | [DDraw GPU 参考边界](2026-10-29-ddraw-gpu-reference.md) | Flip Blt Unlock palette D3D9 OpenGL persistent texture DirectComposition layered readback |
| 矿石动画仅重建受影响地形页并保留远处 GPU 素材 | [地形局部重建](2026-10-29-terrain-dirty-recomposition.md) | overlayFrame 128px dirty pages256 native bank COW xBRZ262 halo owner full fallback |
| 渲染层级、局部合成、页存储复用、原子滚屏与迷雾源/目标分离 | [反馈与性能修复](2026-10-29-render-feedback-performance.md) | layer5 Incremental monitor Hz epoch8192 zero allocations overscan96 atomic camera fresh shroud sourceHidden 1MiB |
| 普通指令线、真实色表与建筑放置红绿格原生反馈保留 | [原生命令与放置反馈](2026-10-29-native-order-placement-feedback.md) | DrawActionLine 4DC060 7049C0 47EC90 foundation cellsel sparse cutout readonly |
| 矿石/覆盖图误用alpha124与Drive/Ship原生坡面矩阵 | [覆盖图不透明与VXL坡面](2026-10-29-overlay-opacity-voxel-posture.md) | overlay255 ILoco674 Draw_Matrix 4AFF60 69F670 Ramp Slerp rocking 2688 |
| 小地图边缘半格 TMP 与渲染升级短暂失败/光标切换闪烁 | [裁剪与连续呈现](2026-10-28-minimap-crop-render-continuity.md) | sourceColor TMP extra signed Level fixed viewport completed sample age feedback cursor grace |
| 普通 CnCNetYR v17 全补丁／guard、真实相机退让、浮点及事件往返与保留所属方缺陷 | [普通版兼容性复核](2026-10-28-cncnet-ordinary-compatibility.md) | ordinary 6D8640 CameraClamp 39 245 75 151 ProtocolZero 0x30 NetworkHousePreserve BLOCKED |
| SHP／连续地形改用未修改的上游 xBRZ 1.9 与实际倍率解析 | [xBRZ 集成](2026-10-28-xbrz-integration.md) | xBRZ Zenju GPL alpha premultiplied live light TMP halo seams RGB8 source hashes |
| 光照前原素材判边、浮点重建与最终颜色量化 | [颜色重建精度](2026-10-28-light-aware-reconstruction.md) | unlit edge float light filter RGB8 emissive coverage |
| 当前视口 TMP／地面覆盖物目标像素颜色与深度合成 | [目标像素地形](2026-10-28-target-terrain-composition.md) | TMP overlay target pixels actual ratio cached terrain depth cliff shadow |
| 飞机5格战车120%覆盖与单目标限额派出 | [飞机伤害分配策略](2026-10-28-aircraft-damage-allocation.md) | aircraft damage 120% five cells single target accepted reservation IPC80 |
| 派出飞机的原生单对象取消选中与选区保留 | [原生单对象取消选择](2026-10-28-aircraft-selection.md) | Deselect 5F44A0 vtable150 CurrentObjects FindIndex aircraft selected subset |
| 实际游戏视口／目标像素比例与光照前 RGB 颜色精度 | [自适应采样与颜色精度](2026-10-28-adaptive-color-precision.md) | actual ratio 1.5x 3x 4x RGB888 continuous light RGB565 source palette |
| 素材高刷现场光照、同地址运行时色表变化与逐素材失效 | [现场光照与色表刷新](2026-10-28-live-light-palette-refresh.md) | live palette RGB565 LightConvert same-frame lighting content cache |
| 渲染升级目标像素深度合成、VXL 2× 几何与已观察状态素材高刷 | [精细场景与素材高刷](2026-10-28-material-refresh-detail.md) | device pixels 256 facings observed interpolation SHP alpha pause teleport cache |
| 普通 CnCNet 無額外反作弊的掉線排查：原版同步、ControlAll新增路徑與可見性／浮點修復 | [普通版不同步排查](2026-10-27-cncnet-ordinary-desync-triage.md) | native events OOS NativeMegaGateway MapIsClear 4147D5 70F1E3 MXCSR RecalcAttributes |
| 戰網實際部署辨識：普通／Hardened同檔名，官方stable／develop DLL均具加固機制 | [客戶端套件與DLL辨識](2026-10-27-cncnet-client-package-variant.md) | CnCNet-Spawner.dll cncnet5.dll yr-9.3.5 SHA256 FC50 49F20 actual loaded module |
| CnCNet Hardened 原理：程式碼／資料完整性、地圖規則封包CRC、事件拒絕及原版OOS與故意異常恢復分離 | [反作弊原理](2026-10-26-cncnet-anticheat-principles.md) | EA00 EB60 E530 401000 7E1000 E5E0 E690 E2E0 E3F0 WOLGameID EEE0 EF10 HouseIndex E530 return ESP CONTEXT VEH |
| 修改大師全圖的顯示／SpySat路徑、Skirmish採集與原版特殊格檢查 | [全圖與地圖檢查](2026-10-26-trainer-map-cheatcheck.md) | 6D88FC 577B3A 577D90 5C0E30 SkipCheatCheck Scenario RNG Session5 Phobos2115 |
| 官方CnCNet v17 Hardened實際DLL：ControlHouse同址與額外House驗證 | [Hardened二進位](2026-10-26-cncnet-hardened-binary-audit.md) | 50B6F0 EC50 EEE0 64BE75 .patch .syhks00 6D88FE SHROUD FOG |
| Hook 衝突、回復殘留／UI遮蔽與 CnCNet 額外檢查 | [Hook與平台檢查](2026-10-26-hook-conflicts-cncnet-checks.md) | HookRegistry partial restore NativeMegaGateway CnCNet hardened verification FileHash ProcessKill |
| 控制全部1.0.37／1.0.38回歸、Hook時機、同House跨發送者順序與Owner關閉對照 | [控制回歸審計](2026-10-26-control-138-regression-audit.md) | ControlAll 64C380 DoList order sender House ControlCommandOwner NativeMegaGateway |
| 透明／半透明迷霧、同步摘要隔離與逐像素管理器開銷 | [迷霧聯機審計](2026-10-26-shroud-network-audit.md) | 47F038 47F274 87E8A4 64DAB0 MapIsClear 1027 Validate halfShroud |
| 网络所属方五点适配、单位Owner四点发令、出售与任务仲裁 | [pre.6 网络所属方保留](2026-10-26-network-house-preserve-pre6.md) | 649CA0 64B660 64B4EA 541040 FrameInfo cache 6929E5 4AC179 646F4E 646EF5 646FD2 6FFC93 6FFE2B IPC79 |
| 自毁按钮、联网所属方覆盖与接收解包 | [pre.5 自毁与网络所属方](2026-10-26-destruct-button-pre5.md) | Destruct 08 649CA0 64B660 64BDD0 FrameInfo CurrentHouse OutList DoList |
| 1.0.38 小地图原生黑幕／雾边缘和雷达右下底色 | [正式版黑幕与底色](2026-10-26-native-minimap-shroud-and-underlay.md) | DrawShroud 4801F0 6D8700 SHROUD.SHP FOG.SHP alpha RadarColor 654EA0 |
| 原生雷達四側內縮、raw 2×1 地格裁切與完整幀閃爍 | [pre.12 南緣與發布](2026-10-25-minimap-bottom-pixels-pre12.md) | SetVisibleRect 567230 FillTerrainColors 654EA0 raw 1240 1244 south vector |
| 步兵／載具原生佇列、工廠 29+1 容額與逐幀預定提交 | [pre.12 單位預定研究](2026-10-25-unit-reservation-native-queue.md) | FactoryClass QueuedObjects Count 50 Object 58 ProduceDeferred UnitCapacity 30 |
| VERA20K 原尺寸渲染与状态绘制参考 | [pre.9 研究](2026-10-25-vera20k-rendering-reference.md) | vera20k GPU scaled_copy RenderZPolicy GPLv3 |
| 弹体采样与共享资产渲染集成 | [pre.9 研究](2026-10-25-projectile-renderer.md) | projectile SHP VXL pitch diffuse Inviso readonly |
| 弹体原版选帧、色盘、层级及专用VXL | [pre.9 研究](2026-10-25-projectile-native.md) | Bullet damage 468090 468B90 753C80 758670 |
| 维修扳手原版状态图标与专用色盘 | [pre.9 研究](2026-10-25-native-repair-wrench.md) | 6F5253 89DDC8 87F6C8 WRENCH no-Z |
| 民房驻军、受损状态和实际主体帧 | [pre.9 研究](2026-10-25-garrison-building-appearance.md) | 43EF90 CanBeOccupied Occupants ConditionRed 5760 |
| 小地图有符号高度与原生相位；raw 取景范围由 pre.12 更正 | [pre.9 研究](2026-10-25-minimap-south-boundary.md) | 480416 signed Level raw span backing 360 |
| 实景小地图分页原图、局部更新、面积缩放与预算 | [pre.9 研究](2026-10-25-native-minimap-composition.md) | NativeCanvas copy-on-write dirty area prefix capacity |
| 原尺寸观察窗、一次呈现缩放与动态效果集成 | [pre.9 研究](2026-10-25-watch-native-presentation.md) | 640x512 scale1 nearest final presentation repair projectile |
| 入舱／駐軍後跟蹤身份與空間宿主、持續取樣與即時生命週期 | [pre.8 觀察窗入駐跟蹤](2026-10-25-watch-passenger-spatial-host.md) | Transporter Occupants 688 694 readFresh spatial anchor |
| 礦石資源池及坡面圖案、TMP 位置表與損壞變體鏈 | [pre.8 礦石與地形變體](2026-10-25-resource-and-tmp-variants.md) | 47F871 B0F4E8 E0 E8 EC 2F0 2BC 8505 |
| 步兵獨立額外亮度與 FA2 HDM 光照沙盒對照 | [pre.8 步兵光照](2026-10-25-fa2-infantry-lighting.md) | ExtraInfantryLight 17D8 519457 66B6FF VPL FA2 sandbox |
| 小地圖黑幕跟隨顯示選項、半透明模式無黑幕與背景失效 | [pre.8 小地圖黑幕策略](2026-10-25-minimap-shroud-policy.md) | reveal half-opacity native fog immutable bank cache policy |
| 靜止切換隊形保留種類、按下／放開原樣提交預覽 | [pre.6 隊形切換](2026-10-25-formation-shape-switch.md) | click stretch shape spacing formationSwitch frozen preview slots |
| Tactical 可繪製邊緣與玩法範圍分離；Radar 准入另見 pre.12 | [pre.6 邊界更正](2026-10-25-renderable-map-borders.md) | Tactical 6D521C 568300 Radar 654EA0 7932 1458 |
| 切回遊戲後整組覆蓋層恢復與小地圖週期保活 | [pre.6 覆蓋層保活](2026-10-25-overlay-focus-keepalive.md) | Z-order WS_VISIBLE SetWindowPos DeferWindowPos Recovery foreground Tab |
| 礦柱原版資源色盤、物件亮度與向上繪點偏移 | [pre.6 礦柱色盤](2026-10-25-ore-mine-palette.md) | SpawnsTiberium 71C280 87F6BC UNITSNO ConvertClass 10A minus16 |
| 原版車輛／飛機額外亮度、已載入 VPL、光照與快取恢復 | [pre.6 VXL 亮度與恢復](2026-10-25-live-vpl-recovery.md) | ExtraUnitLight ExtraAircraftLight 17D4 17DC 73D0C3 414925 B41178 B2FB58 signature retry |
| 普通物件／動畫的原版色調來源與 LightConvert 色階對照 | [pre.5 物件色調](2026-10-25-native-object-lighting-source.md) | 705D70 705F7D 70718F 423114 556090 53 shades owner cell tint |
| 即時地圖可視範圍與觸發失效；繪圖遮罩推論已由 pre.6 更正 | [pre.5 地圖邊界](2026-10-25-live-map-boundaries.md) | VisibleRect 567230 578540 654490 31640 clamp fresh publication superseded mask |
| 觀察窗建築偏暗：不透明證據與雪地截圖精確對位 | [偏暗診斷](2026-10-25-preview-brightness-diagnosis.md) | alpha255 nearest 1429pixels RGB tint ColorScheme 705D70 |
| 多跟蹤窗瞬間失讀、非同步舊狀態與 2／3 窗重排 | [窗口生命週期修復](2026-10-25-watch-layout-lifetime.md) | watch frame focus control revision stale result 500ms layout |
| 原版 VXL 左下來光、VPL 漫反射／高光與模型局部座標 | [pre.4 原版光源](2026-10-25-native-voxel-light-direction.md) | 887470 B44318 753D00 7586F0 Normals4 directional light local Y |
| Bib、工廠生產基底／屋頂／門與放置過渡 | [pre.4 建築獨立部件](2026-10-25-native-building-components.md)／[渲染整合](2026-10-25-building-components-renderer.md) | 43D290 1518 14E4 14FC 150C 4A52F0 Unload ActuallyPlacedOnMap |
| 原版建築動畫讀取更正、遮擋、VPL/陰影與小地圖對齊 | [RenderFix2 實證](2026-10-24-native-animation-alignment-lighting.md) | OwnerObject=0 Value+Start 451890 422BC0 126 observations MinimapAlign |
| 实景小地图发布门限、常驻动画与队形透明度 | [实景/动画/预览修复](2026-10-25-minimap-animation-preview-fix.md) | endFrame latest frame-zero NewTheater alpha 124 fairness |
| Ares/spawn 共用进程发现与窗口选择 | [pre.2 Ares 发现检查](2026-10-24-ares-process-discovery-138-pre2.md) | gamemd-ares.exe gamemd-spawn.exe B73550 Windows fixture PID |
| 深度圖重複偏置、建造/售賣/全域動畫、非同步覆蓋層與快取 | [RenderFix3](2026-10-25-renderfix3-async-native-depth.md) | Fetch_Z_Data 45E8F0 Buildup E00 Stage F8 latest_worker 1000% |
| 副相机逐像素深度、实际建筑动画、实景小地图和统一层级 | [pre.2 渲染集成](2026-10-24-camera-pipeline-138-pre2.md) | VPL BUILDNGZ AnimClass 55C CCINI overlay stack |
| 游戏已解析图标、live Rules/Art INI 与文件回退 | [pre.2 INI 与图标](2026-10-24-live-ini-cameos-138-pre2.md) | CCINI 887048 887180 7E1AF4 Image Cameo RLE |
| 两格内敌方标识聚类与平滑位置聚合 | [pre.2 聚类](2026-10-25-enemy-clusters-138-pre2.md) | 512 leptons pairwise centroid Motion owner type |
| 副摄像机 ccmaps-net 光照/VPL/逐像素深度/建筑动画参考 | [1.0.38-pre.2 研究准备](2026-10-25-ccmaps-rendering-138-pre2.md) | VPL Palette ZBuffer ActiveAnim SHP TMP planned |
| 地图/随处建造直接模式、科技加强字段恢复与暂停售景高刷/缩放 | [1.0.38-pre.1 研究](2026-10-25-direct-modes-138-pre1.md) | 508E60 577B3A 4A8EC3 TechLevel 634 -255 Restore Assist70 |
| 铁幕内圈锚点、状态标识连续移动与副摄像机遮挡/刷新/层级 | [pre.14 研究](2026-10-24-display-anchor-137-pre14.md) | 82D028 IronCurtain 346 28.5 visual anchor UpdateWindow Motion terrain layers |
| 三窗跟踪/组内死亡接替、方框队形、任务候选与相机中心 | [pre.13 补充研究](2026-10-24-tracking-formation-missions-137-pre13.md) | Hunt Patrol Wait 5B35ED 6D8B30 WatchSlots 5-cell Assist69 |
| 高度光照、覆盖物/投影、零拖拽队形、磁电分端、菜单避让和显式联机Sticky | [pre.13 研究](2026-10-24-height-overlay-sticky-137-pre13.md) | 484680 10A 10C 10E 480110 5FDCC0 B0CD48 zero-drag Recovery MegaMission6 Assist69 |
| 地图当前光照、临时色调、建筑预留与机动防空有限接敌 | [pre.12 研究](2026-10-23-lighting-advance-137-pre12.md) | AmbientCurrent 352C LightConvert 556090 Advance Guard 120% 128单位 Assist67 |
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



- [1.0.37 正式版设置持久化、默认值与显示调查](2026-10-24-stable-settings-137.md)


- [2026-10-26：联机事件、原版每帧摘要与飞机联机实验](2026-10-26-network-event-audit.md)：基线 v1.0.38，原版64DAB0的ROL+加法摘要、科技limitOnly路径、FrameInfo历史比较、原生规划本机DoList、Sticky的BalloonHover条件路径和Ctrl+Alt重复门槛测试。

- [2026-10-26：32 种任务和 Sleep / Guard / Harmless 模式](2026-10-26-selected-mission-modes.md)：MegaMission=04 携带任务编号；真实32项分派、四类对象处理器、空处理器与AttackMove预处理；Selling/Rescue/Wait限制；pre.2 三组可配置按键的事件投递及验证边界。

- [2026-10-26：建筑格进入、火控留枪与2%索敌](2026-10-26-entry-fire-control-pre3.md)：Capture/Sabotage目的地Cell；工程师Ctrl+Alt对象/格子分支区别；Sleep预留及短期已射伤害；原版double Verses与float阈值边界；原生索敌过滤的联机范围。


- [2026-10-26：对手建筑变卖实验与所属方自毁事件](2026-10-26-enemy-sell-pre4.md)：Sell=0x16、实际Owner编号、默认关闭及旧配置迁移；Destruct=0x08、Exit/AboutToExit/RemovePlayer区别；4FC980标志与定时器、4FC6D0销毁后继及联机证据边界。

