# MOControls2 新功能合入與本分支策略保留

來源為 [adaae2b3f4a880d1a5f0e9b87f905b69f80b1329](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/adaae2b3f4a880d1a5f0e9b87f905b69f80b1329)，包含其前置 5cd90d7dde2f8de4dd1d08e0665ae8be18d05bd6。兩條線的共同基線是 17ee48810d33d47203d52893d3b0472d992f3a11，本地工作基線是 f05d3e1131843f0d38e0333ab081c9c42ac6f32d。

使用者已明確選擇「合入新功能，保留本分支已確認的 Hook 與光標修正」。因此逐段移植新增操作與依賴，不把整個舊分支檔案覆蓋過來，也不導入 incoming 的全局禁止程式碼／虛表寫入、Runtime5、Entry4 或舊 Assist85 佈局。現有控制、原生建造／科技、光標提示、渲染和切屏生命週期修正保留。

## 具體整合

- 模糊下令、區域目標分配、自訂巡邏和快速占位共用既有按住鍵／滑鼠配置、自動保存、Host/Runtime 手勢解析與命令仲裁。快捷鍵變更、衝突、前台/PID 切換會取消過期捕獲，避免 Host 輪詢與 Runtime 同次重複發令。
- 原生巡邏用既有 MegaMission、PlanConnect、PlanCommit 構造與 SP DoList／MP OutList 通道。新增 PlanConnect 的產品准入限於當前受驗證的巡邏狀態，構造前、入隊前及原生時間更新後再次核對，不是放開所有 ProbeOnly 事件。
- 快速隊形共用原隊形開關、單位手動版本與任務仲裁。新配置收入原九組摺疊中的隊形組；沒有把既有面板收納撤回。
- 獨立超武面板走外部只讀模型與既有 SelectSuper 命令，不接管原生側欄。按下、放開與 Runtime 執行時保留並核對 index/instance/type/House；有效空清單清快取，短暫失讀可以保留已完成圖像但不授權點擊。SHP／色盤變更會刷新，素材失敗以文字回退，取消超武不清除無關建築目標光標。
- 原生視野縮放／側欄／暫停仍禁用，保存的相關快捷鍵不註冊執行。未導入它們的新補丁。

通訊升為 Assist88，Shared=6584688；原 Protocol87 尾前所有欄位保留順序，只追加 MO 狀態、預覽和手勢。Runtime4、Entry3、Cursor155 保持。735 項實際 Windows x86/x64 大小、對齊與 offset 一致；Python 原生執行 harness 的 269 個 Shared offsets 與 C++ 報告逐項比對，拒絕用舊佈局測新 DLL。

操作入口及預設按鍵見 [MO-CONTROLS.md](../../docs/MO-CONTROLS.md)。原始行為參考 MagicShiba/MO_ExperienceEnhancements@33ed558bd38f36b5012eddcc9c0fb109cc68e38a；本輪適配保留作者註記和 THIRD_PARTY_NOTICES，未導入其工程／二進位。

## 實際 Runtime 消息入口

最後整合發現 GameRuntimeHook 只把左／右鍵交給 AssistRuntimeFilter，中鍵及側鍵 down/up/double-click 未在白名單中。已補齊六種消息，保持 Cursor→Tactical→Filter 的服務順序。編譯後 DLL 的 runtime_gesture_dispatch.py 執行31項邊界檢查；舊 DLL 穩定重現中／側鍵及其服務尾段10個失敗，新 DLL 全部通過。此測試另核按鍵、既有左右鍵、未啟用 Tactical、PM_NOREMOVE、負 hook code、null MSG 和不相關消息。

## 合併中修正的巡邏問題

原提交只看 node count 增長及 loopLength>0，未證明收到的是本次建立的路徑。現在以 checked read 區分無圖與未知狀態；選中單位已有非空圖則不追加。驗證 token 反向關聯、完整節點向量、每節點 actor/event、原生任務種類與 destination/nearest cell，重讀整組資料，並把完成閉環綁到同一 token 與節點身份。

自動 House-wide Commit 前檢查其他本方 pending 圖，避免提交不屬於本次操作的編輯。原生規劃按鍵由真實熱鍵表解析；其原始釋放消息若需要延後，保留發送責任。暫時失讀或 PostMessage 失敗不冒充狀態換代，不把未知 planning flag 當 false；身份確定更換才丟棄過期消息。取消只停止未提交尾部，原生已接受部分使用獨立狀態提示；不製造補償 Commit、不直接刪原生節點。

### 原生機器碼對照

調查前查閱研究索引、pre.10 的原生規劃收發、2026-10-26 network-event audit、Hotfix22 native planning 與既有 native_planning.h。Library 當前不可用；新結論保留於本檔供後續寫回。私有樣本 SHA-256 為 247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0，探針前後一致，未發布原始 PE 或 dump。

隔離 x86 探針執行原版 MegaMission 構造器 0x4C6860、Waypoints 構造器 0x4C6780、NoOptions 構造器 0x4C66C0 和接收器 0x4C6CB0。三種構造器以 ECX 指向事件、原生 callee cleanup 執行。物件/ID resolver、heap、owner/audio 和 0x639D13 的無參數 waypoint eligibility 查詢使用合成替身；原生 graph 構建、Connect 和 Commit 本體執行，沒有改寫遊戲程序碼。

四個 cell Move 節點的實際結果：

| 階段 | pending +8C | loopLength +90 | loopStart +94 | 節點 mission +12 |
| --- | --- | --- | --- | --- |
| append 後 | 0 | -1 | -1 | 2,2,2,2 |
| Connect 首節點 | 0 | 4 | 0 | 29,29,29,29 |
| Connect 第二節點（對照） | 0 | 3 | 1 | 2,29,29,29 |
| Commit 後 | -1 | 保持 | 保持 | 保持 |

每節點 Event+7 actor ID、+11 tag34、+14 target0/+18 tag0、+19 destination/+23 tag0B、+24 nearest/+28 tag0B 都與原送入資料一致。這證實「只要有閉環」不足，並糾正本輪初版過嚴判定：閉環後 mission 應為29，不能還要求2。正式核對要求未提交 pending0，開放圖 mission2；首節點閉環則精確 loopStart0、length等於路徑數、mission29。此差異由原生執行發現，不以模擬預期自證。

可重現探針：assist/tests/mo_native_planning_runtime.py，兩個 --cells --connect-index 0/1 案例；最終報告收錄在 [完整構建證據](evidence/render-ui-followup-build.json)。它不涵蓋實際尋路、移動到達、遠端網路或平台檢查。

## 驗證與邊界

實際 Host config/dispatch/IPC/focus/preview/category 71項、cold demand130項、九摺疊2235項各在x86/x64通過；手勢仲裁亦窮舉驗證。超武模型與實際 Panel::proc/update、SHP decoder、身分替換、失讀、覆蓋層隱藏、cache及取消測試覆蓋兩架構。巡邏圖28項各在x86/x64通過；實際MO bridge42項、既有AirPathMove87項、快速隊形及key-up恢復回歸通過，涵蓋未完整路徑、已提交／錯誤閉環及舊功能准入。

最終完整產品／診斷構建、實際 DLL Runtime冷啟用9項／控制9項、Cursor文字與佈局、兩項原生planning探針、31項消息入口、原有Runtime事件與manager回歸、PE／跨位元通訊及診斷版實際loader均通過，結果及雜湊見完整構建證據。合併前的成功構建另行保留，不能代替合併後成品。沒有啟動或操控使用者遊戲，也沒有聲稱本輪完成 RA2OL 雙機或硬體GPU現場驗收。
