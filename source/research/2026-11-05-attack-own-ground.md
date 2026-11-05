# 選中單位攻擊腳下：零射程目標與原生事件批次

使用者要求原版／尤里復仇的化學兵等零射程單位能攻擊腳下，並提供快捷鍵。基線是 `codex/hook-recovery-20261105@c27f86885f119bc9c61583fd154562a8660fbd6a`，包含 pre.7 共享提示框清理及 pre.8 硬體斷點 rendezvous／debug 例外傳遞修正。本輪標記 pre.9；不改現有 Shared 結構、Assist96／Cursor155／Runtime4 ABI，不新增 Hook 或硬體槽，不寫武器射程、冷卻、彈藥、規則或遊戲程式碼。

調查前查過 INDEX、Runtime(9)、既有火控／原生事件筆記與持久 SQLite v130。使用者之前提供的遊戲程式只供本機讀取，不提交 EXE、轉儲或原始反組譯。

## 樣本與原生判定

| 樣本 | SHA-256 |
| --- | --- |
| 樣本3 | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| 樣本4 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

依原始 PE 的受控 x86 執行及反組譯：

- `Techno::IsCloseEnough=6F77B0`，thiscall，target／weapon index 兩參數，ret8；下層 `6F7220` 讀 `Weapon+ B4/B8`，執行原生距離與最小射程比較。
- 一般零射程武器的起點是單位的精確 subcell 座標。單位不在格子正中心時，當前 CellClass 的中心座標和單位座標不同；Range0 對這個地格判定為超距，而以單位自身物件為 target 則距離為0。
- `Weapon+134` 的 AreaFire 路徑會把起點切到腳下地格中心。這時應使用 Cell target，不能一律把所有零射程武器都改成 self target。
- 完整共用 `GetFireError=6FC0B0..6FCD35` 在合成就緒、地面彈體、有效彈頭 Verses 的單位上：零射程 self target 返回0；非中心地格返回8；保留再裝填時間返回3；彈藥0返回1。`6FCCEE..6FCD04` 的第三個參數為1時會執行範圍檢查；不能只依上游參數名稱推斷其極性。

每份 PE 12項：self／cell、非中心／中心、高地、MinimumRange、一般射程、完整共用開火判定、冷卻、無彈藥，以及 AreaFire 的相反目標選擇。兩份共24項通過。

這些檢查執行原生距離及完整共用 GetFireError；座標、武器 getter、可見性／地圖與障礙查詢使用合成替身。沒有執行完整 Infantry::Mission_Attack、所有 Infantry 特例、彈體產生／核彈落地或實機／聯機。因此結論是目標能通過已測原生判定，不能把它稱為所有武器在實機必然開火。

## 功能與生命週期

控制頁「攻擊腳下」按鈕與快捷鍵；預設 Ctrl+Alt+F，沿用既有組合／雙擊設定，INI `[Hotkeys] AttackOwnGround`。既有三項模式的 command ID 不變；新增 command34，UI9176／9177，hotkey1727。設定了這個快捷鍵會要求正常命令連接；取消綁定後仍可由按鈕啟動連接，再次點擊執行。

Host 不把此命令放入一般延遲隊列；過期、忙碌、未連接或選取通道正忙時拒絕本次輸入。WM_HOTKEY 舊通知及延遲手勢到期不能轉成新的攻擊；混合快捷鍵需改成獨立按鍵。Web 按鈕沿用 Immediate150ms 檢查；Runtime 初始請求也要求150ms內。

Runtime 在原命令執行入口固定當時選中的己方存活步兵／戰車；略過載具內、被操控／扭曲、Sticky 或無武器對象。普通武器對各自當前格發 Attack=1、Cell tag11；首個有效武器 Range0、MinimumRange0 且非 AreaFire 時，對原單位 UID 發 Object tag34。引擎仍自行選武器並做實際開火判斷；不直接呼叫 Fire，不強制冷卻歸零，也不改 ROF。

最大8192個來源儲存在 static job，避免大型聚合暫存把遊戲執行緒棧撐爆。全組沿用共同每幀事件預算、隊列保留空間、原構造器、SP DoList／MP OutList、原所属方及回執。每個單位一次下令；多幀只延續尚未提交的來源，不重複已提交項。後續改選不會替換來源；發令前重新查 UID／指針／所属方、存活狀態與 manual token，新的手動操作優先。

對局變更、重置、失去可執行狀態、聊天、停止、fault、請求換代或3500ms期限會結束未完成部分。只在最後發布一個最終計數，不自動重試已失效的選區；延續不重播指令語音。沒有新增 direct field／code patch 或繞過原生事件接收檢查。

## 驗證與交付邊界

- `assist/tests/attack_ground_native.py`：兩份私有 PE 的24項原生受控檢查；輸出只含 SHA、結果及範圍，不含原始指令。
- `assist/tests/attack_ground_policy.py`：抽取真實 production 批次及快捷鍵函式編譯執行，27項通過。含60單位分三幀、換選區、manual token／死亡／UID變更、超時、換對局、Sticky、失敗、聊天／暫停、request不匹配、過期／衝突快捷鍵及控制頁分類。
- 證據為 [原生樣本3](evidence/attack-ground-native-range.json)、[原生樣本4](evidence/attack-ground-native-range-cncnet.json)、[批次／輸入](evidence/attack-ground-policy.json)。Windows 配對編譯另由專用 workflow 驗證，不把編譯或合成 IO 測試當成遊戲內／聯機驗收。

待實機確認：WEEDGUY 實際武器選擇與完整攻擊／核彈行為、平台覆寫的開火函式，以及真實雙機同步。沒有宣稱修正所有零射程 MOD 武器或允許原本受特殊開火條件禁止的武器。
