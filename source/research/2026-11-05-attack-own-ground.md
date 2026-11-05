# 選中單位攻擊腳下：零射程目標與原生事件批次

使用者要求原版／尤里復仇的化學兵等零射程單位能攻擊腳下，並提供快捷鍵。基線是 `codex/hook-recovery-20261105@c27f86885f119bc9c61583fd154562a8660fbd6a`，包含 pre.7 共享提示框清理及 pre.8 硬體斷點 rendezvous／debug 例外傳遞修正。後續合入完整 Hook pre.9 `3be939057d72bfa3e4b3bb0a55b593412d3423be`（例外所有權、SEH 清理及 rendezvous），本輪標記 pre.10；不改現有 Shared 結構、Assist96／Cursor155／Runtime4 ABI，不新增 Hook 或硬體槽，不寫武器射程、冷卻、彈藥、規則或遊戲程式碼。

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

初版每份 PE 12項：self／cell、非中心／中心、高地、MinimumRange、一般射程、完整共用開火判定、冷卻、無彈藥，以及 AreaFire 的相反目標選擇。兩份共24項通過。

新增反證及實際發射檢查：

- `Techno::SetTarget=6FCDB0` thiscall target/ret4；`6FCEB8..6FCEE9` 在 target==this 時轉成當前 Cell，`6FCF3E` 儲存到 `this+2B4`。所以「Object34 self 事件能解決 Range0」假說不成立，初版事件方案已撤回。受控檢查只執行至儲存後 `6FCF44`；樣本4 的 `6FCF53` 有平台跳板，沒有假造執行其缺少的執行期內容。
- 單機直接呼叫步兵原生 `GetFireError=51C8B0`／`Fire=51DF60`，戰車 `740FD0`／`741340`，thiscall；self target 不經 SetTarget，因此仍是精確零距離。GetFireError 的範圍參數保持1；沒有忽略範圍、最小射程、冷卻、彈藥、朝向或特殊限制。戰車 Fire 內還會再次檢查 GetFireError。
- 兩份樣本各自執行完整步兵／戰車 Fire，進入共用 `6FDD50`，到達原生彈體建立請求；原生 `6FF2A4..6FF2BB` 寫入 RearmTimer，`70D670` 扣減 Ammo，第二次 GetFireError 返回3，彈藥只扣一次。兩種 override 的返回棧平衡已檢查。合成步兵就緒0、冷卻3、無彈藥1，戰車未對準返回2；這些限制沒有被放寬。

最終每份 PE 20項，共40項。地圖、可見性、座標／武器／炮口 getter、locomotor COM 及彈體配置／啟動仍是合成 IO；執行的是原始判定、Fire 控制流程與冷卻／扣彈，不是彈體物理、完整實機攻擊動畫、音效裝置或聯機驗收。

## 功能與生命週期

控制頁「攻擊腳下」按鈕與快捷鍵；預設 Ctrl+Alt+F，沿用既有組合／雙擊設定，INI `[Hotkeys] AttackOwnGround`。既有三項模式的 command ID 不變；新增 command34，UI9176／9177，hotkey1727。設定了這個快捷鍵會要求正常命令連接；取消綁定後仍可由按鈕啟動連接，再次點擊執行。

Host 不把此命令放入一般延遲隊列；過期、忙碌、未連接或選取通道正忙時拒絕本次輸入。WM_HOTKEY 舊通知及延遲手勢到期不能轉成新的攻擊；混合快捷鍵需改成獨立按鍵。Web 按鈕沿用 Immediate150ms 檢查；Runtime 初始請求也要求150ms內。

Runtime 在原命令執行入口固定當時選中的己方存活步兵／戰車；略過載具內、被操控／扭曲、Sticky 或無武器對象。普通武器對各自當前格發 Attack=1、Cell tag11；首個有效武器 Range0、MinimumRange0 且非 AreaFire 時，僅單機嘗試一次上述原生 Fire(self)。聯機跳過此補充開火，回執明示未執行，不製造只在本機生效的傷害。AreaFire 繼續使用地格事件。零射程在發射前重新檢查武器欄位、原地格、原生 vtable 及入口簽名；未知或被改寫的入口只拒絕這個功能，不使 Hook 全局 fault。冷卻／無彈藥等不能開火的對象立即略過，不保留等待冷卻後開火的請求。

最大8192個來源儲存在 static job，避免大型聚合暫存把遊戲執行緒棧撐爆。全組共用每幀24次操作預算。一般地格事件沿用隊列保留空間、原構造器、SP DoList／MP OutList、原所属方及回執；單機補充發射不產生網路事件。每個單位一次下令；多幀只延續尚未提交的來源，不重複已提交項。零射程回應語音在 Fire 前播放，避免 Suicide／瞬發武器銷毀自身後再次引用來源。後續改選不會替換來源；發令前重新查 UID／指針／所属方、存活狀態與 manual token，新的手動操作優先。

對局變更、重置、失去可執行狀態、聊天、停止、fault、請求換代或3500ms期限會結束未完成部分。只在最後發布一個最終計數，不自動重試已失效的選區；延續不重播指令語音。沒有新增 direct field／code patch 或繞過原生事件接收檢查。

## 驗證與交付邊界

- `assist/tests/attack_ground_native.py`：兩份私有 PE 的40項原生受控檢查；輸出只含 SHA、結果及範圍，不含原始指令。
- `assist/tests/attack_ground_policy.py`：抽取真實 production 批次及快捷鍵函式編譯執行，37項通過。含60單位分三幀、換選區、manual token／死亡／UID變更、超時、換對局、Sticky、失敗、聊天／暫停、request不匹配、過期／衝突快捷鍵、控制頁分類，以及 SP／MP、冷卻略過、武器／地格改變和零射程分幀預算、空選取指針和瞬發後來源記憶體失效。
- `assist/tests/attack_ground_native_policy.cpp`：production 原生轉接函式31項，驗證 SP／入口所有權／簽名拒絕、原生錯誤碼、self 參數、語音在 Suicide 開火前、粒子回傳空值及無重試。只有合成 IO，不當成實機證明。
- 證據為 [原生樣本3](evidence/attack-ground-native-range.json)、[原生樣本4](evidence/attack-ground-native-range-cncnet.json)、[批次／輸入](evidence/attack-ground-policy.json)。Windows 配對編譯另由專用 workflow 驗證，不把編譯或合成 IO 測試當成遊戲內／聯機驗收。

待實機確認：WEEDGUY 實際武器選擇與完整攻擊／核彈行為、Suicide／粒子／特殊 MOD 的完整效果、平台覆寫的開火函式，以及真實雙機同步。沒有宣稱修正所有零射程 MOD 武器或允許原本受特殊開火條件禁止的武器。


## 配對構建完成

`276235401d3d31ed0e27c639f718609a6a9f3c55` 的 Windows workflow `41941472508` 成功；Windows 37項 production 輸入／批次檢查、完整 x64 Host／x86 Runtime／x86 Broker 構建及既有輸出驗證全部通過。下載 artifact ZIP 的 SHA-256 與 GitHub 摘要一致，四個交付檔案逐一核對編譯清單的雜湊、架構，三個自家 PE 檔案版本均為1.0.41.10，Assist96／Cursor155／Runtime4 配對。

產品更新包只含四個執行檔、WebView2 授權及使用說明；不包含開發清單、原始碼、私有 EXE 或測試紀錄。詳見 [構建及更新包摘要](evidence/attack-ground-windows-build.json)。編譯成功不代表完整實機或聯機驗收。
