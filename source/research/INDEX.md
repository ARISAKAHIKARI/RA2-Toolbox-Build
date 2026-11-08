# 逆向研究索引

从 2026-10-21 起记录新研究，不补录旧工作。

持久笔记：`RA2-Reverse-Engineering-Notes.sqlite`。表 `notes` 保存结论、依据和未决问题；`notes_fts` 可按英文关键词检索，中文可用 `LIKE`。后续更新同一文件，避免每版另建副本。

| 主题 | 新笔记 | 关键词 |
| --- | --- | --- |
| 1.0.45 完整包精简与依赖按需获取 | [固定官方来源、后台准备、任务隔离和发布验证](2026-11-08-release-145-on-demand-dependencies.md) | optional-python numpy scipy opencv pinned-download immutable-components cancel profile-lock lightweight-full |
| 逐功能点补充审查 | [功能/模式清单、七处修正、未闭合问题与真实验证边界](2026-11-08-feature-point-audit.md) | per-feature repair unload cursor-shape frame-diagnostic paused-overlay broker-lock retained-modes |
| 模块性能与正确性审查 | [五类缺陷修复、功能覆盖与未决热点](2026-11-08-module-performance-audit.md) | reverse-dodge stale-subscriptions sealed-terrain palette-token async-section-save pre5 |
| 控制全部直接放行 | [原生 true/strict 叶、维护资格发布与固定槽掩码](2026-11-08-control-direct-permission-hotpath.md) | pre4 direct-yes control17to1 maintenance-publication not-per-hit-world strict-owner same-DR explicit-behavior-change |
| 硬件异常热路径第三轮 | [科技候选预计算、固定比较结果与本方上下文精简](2026-11-08-hardware-precomputed-tech-and-owned-dispatch.md) | pre3 candidate-index Full early-reject Enhancement64 constant-flags static-reader same-live-guards flat-dispatch no-hit-count-reduction |
| 原始世界样本与消费者实际接入 | [capture/seal、动态跨进程 payload、战术与视图阶段拆分](2026-11-08-shared-world-sampling-implementation.md) | Assist101 pre2 raw-overlap single-producer demand sealed no-live-fallback dynamic-mapping nothrow pause UI-generation source-bound-tests |
| 硬件异常热路径第二轮 | [外部异常快速分流、维护复用与真实命中窗口](2026-11-08-hardware-dispatch-and-rate-observation.md) | owned-foreign classify-before-scope same-context maintenance-view telemetry1s no-per-hit-clock inherent-VEH-cost |
| 每逻辑帧一份原始世界样本 | [现存重复入口、共享读集与统一采样待实现契约](2026-11-08-unified-world-sampling-contract.md) | cee2af56 pre1-epoch-only FrameReadSet capture-seal projection LiveRead SceneBank-overview minimap no-new-implementation |
| 硬件控制与工具箱计算的重复工作 | [准入读合并、同帧 Scatter 门控与批内 fresh selector](2026-11-08-hardware-control-and-host-compute-hotpaths.md) | bf3fbe88 admission24to3 VEH-unchanged Host-demand reverse-sync raw-batch AntiTheft-retained operation-counts-not-FPS |
| 科技加强规则模式总统／校车未显示 | [原生枚举反证、Ares 动态图标与现场边界](2026-11-07-tech-enhancement-rules-native-followup.md) | PRES BUS native-CanBuild 8-cases Insignificant-not-native-filter rules-mode-unresolved |
| 科技加强的总统／校车目录缺口 | [特殊类型标志、身份限定准入与原生边界](2026-11-07-tech-enhancement-special-catalogue.md) | pre14 PRES BUS Insignificant typeSlot owned-identity legacy-vs-exact native-sidebar-unverified |
| 4K/2K 光标坐标域与取点失败恢复 | [pre.12：物理取点、DPI 作用域与正常点位保持](2026-11-07-cursor-coordinate-recovery.md) | GetPhysicalCursorPos PMv2 same-domain failed-sample no-position-ratio no-SetCursorPos |
| 2K 左上输入画布拉满 4K 的光标位置 | [pre.15：呈现映射与原生光标退让](2026-11-07-cursor-fullscreen-presentation.md) | GetClipCursor canvas-to-monitor compatibility-inference effective-native overlay menu-blank no-SetCursorPos |
| gameares 漏识别与游戏连接恢复 | [pre.11：模糊描述、追加检测名单、身份复核与真实状态](2026-11-07-game-process-discovery-recovery.md) | FileDescription allowlist gameares PID-birth background-metadata one-way-reconnect hidden-status |
| 强制注入平台的只读 Hook 探测 | [pre.10：磁盘基准、代码差异、覆盖范围与解释边界](2026-11-07-platform-hook-detector.md) | disk-baseline no-clean-live-state PE relocations coverage candidate-chain writer-unknown 161-reference-intersections |
| 自动维修归组与自动功能开关快捷键 | [pre.8：维修间隔、共享状态与异步取消](2026-11-07-automatic-controls-repair-frequency.md) | 13-toggles 10-new-keys RepairMs late-result cancellation manual-isolation hotkey-conflicts |
| Ares/Phobos 与其他注入的适配边界 | [固定上游、入口交集与分层适配](2026-11-07-injector-compatibility-adaptation.md) | 380-source-files 7-exact-15-dependency-intersections CanBuild+7 Interop-major SyringeEx no-new-universal-adapter |
| 调试入口页与逐入口签名开关 | [pre.7：独立 Call/Patch/DR、真实状态与 owned 恢复](2026-11-07-debug-entry-signature-controls.md) | Assist100 stable-ID INI seqlock 161-entries six-DR policyRevision actual-snapshot owned-restore no-live-game |
| 底层等待、错误冲突与平台调用适配 | [pre.9：同步断点事务、桥接退役、有限 Phobos 合同](2026-11-07-wait-recovery-and-platform-entry-fixes.md) | owner-rendezvous witness no-suspend WAIT_FAILED retry journal-recapture 15-intersections bounded-profile |
| 原生入口依赖隔离与失败恢复 | [pre.6：21 项局部依赖、硬件失败重查与连接退避](2026-11-07-runtime-dependency-isolation-and-recovery.md) | LocalNative Unused Full-control500ms bridge2s30s broker1s completion-clock no-live-game |
| Ares 原生入口与科技参数兼容 | [pre.5：普通函数调用、平台跳转和实际断点分离](2026-11-07-ares-native-entry-compatibility.md) | Runtime10 native-callable Ares 6AA600 5F7900 4F7870 Full parameters Exact-local-status |
| 科技加强中途开启的建造栏刷新 | [原生重查与有效目录刷新](2026-11-07-tech-enhancement-midgame-refresh.md) | pre4 RecheckTechTree House1FC exact-catalogue request-edge active-Master queues-preserved |
| 点死蜘蛛后矿车发呆 | [快速完成、残留字段与恢复确认](2026-11-07-spider-kill-work-resume.md) | pre4 Guard missed-Attack dead-target queued-not-complete manual-priority |
| 点杀蜘蛛的参与单位独立配置 | [独立类型清单、地面单位恢复与手动优先](2026-11-06-spider-defender-types.md) | Assist99 SpiderDefense independent-types Infantry Unit Guard AreaGuard manual-hold |
| 矿车自动点杀蜘蛛、手动优先与工作恢复 | [独立开关、N／N+1 与排队意图保留](2026-11-06-miner-spider-defense.md) | Assist98 native-events manual-priority FIFO-intent synthetic-memory no-new-hooks no-live-game |
| 建築末幀、星級縮放與單機 FPS | [普通 SHP 與精確單機限速](2026-11-05-animation-rank-single-player-fps.md) | TimeToDie slot Start+Value rank 55D7BC QPC Assist97 tag32 SP-only |
| Hook 清理漏掉 Cursor 佈局槽與故障退休 | [Runtime(9)、兩槽共享與27項隔離檢查](2026-11-05-hook-cleanup-shared-tooltip.md) | gate6 7F74C8 7F74D8 fault cleanup-only pinned reconnect SINGLE_STEP-unresolved |
| 新 MRGA 信标函数证据（未投产） | [新采集 ABI 与严格 profile 候选](2026-11-04-platform-beacon-current-capture.md) | 431410 C3A000 153CD0 ret4 proposed-only |
| 平台 Reveal 上下文跳板与采集缺口 | [原生 fallback 不等于透明授权](2026-11-04-platform-reveal-wrapper-boundary.md) | 577D90 577E1E 577F03 MEM_PRIVATE handler-missing no-authorization |
| 蜘蛛自动功能的 11-04 历史暂停检查点 | [当时字段与未完成方案](2026-11-04-parasite-automation-pending.md)；[11-06 矿车防蛛实现](2026-11-06-miner-spider-defense.md) | historical-pause parasite-service-pending manual-priority queue-semantics |
| pre.4 导弹输入聚合临时与栈溢出 | [超大临时与原位重置](2026-11-04-missile-input-stack-overflow.md) | C00000FD BA672 418D0 Input104418 placement-new DMI Zig013 |
| 平台实机 Hook 已连接但可选入口阻断全局 | [19项核心与命令依赖隔离](2026-11-04-platform-startup-dependency-isolation.md) | commandStatus3 431410 577D90 E9 core19 ClearBeacons RevealMap SpySat owned-only |
| 随机名称平台的后台子渲染捕获 | [旧逻辑历史与全窗口树发现](2026-11-04-background-renderer-discovery.md) | random-name background child EnumWindows EnumChildWindows once-per-PID strict-image fingerprint MRGA preserved |
| 硬件断点热路径性能 | [即时签名核验、归属分类与诊断计数](2026-11-04-breakpoint-hotpath-performance.md) | Full/Enhance no-RPM/FNV hot snapshots owner counters foreign passthrough real-DR comparison |
| 单槽硬件黑幕显示开关 | [共用四槽、互斥与 Assist96](2026-11-04-map-visibility-one-slot-switch.md) | Map bit2 ZF6D88FC slots23-vs29 default-off display-only pre2 |
| 超武每帧持续授予实验开关 | [原生动作、开关生命周期与 Assist95](2026-11-04-superweapon-per-frame-switch.md) | GrantPermanentSW nativeAction per-observed-frame default-off no-DR no-broadcast |
| 地图全开的硬件断点候选 | [单槽黑幕帧选择与完整功能边界](2026-11-04-map-visibility-hardware-candidate.md) | GetOcclusion 6D88FC ZF Cell12C one-slot five-sites no-write |
| 原生放置跨帧点击与 Q/W 入队清理 | [调度刷新、双缓存与完成观察](2026-11-04-placement-input-queue-cleanup.md) | Host-ready0 exact-frame MouseUp734270 QW queued cleanup |
| 超武每帧持续授予与永久保留 | [动作34、CanHold 与接收端使用检查](2026-11-04-superweapon-per-frame-permanent-grant.md) | 6E1C40 CanHold60 180frames AddCameo Fire_SW multiplayer |
| 控制全部硬件入口与共用四槽 | [严格所属编号、Full 单入口与 Assist93](2026-11-04-control-hardware-parameters.md) | HWBP VEH control50B6F0 CanBuild4F7870 1+2+1 no-text no-owner strict-return |
| WebView2 控制臺啟動失敗急修 | [真 Runtime 導航自我取消、精確 URI／ID 與 native smoke](2026-11-04-webview-navigation-startup-hotfix.md) | WebView2 NavigateToString data-uri about:blank NavigationId OPERATION_CANCELED Host-only |
| 科技加強精確等級參數 | [21白名單、原生Jcc／四DR交易與雙向在產交接](2026-11-03-tech-enhancement-exact-parameters.md) | Assist92 TechEnhance EFLAGS 4F7951 4F7B66 exact-Level1 four-slots legacy-handoff no-text no-rules |
| 1.0.41-pre.1 MO 接續與視野依賴 | [倍率、雷達遮罩、退出夾限及完整構建](2026-11-03-pre1-mo-followup.md) | Assist92 MAX radar-only native-widget retirement 6D8640 SmartArea WebView2 |
| MO 區域進樓、公平配額與操作政策 | [Select/Move補令、pending容量與七欄捕獲](2026-11-03-mo-smartarea-and-policy.md) | SmartArea InfantryAbsorb Enter7 forceConstructed Options28 Assist92 Commit |
| 1.0.41-pre.1 Web 控制臺整合 | [真正三方合併、15 組收納與即時操作時效](2026-11-03-console-pre1-integration.md) | WebView2 4b6bb9cc 35-delta metadata runtimeError immediate TTL owner-thread |
| MO 地圖縮放 Host A/R 整合 | [物理錨點、換代失效、能力握手與啟動探查](2026-11-03-mo-viewport-host-projection.md) | Assist91 Transform physical logical generation HostHandshake Lifecycle mappingLost probeDue |
| Native viewport 首幀與重連未知狀態 | [唯讀 mapping 探測、101 項真 Win32 驗證](2026-11-03-viewport-first-frame-probe.md) | probeExisting probeAbsent HostHandshake first-frame prior mapping PID creation |
| 收納狀態中的真正錯誤與還原失敗 | [錯誤自動展開、偏好還原與 4063 項驗證](2026-11-03-console-fold-error-visibility.md) | failedSettingGroups restore CRC network control tech Master error-visible |
| 控制臺摺疊與當下選取所屬方自毀 | [14 組收納、一次性快捷鍵與高危標記](2026-11-02-console-folds-and-selected-owner-destruct.md) | Destruct arg0 owner selection immediate hotkey1735 folds14 CRC high-risk |
| 原生放置高地底邊與待確認產品重啟 | [578540 true、高程邊界與重複入隊入口](2026-11-02-placement-height-boundary-reentry.md) | 578540 CoordinatesLegal 568300 bottom elevation pending Place |
| Place 成功後只清原產品的原生游標 | [有限監看、active/copy 原生 setter 與產品身分](2026-11-02-native-placement-completion-ui.md) | 4A8BF0 4A8D50 product UID OnMap completion no-fake-click |
| 科技不改指令／規則的可選原生面板模式 | [硬體斷點、參數權限、衝突與恢復](2026-11-02-tech-ui-parameter-interception.md) | Assist90 buildLimitOnly HWBP VEH parameter native-sidebar no-text no-rules |
| 放置預覽幀差、原生中心與落地後 UI 收尾 | [持續性、地形高度、viewport 與產品確認](2026-11-02-native-placement-persistence-and-projection.md) | Place foundation 4A8BF0 4A8D50 578080 forward-frame viewport completion |
| 地圖觸發後 backing 不變的活動投影 | [原生 SetVisibleRect 與小地圖私人採樣](2026-11-02-trigger-radar-active-projection.md) | 654490 ActiveProjection backing bounds SceneBank trigger |
| 小圖單一維度拖屏固定 | [MOEnhancement 分軸行為及獨立開關](2026-11-02-small-map-axis-clamp.md) | 6D8640 Clamp_Center one-axis viewport opt-in |
| 科技不改程式碼／規則的 UI 參數攔截候選 | [兩處原生 CALL、棧參數與未驗證斷點方案](2026-11-02-tech-ui-only-breakpoint-feasibility.md) | CanBuild Classic buildLimitOnly 6A97D2 6AA781 DRx VEH no-text no-rule research-only |
| MOControls2 新操作合入，保留本分支 Hook 策略 | [配置、超武面板、原生閉環與合併邊界](2026-11-02-mo-controls-scoped-merge.md) | Assist88 configurable gestures fresh PlanToken PlanConnect loopStart PlanCommit scoped-merge |
| 隱藏光標保留提示框與偏好 | [原生文字／佈局雙槽修復](2026-11-02-cursor-hide-tooltip-layout.md) | Cursor155 RefCount 7F74C8 478BA0 tooltip layout no-high-risk |
| 功能收納、渲染耗時與副相機連續定位 | [本輪實作、對照與完整構建](2026-11-02-render-ui-followup.md) | settings-folds Timeline VXL HVA geometry prefetch InstanceOrder current-anchor |
| 原生面板科技資料模式與還原責任 | [規則欄位、面板與模式切換](2026-11-01-native-sidebar-tech-data-mode.md) | CanBuild data-journal TechEnhance native-sidebar Assist87 |
| 原生事件任意建造正式選項 | [完工產品、游標與入隊邊界](2026-11-01-native-placement-event-implementation.md) | Place factory-UID frozen-cursor no-code |
| 觀察窗誤消失與讀取能力遺失 | [生命欄位、換代、批讀與輸入重用](2026-11-01-watch-lifetime-reader-performance.md) | IsAlive IsOnMap display-epoch typed-prefetch Timeline |
| 小地圖星級的最終面板合成 | [相同場景標記與 CPU/GPU 對照](2026-11-01-radar-final-panel-insignia.md) | PIPS14 PIPS15 physical-pixels immutable-scene WARP |
| 建造提示按檔位上限換算 | [原生速度分支與估算規則](2026-11-01-build-time-speed-ceiling.md) | GameSpeed A8EB60 multiplayer-cap 647C43 nominal-FPS |
| 原生 Place 接收鏈與超距離／陰影放置候選 | [實際判定鏈與 12 項隔離驗證](2026-11-01-placement-event-reception.md) | Place0B ObjectUnlimbo5F4F3C legality no-text native-event |
| 科技全開的規則資料／原生生產事件候選 | [原生門檻與 31 項隔離驗證](2026-11-01-tech-no-code-feasibility.md) | CanBuild FindFactory Produce PrerequisiteOverride DoubleOwned data-only no-text |
| 快取預算、重複計費與整庫清空 | [只讀審計與容量調整邊界](2026-11-01-cache-budget-audit.md) | TotalBytes-unused per-context shared-owner LRU RAM VRAM resident |
| VXL 舊新姿態同占預算導致永遠無法發布 | [重現、修復與回退素材保留](2026-11-01-pose-cache-transition-liveness.md) | PosePixels displayed wanted cache self-lock current frameFrom immutable-front |
| 動態物件 palette 捕獲的重複跨進程讀取 | [首次欄位批讀與雙讀驗證](2026-11-01-palette-read-batching.md) | first-field bulk fresh RPM identity parent palette-race fallback |
| 地形逐畫面重複查詢、完整世界與新鮮驗證 | [正式渲染器對照與頁快取成本](2026-11-01-terrain-presentation-read-cost.md) | converter metadata cell-intensity fresh-verify ProcessReader full-world |
| 控制標記替換為修改大師基本指令模式 | [配置、所有權與產品驗證](2026-11-01-master-control-implementation.md) · [原生指令及 ABI](2026-11-01-master-control-native-audit.md) | Assist86 Master direct-jump 50B6F0 6339BA 637E40 strict-before-common restore-order |
| 任意建造不改程式碼候選與資料旗標區分 | [原生 Place、距離與落地合法性](2026-11-01-placement-no-code-feasibility.md) | Place0B proximity4A8EB0 AdjacentEB4 PlaceAnywhere1703 Unlimbo preliminary-legality |
| 局中首次啟用：連接需求漏項與具體阻塞診斷 | [已修缺口、冷啟用與實機邊界](2026-11-01-midgame-feature-activation.md) | needsBridge cold-enable requested-applied commandStatus Scenario fixture |
| 動態 VXL／SHP 核心、素材重用與裁切前准入 | [實作、來源與逐位對照](2026-11-01-actor-render-performance.md) | HVA normal-pages SIMD SHP physical-material VPL clip-admission |
| 回退前後 CPU 繪製效能與安全並行 | [歷史對照與分區回放](2026-11-01-shp-kernel-and-cpu-replay.md) | HF29 parallel-rows full-target ordered barriers exact-Z-A |
| 全鏈路顯示效能：讀取、排程、計畫與呈現 | [修正、受控量測與實機邊界](2026-11-01-end-to-end-render-performance.md) | batched-RPM COW-TMP CPU-plan-reuse immutable-panels dirty-upload timing |
| 覆蓋層連續性、面板尺寸合成、星級與觀戰 Hook 保留 | [修正、原生 HWND 寬度與驗證邊界](2026-11-01-render-continuity-and-hook-lifetime.md) | camera-independent HUD checked materials output-sized renderer HWND DWORD scene-bank observer hook-ownership |
| 科技純資料候選、同世界 epoch 改變後還原責任 | [尚未實作的候選與已修的還原錯誤](2026-11-01-tech-data-candidates-and-recovery.md) | CanBuild direct-E8 Type634 Prerequisite648 Scenario epoch restore |
| 控制標記資料模式、跨epoch還原與普通事件功能分類更正 | [實作、互斥與驗證](2026-11-01-control-house-flag-mode.md) | Assist85 House1ED data-only mode2 native-query classification epoch ownership |
| 控制／科技純資料方案核對，科技清單一次刷新 | [判定分支、舊版依據與驗證](2026-11-01-control-tech-data-only-review.md) | House1EC 1ED CurrentPlayer 50B6F0 TechLevel 634 full-pass no-frame-slicing |
| 深入不同步審查：失敗恢復、TID 回收、過期指令與查詢副作用 | [六項修正、原生查詢高危選項與驗證](2026-11-01-deep-desync-followup.md) | Assist84 native-query opt-in frame-write-failure thread-identity stale-heartbeat shroud-read-retry AirPathMove |
| 優先無程式碼改寫、原生事件與高危分類 | [功能遷移、51 站點與驗證範圍](2026-11-01-no-code-preferred-feature-migration.md) | no-code-preferred v0720 native-events polling material-alpha high-risk migration |
| 恢復普通聯機幀通知與測試預設，保留切屏修正 | [使用者確認的回退範圍](2026-11-01-frame-notification-policy-rollback.md) | native-frame session0/5/3/4 no-forced-polling no-default-skip focus-retained |
| 測試器切屏誤停、可選基線與手動略過 | [日誌對照、焦點生命週期與回歸](2026-11-01-hook-test-focus-selection.md) | no-native-OOS seqlock retry focus-retains-hooks optional-baselines early-verdict shared-site-exclusion |
| 高刷光標吞掉單位／價格提示；RA2OL 第三組幀通知首次 OOS | [原生分支、共同槽所有權與日誌結論](2026-11-01-cursor-tooltip-frame-oos.md) | RefCount -1 GetToolTipText479050 Sidebar6D1800 shared7F74D8 Frame55DE81 message-fallback explicit-diagnostic not-detection-proof |
| 依使用者要求撤回首次同步／科技模式封鎖，修正不完整陰影發布 | [回退範圍、欄位與回歸](2026-11-01-hook-gate-rollback-shroud-publication.md) | no-peer-sync-prerequisite TechEnhance opt-in incomplete-mask keep-front selector-read-failure projection-offset |
| RA2OL 同步錯誤的逐組 Hook 測試版、隱式相機補丁與小地圖閃爍候選 | [流程、來源證據與實測邊界](2026-11-01-ra2ol-hook-isolation-diagnostics.md) · [操作／編譯說明](../hook_diagnostics/README.md) | RA2OL 30seconds explicit-verdict isolated-groups 48+3 14denied CameraClamp6D8640 diagnostic-opt-in incomplete-shroud not-netplay |
| 首次同步之后仍会分叉的直接写入与恢复遗漏 | [后续不同步审计与修复](2026-11-01-latent-desync-mutation-audit.md) | TechLevel SP-only failed-write ownership retry MapIsClear read-failure premature-unhook stop-ack all-patches native-query side-effects |
| 宿主 Hook/订阅生命周期与目标槽回收 | [Hotfix33 回收修正与受控证据](2026-10-31-host-hook-lifecycle-hotfix33.md) | 8targets 32leases 1404 retained-process reap failed-unhook capacity |
| 首次远端 FrameInfo 匹配门槛、51 站点清单与重连/恢复审查 | [实现与联机风险边界](2026-10-31-peer-sync-hook-activation-audit.md) | pending-executed CRC-history read-only activation 8targets 32leases invalid-HHOOK cleanup-handoff control-native-fallback |
| Hotfix32全图MapIsClear实验镜像、自然写入与开局/局中启用时机 | [实现、地址、生命周期与验证范围](2026-10-31-crc-map-shadow-hotfix32.md) | Assist83 MapIsClear shadow 8sites natural-write runtime21 lifecycle5 cached-signatures computeApplied new-epoch not-netplay |
| CRC原值镜像机制与当前功能的直接/间接影响 | [模型、14项原生验证及边界](2026-10-31-crc-shadow-and-feature-classification.md) · [证据](evidence/crc-shadow-research.json) | MapIsClear64DCCA mirror natural-write same-value identity no-production-hook CRC-input |
| Hotfix31连续House分包、协议0匹配帧头与CRC边界 | [实现与受控验证](2026-10-31-network-house-grouping-hotfix31.md) · [36项证据](evidence/network-house-grouping-hotfix31.json) | FrameInfo649D61 protocol0 limit649DBC globalCRC no-recompute native7sites own-wire-identical |
| Hotfix30完整A查表、原生专用内核与精确整帧SIMD | [实现、对照与测量](2026-10-31-native-blitter-performance-hotfix30.md) · [原始轮次](evidence/native-blitter-hotfix30.json) | blitter full-A SSE2 127 exact 8192 four-plane 1440p CPU12.505ms not-game-FPS |
| YRpp原生绘制性能参考：专用blitter、A查表、RLE跨度、VXL姿态缓存和可见/脏区 | [源码核对与路线](2026-10-31-yrpp-native-rendering-performance-reference.md) | YRpp d979554 AlphaLightingRemap palette RLE voxel cache dirty areas native packed |
| HF29之后逐项评估回合与CPU冗余初始化候选 | [合回条件、对照和下一步](2026-10-31-performance-remerge-after-hotfix29.md) | confirmed-recovery separate-branch CPU memory initialization benchmark no-direct-output |
| Hotfix29恢复完成帧主体、GPU回读呈现与原始素材 | [恢复方案与验证](2026-10-31-stable-render-recovery-hotfix29.md) | complete-front single-owner no-interop CPU full-surface waterline fence bounded1500 quality-paused |
| HF28整合自审：迁移不完整、阶段/水线Z/最终呈现边界与测试缺口 | [源码问题与验收范围](2026-10-31-render-integration-self-audit.md) | BuildingBase Display Extras Spotlight CPU GPU half-pixel waterline duplicated source work SEH DirectComposition untested row bands |
| Hotfix28用户CPU乱序/约250ms与GPU崩溃反馈：显示清单、并行合成、故障隔离 | [修复与证据边界](2026-10-31-render-recovery-hotfix28.md) | Display8A0360 stride24 native order row bands Z A cloak barrier palette lookup CPU reuse GPU SEH fault log no live proof |
| Hotfix27后渲染遗漏与建造五因素复核 | [当前遗漏/计时判定](2026-10-31-render-timing-audit-after-hotfix27.md) · [证据](evidence/render-timing-audit-after-hotfix27.json) | BuildSpeed power54 rate255 ready+1 unproven health brackets bomb selfheal talk control-link airstrike Fly Hover Jumpjet Rocket phase Chrono radial |
| Hotfix27建造提示框、54格/供电期限与GPU范围/电弧/建筑混合部件 | [计时/ABI/实施与边界](2026-10-31-build-time-and-render-hotfix27.md) · [证据](evidence/build-time-render-hotfix27.json) | CCToolTip7F74D8 TimeToBuild6F47A0 Factory54 rate255 LowPower4CA6E0 DesiredFPSA8B558 radial pairedAA EBolt phase building B8 C0 recoil3D8 3F8 |
| Hotfix26原生Z/A、跨页位移隐形、完整色阶与光束/临时灯光 | [原生字段/实施记录](2026-10-31-native-surface-hotfix26.md) | unsigned16 signed candidate ABuffer AlphaShape word-offset Foot ZFudge native Wave RadBeam2 Spotlight90 FullColorData Laser AA transient light |
| Hotfix25共同帧输入、来源历史、协作快照与协议81 | [输入/执行/ABI](2026-10-31-coherent-compute-frame-hotfix25.md) | computeFrameSeq latest source history 16 consumer bits cooperativeInputFrame protocol81 |
| Hotfix25卸载顺序、特殊pips、范围提示、CnCNet隐形准入与Phobos指示器 | [实施/字段/差异](2026-10-31-render-counters-hotfix25.md) · [原生证据](evidence/render-hotfix25-native.json) · [固定上游](evidence/render-hotfix25-upstream.json) | tether Unload16 spawn garrison absorber mind-control radial 6ED harvester active power delta |
| Hotfix24原生帧完成通知、独立高刷位置与保留实例更新 | [原生位置/ABI/边界](2026-10-31-frame-notified-high-refresh-hotfix24.md) · [构建记录](evidence/render-hotfix24-final.json) | 55DE81 A8ED84 SetEvent latest bounded extrapolation shadow depth 64-byte |
| Hotfix23第二轮11入口：原生保存图/材质/尾迹、地面层、信标雷达、PixelFX和Chrono | [实施/字段/边界](2026-10-31-vera-scene-extras-hotfix23.md) · [静态证据](evidence/render-hotfix23-native.json) | Fogged8B3D10 trailABCB78 CellFC PixelFX7EFDA4 ChronoA9FAB0 sin84F084 Rubble14F4 Smudge3C |
| Vera第二轮绘制缺口：迷雾保存图、隐形伪装、沉船、污迹、尾迹、信标、雷达事件、桥影、废墟、闪光与整屏扭曲 | [额外11项审计](2026-10-31-vera-render-additional-gaps.md) | FoggedObject DrawState Sinking Smudge LineTrail Beacon RadarEvent BridgeShadow Rubble PixelFX ChronoScreen SLOP ZShape |
| 真正Z路径点/规划图、共享前驱/回环/高亮与剩余绘制差距和GPU路线 | [Hotfix22路径点](2026-10-31-native-planning-render-hotfix22.md) · [差距/GPU清单](2026-10-31-vera-render-gaps-and-gpu-roadmap.md) | PlanMgr AC4B30 AC4C78 token514 event NetID loop fresh readset UI5 static terrain async GPU HVA A-buffer |
| Vera绘制12领域梳理、GPU动态原尺寸页、延后色盘、565混合、波形依赖及资源UI | [迁移总表](2026-10-30-vera-render-migration-map.md) · [Hotfix21实施](2026-10-30-vera-render-integration-hotfix21.md) | indexed palette native page Z readonly RGB565 dependency wave resource pip PC53 chop UI isolation |
| 小地图 GPU 完整帧、盯防共用世界管线、PIPS2 与采矿 SHP | [世界／UI 迁移 Hotfix20](2026-10-30-world-ui-minimap-hotfix20.md) | direct minimap no readback ordered effects watch shared world PIPS2 ANIM plain Convert no native holes |
| 世界输入常驻、当前相机整帧与观察窗 GPU 直出，删除边缘复制 | [呈现管线 Hotfix19](2026-10-30-world-pipeline-hotfix19.md) | world snapshot camera redraw tactical scissor sidebar shared SRV direct watch no edge replication |
| Vera20K 批绘、GPU 直出、场景/VXL 耗时、矿石与树影 | [渲染 Hotfix18](2026-10-30-scene-hotfix18.md) | shared texture DirectComposition no readback ordered effects wave VXL pose tint LRU ore Terrain shadow |
| 1000 Hz 最新状态采样、GPU 回读/深度缓存、拖屏连续性与有界退出 | [渲染延迟 Hotfix17](2026-10-30-render-latency-hotfix17.md) | latest snapshot high resolution readback cached depth reuse scroll close timeout parity merge |
| CPU/GPU 共享场景对象清单、原生 GPU 与画质解耦、静止 VXL 缓存 | [CPU/GPU 场景对齐](2026-10-30-cpu-gpu-scene-parity.md) | ddf0002 native Vulkan VPL revision object coverage |
| HVA 多帧、独立碎片／程序化效果与队形公共画布 | [动画与队形 Hotfix15](2026-10-30-animation-effects-formation-hotfix15.md) | HVA frames VoxelAnim rawVXL quaternion LaserDraw Wave AlphaShape points ViewportCanvas |
| 本地 UI 独立绘制、当前原生字体与 Vera20k 对照 | [独立本地 UI Hotfix14](2026-10-30-independent-local-ui-hotfix14.md) | GAME.FNT CSF primary rally font cache worker no broad UI cutouts |
| 共享缩放前素材、五类界面与粒子／本地 UI 边界 | [共享场景与本地 UI Hotfix13](2026-10-30-shared-scenes-local-ui-hotfix13.md) | SceneBank NativeImageBank particles defaults messages timers primary rally |
| 持续规划后台线程、全局磁电索引、固定名额与动画类别审计 | [规划线程与动画 Hotfix12](2026-10-30-planner-thread-animation-audit-hotfix12.md) | UI ValueTask receipts expiry VXL256 selection512 EBolt128 SHP translucency x87 cooperative frozen keys |
| 迷雾素材替换与单位统计、副相机及进程识别性能 | [素材与工具箱性能 Hotfix11](2026-10-30-shroud-material-and-toolbox-performance-hotfix11.md) | SHP GetData 69E740 refs lazy alpha127 restoration async statistics VPL cache |
| 批量读取、行并行、GPU 基础档、耗时解码预算及迷雾替换验证 | [读取/GPU/动画 Hotfix10](2026-10-30-reader-gpu-budget-hotfix10.md) | prefetch64KiB rows4 atlas CSR decode budget transient VPL SHP127 replacement |
| 只开启渲染升级时的默认重处理、基础快速模式及高速游戏时效 | [快速模式与高速采样 Hotfix9](2026-10-30-fast-mode-and-high-speed.md) | 72ms 260ms RenderProcessing xBRZ explicit terrain thread wallclock actor age |
| 独立采样与整帧绘制时钟、小地图全图源数据及 TMP 解码复用 | [独立整帧时钟 Hotfix8](2026-10-30-independent-full-frame-clock.md) | Recurring 5-500Hz 500ms lease completion notify scene bank shared TMP force full upload |
| 设置页首次加载慢与切换空白、原子类别布局、输入绘制饥饿 | [设置页生命周期 Hotfix7](2026-10-29-settings-page-lifecycle.md) | latest category foreground lag relative z order shared tooltip 600 fair message pump |
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


| 共享场景与缩放前结果、五类 UI、粒子与本地消息／倒计时保留 | [Hotfix13](2026-10-30-shared-scenes-local-ui-hotfix13.md) | SceneBank SceneRenderer NativeImageBank Particle MessageList BitFont DrawTimer PrimaryFactory ArchiveTarget |

- [Hotfix16：基础路径与整图延迟调查及 CPU 修复](2026-10-30-render-latency-hotfix16.md)：原尺寸像素热点、128 项 xBRZ 缓存抖动、分页并行；CPU 积分跨帧缓存与不变对象页误标脏修复、定向检查和构建。

- [MO新操作預覽節奏、框消失與軌跡末端](2026-11-02-mo-operation-preview-followup.md)

- [原生建造自動匹配、地基預覽與點擊配對](2026-11-02-native-placement-preview.md)

- [科技圖示逐觀測幀補缺與早期版本身份](2026-11-02-tech-cameo-frame-maintenance.md)

- [小地圖即時遮罩、完整發布與最終尺寸地形快取](2026-11-02-minimap-live-mask-and-target-prefix.md)

- [逐幀、整畫面縮放、十三處側欄實驗與資源所有權](2026-11-02-experimental-view-controls.md)

- [新操作、小地圖與實驗顯示：本輪整合、效能與構建證據](2026-11-02-mo-render-followup.md)

- [原生與工具卸礦完成後接管最近礦格、玩家命令優先與輕量觀察](2026-11-03-native-unload-harvest-handoff.md)
- [側欄立即重畫、MO 十七原始 ABI 與 A/R 整合契約](2026-11-02-sidebar-redraw-and-mo-viewport-contract.md)（6D2790 RET14 WM_PAINT RenderSerial 4F4480 4F459D map-only；早期調查，後續實作見下文）
- [MO 原生地圖縮放：A/R、共用完整 Render、輸入／重連與世代退役](2026-11-03-mo-native-viewport.md)

- [固定容量扩容、科技全开生产预定与禁用超武](2026-11-04-fixed-capacity-production-superweapons.md)（Assist94；SW 原始指令 17 例；无真实游戏／联机验证）


- [小地圖頂部裁切、底部導航與真實視口](2026-11-04-minimap-camera-overview.md)（Tactical overview／native backing 身分／R 視口／排除端點；受控測試，實機待驗收）

- [Runtime(8) 單步異常、故障鎖存及同進程重連殘留](2026-11-04-runtime-single-step-fault-reconnect.md)（80000004／4F7870／fault未清／callback仍活；來源與實機日誌分析，未修改Runtime）


- [pre.8 Hook 異常傳遞、SEH 擁有者清理與重新連接](2026-11-05-runtime-exception-policy.md)（Search／access-only fallback／exact lease／publication abort／fault retirement；Windows 自有程序與產品編譯）

- [硬體斷點 owner 交會與偵錯例外分流](2026-11-05-breakpoint-owner-rendezvous.md)（pre.9；Windows DR／SEH、延遲交易與配對成品通過；實機首發來源待確認）

- [選中己方單位攻擊腳下與零射程目標](2026-11-05-attack-own-ground.md)（pre.9 整合；保留最新 Hook／例外政策，受控測試；實機發射與聯機待驗證）
- [小地圖、副相機、隊形預覽與渲染升級共用效能](2026-11-05-shared-render-performance.md)（pre.10；LiveINI 失敗重掃、共用光照、CPU prefix／批次失效、GPU adaptive bins；受控對照，非實機 FPS）


- [隐形全图重画、读取缓存淘汰及地形采样等待](2026-11-05-render-cost-fixes-pre11.md)（pre.11；CPU 局部依赖回放、epoch 旧页优先、compact traversal；受控对照，非实机 FPS）

- [读取预算扩展、CPU全量并行与GPU重绘/图集突增](2026-11-05-render-cost-expansion-pre12.md)（pre.12；256MiB/1GiB、GPU位移页闭包/坐标索引、前缀/图集/地形LRU；受控对照与Vulkan CI）

- [地形、SHP、VXL、动画与粒子混合场景持续渲染](2026-11-05-mixed-scene-render-stress.md)（固定生成素材；CPU 两档各300帧、五次全局失效；软件Vulkan视口／全图各120帧及三帧冷对照；Area子阶段计时未采集；非游戏FPS）

- [混合场景重复地形与GPU阶段引用](2026-11-05-mixed-scene-render-optimizations-pre13.md)（pre.13；CPU两档300帧中位改善／High尾延迟变差；65阶段CSR；验证专用软件等待额度下原始／筛选GPU对照）

- [CPU整图面积缩小和地形恢复](2026-11-05-cpu-area-reduction-pre14.md)（pre.14；两档完整/增量各300帧同机逐帧输出哈希一致；可选缩图表、地形列表复用与并行帧恢复；非游戏/物理GPU FPS）

- [CPU页内重绘、缩图缓存与多视图线程共享](2026-11-05-cpu-local-damage-and-row-team.md)（1.0.42-pre.1；最终CPU两档300帧、双视图／增量像素对照；392四平面检查；纠正GPU直出启用状态，非游戏FPS）

- [设置保存误报权限、重复旧键删除与运行中重试](2026-11-07-settings-save-recovery.md)（1.0.43-pre.13；实际 Store/生产保存轨迹反例，真实错误来源与失败意图保留；非用户 Windows 文件系统实测）

- [1.0.44：Broker 非阻塞生命周期、建造预定固定锚点/世界意图与 SHA/SHP 禁用](2026-11-08-release-144-broker-reservation-assets.md)（Host/UI 修复与受控前后对照；实机未验收）
