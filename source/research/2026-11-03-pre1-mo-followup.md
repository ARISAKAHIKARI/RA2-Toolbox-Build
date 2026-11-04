# 1.0.41-pre.1 接續及 MO 操作補齊

## 範圍和依據

本輪以使用者指定的 `1.0.41-pre.1` 繼續：`codex/console-architecture-20261103` / `4b6bb9cc59658876eb1e3a10835c85298036371b`。它是分支版本，並無對應正式 release/tag。35 檔上游變動經 blob 核驗及三方合併，保留本地已完成的放置、採礦、光標與渲染修正；不把未 materialize 的遠端歷史檔案當作刪除。

參照 `MagicShiba/MO_ExperienceEnhancements@80850c7acf7d43d112975583b4fee5037c800263` 的 ScaleBar、FullMap、SmartArea、Patrol、AirWay 與操作設定。持久 Library 筆記本本輪不可用，依 AGENTS 規定保留此研究與索引待回寫。操作及入口見 [MO-CONTROLS](../../docs/MO-CONTROLS.md)。科技加強和控制臺分別有獨立研究，不能以「沒寫 .text」推論聯機安全。

支援私有 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。只讀 EXE 與隔離受控執行，未操作遊戲進程，未做 RA2OL／雙機、真實渲染或 FPS 驗收。

## 地圖縮放和側欄

- ScaleBar 的 A/R 分離已接入原生 Render `0x4F4480`、場景/輸入及 Host 投影，保持實體 UI 尺寸。不是每次調整整個顯示器解析度。
- 倍率為 MAX、50、66.7、75、80、100、125、150、200、300、400%。66.7 檔使用精確 `3/2` 邏輯尺寸。MAX 由活動地圖尺寸求適配比例，受每軸 8192 與 4000 萬像素預算限制。獨立審查發現四捨五入能跨過總像素上限；現在僅對 MAX 的這種情況向下取整，不把正常地圖誤判不支援。`view_zoom_modes.py` 保留反例。
- F11 循環完整、只保留雷達、全隱藏。原生雷達區域沿用原生寬 `0x7F5BF8`、高 `0x886F94`、左右側 `0xA8EB7C`；Host 遮罩雙讀顯示尺寸 `0x8A00A4/A8` 及 mode/status/busy，跨代則暫停輸出。遮罩必須在 grace 重用分支前更新，CPU 與 GPU 重撈節點兩處均適用。新模式對原生 widget 維持實體座標，地圖放置不穿透雷達。
- MO 小地圖分軸固定 `Clamp_Center@0x6D8640` 除獨立開關外，亦在縮放 Render 仍擁有資源時作為依賴保留。退出若 native Wnd 尚忙、Capture 已拒絕而 Render 尚未退休，不得先撤相機夾限；改用純計算的冪等 clamp。下一輪 reset 確認 Render/Wnd 退休後才還原。冷停止不新裝 Hook；foreign phase 持有時不釋放仍被引用資源、不假報 stop=2。

原錯誤在舊中間 DLL 重現，修正 DLL 受控測試通過。永久測試：`native_viewport_runtime.py`、`native_viewport_retirement.py`、`small_map_camera_axes.py`、`radar_only_overlay.py`，以及原有 Host/drawings/scene、SEH、原生座標測試。受控原生圖形物件為模型，不是視覺驗收。

## 下令、隊形及巡邏

七項參數隨手勢政策同一 sequence 捕獲：Range/Squad/PatrolSize/NodeStep/LoopClose/LoopStep/AutoCommit，預設 `4/4/4/2/3/3/1`。區域寬度不是半徑，奇數兩側按 MO 原義不對稱；修改中取消尚未發出的舊尾段。預設幾何和舊版逐點比對，避免增加選項卻改變舊操作。

SmartArea 增補兩條實際缺口：Select/Move 可進入本方相容吸收建築；InfantryAbsorb 依真實剩餘容量公平分配。預覽及最終入隊共享 pending 容量保留，遊戲 timing 回來後再核實；僅此 Enter7 容量路徑強制事件建構傳輸，避免 MP 高階入口略過最後檢查。普通命令傳輸不變。測試含舊版三個反例，沒有新增 Hook。

快速占位已有隊形快捷預覽；自訂巡邏圈、原生規劃節點、飛機路徑點兼容／原生模式、可拆超武面板、單機暫停逐幀已存在，本輪核對入口並補齊文件。沒有把 MO 特定 Ares 偏移盲搬到任意版本，也沒有把兼容模式描述為原生 vtable 模式。

## 協議及驗證

配對 Assist92，Shared `6584992` bytes；x64 WebView2 Host + x86 Runtime/broker。WebView2 SDK 固定 `1.0.4314.35`，隨包固定 x64 Loader 與官方授權，Evergreen Runtime 仍是系統依賴。

最終完整編譯與驗證結果、來源/產物雜湊、命令退出碼及限制由 [pre1-mo-followup-build.json](evidence/pre1-mo-followup-build.json) 綁定。歷史 Assist91 與中間 DLL 報告保持原意，不重新標成此最終產物的實測。
