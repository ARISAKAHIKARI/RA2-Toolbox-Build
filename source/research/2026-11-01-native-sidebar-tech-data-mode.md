# 科技全開：原生面板規則資料模式

本頁接續 [不改程式碼的兩條候選路線](2026-11-01-tech-no-code-feasibility.md)，更新其中「尚未實作」的研究狀態。按照使用者最後要求，實作的是 `techImplementation=3`「原生面板（規則資料）」：在遊戲原生建造欄新增可用圖示，由原生 UI 生產；未發布的工具箱獨立 Produce 清單方案已移除。原有受控、經典、Master 三種模式保留。

此模式自身不改遊戲指令或 vtable，**仍標示高危**：它改寫所有玩家及 AI 共用的 Type 規則資料；無程式碼補丁不等於聯機同步安全。沒有雙機 RA2OL 驗證，不能據此保證不會同步錯誤。其他獨立功能、既有幀通知／時鐘選擇仍各按使用者設定運作，本模式不藉它們新增科技補丁。

## 實際範圍

僅處理既有 `scanTypes` 目錄中使用者選取的 `techAllowed` 類型，不擴展隱藏飛彈、裝飾等目錄外物件。對每個經 Type array/index、vtable、UID、ID 核實的類型，記錄並修改以下九欄位：

| 欄位 | Type 位移 | 暫時值 |
| --- | --- | --- |
| TechLevel | `0x634` | 0 |
| Prerequisite.Count | `0x648` | 0；保留原向量指標及容量 |
| OwnerFlags | `0x6CC` | `0xFFFFFFFF` |
| RequiredHouses | `0xDA0` | `0xFFFFFFFF` |
| ForbiddenHouses | `0xDA4` | 0 |
| Unbuildable | `0xC98` | 0，單位元組 |
| 三種 RequiresStolen | `0xD9B..0xD9D` | 0，各單位元組 |

保留 BuildLimit、價格、真正擁有的工廠、海陸相容及超武關聯／對局設定。全目錄在一次刷新中處理，只有完整原生 `CanBuild(type,false,true)` 通過且原生 `FindFactory` 找到合規工廠，才呼叫原生 AddCameo；不分段偷偷降低範圍。忙碌或停電工廠仍可擁有圖示，原生點擊及生產流程繼續處理電力、金錢、限額和佇列。這不是保證每一種 Type 都能被放行的無條件解鎖。

Mode 3 拒絕工具箱 `Produce`／`CancelTechProduction`，不退回獨立事件清單。進入時清除舊工具箱生產對應的 `reservedSidebar` 觀察記錄；單純啟用本模式不會安裝預定生產維護／裁剪補丁，也不會因沒有 cameo 自動發 Abandon。其他獨立功能仍保留自身開關。

## 切換與還原

先完整撤掉原有兩個科技 CALL 補丁或五個 Master 科技站點；registry active/conflict、實際原始位元組任何一項不符，都不宣告 mode 3 就緒。實際成功狀態是 `Snapshot.techOpen=4`，不是主機下拉選項的預期值。

舊模式有依賴其特殊科技權限的生產未完成時，切入資料模式回傳 `-43`，保留舊模式、舊 allowlist 和原補丁；資料模式有依賴其改寫 Type 的生產時，切回其他模式也等待。完成／取消後清除等待錯誤，由主機重試目前選擇，沒有隱式取消產品。使用者明確關閉科技功能則恢復資料；此時遊戲可能自行取消已不再合法的產品，UI 有提示。

資料逐欄位寫入前先保留所有權，寫後讀回；任何中途失敗還原整批已寫欄位，不先增添圖示。只還原仍等於本功能寫入值的欄位，第三方值不覆蓋；已回到原值可解除責任。短失讀保留 journal，重用 Type 或真實世界退役不向舊物件回寫。無法還原時回報 `-44`，停用／reset 也保留這個回報與 cleanup pending，不會因畫面顯示關閉就承認已完成卸載。

`TechEnhance` 也是 Type+0x634 寫入者，兩者不得把彼此結果誤當原值。Mode 3 先完整還原 TechEnhance，再捕捉原值；資料模式活躍時暫停 TechEnhance，但保留使用者勾選。資料完全還原後原選擇恢復運作。還原衝突時不交接所有權。

## 驗證與界線

| 驗證 | 結果與證據 |
| --- | --- |
| 真實 portable recovery policy，合成記憶體 | [144 項 PASS](evidence/technology-rule-data-recovery.json)：九欄位 pre/post write failure、短失讀、重用、外部寫入、向量與保留欄位 |
| 抽取正式 sidebar／dispatch／patch lifecycle 函式 | [38 項 PASS](evidence/technology-rule-data-mode.json)：mode 3 與全部舊模式切換、補丁還原失敗、恢復重試、完整圖示閘門及 production 等待 |
| 抽取正式資料 bridge | [21 項 PASS](evidence/technology-rule-data-world.json)：world／House／epoch、allowlist、原值、TechEnhance 交接及全批回滾 |
| 支援 PE 原始 CanBuild／FindFactory 指令 | [38 項 PASS](evidence/technology-rule-data-native-predicates.json)：含實際九欄位方案對 UnitType／BuildingType 的改寫前、改寫後、還原後；超武限制維持、程式碼 hash 不變 |
| 最終 compiled Runtime 與 EntryManager | [9 組 PASS](evidence/technology-rule-data-runtime.json)：SP／MP 合成 session 冷啟用、全部舊模式切換、圖示 gate、-43、-44/stop 真正恢復、切屏／暫停、Enhance 交接、部分失敗重試 |

編譯 DLL 的科技資料模式測試中，遊戲所有 executable sections 的 hash 均與該例啟用前一致，EntryManager 安裝數為 0；舊模式仍分別是 2／2／5 個科技站點。最終 DLL SHA-256：`59b5e315a43f8a13e8fc5366f8c8ab7819855589e23c75f4a1cacda67649115a`，測試前後相同；交付組合資訊由 [完整建置證據](evidence/native-event-modes-build.json) 綁定。

Compiled fixture 執行實際 Runtime／manager，Win32、合成世界與 UI/native query callbacks 有界替代；原生謂詞本體另由支援 PE 測例驗證。原生生產進度、實際側欄操作、MOD 覆寫及雙機同步尚未端到端測試。第一輪 compiled fixture 以 unsigned ctypes 直接比較負錯碼而誤報，修正測試的有符號解讀後 9 組通過，未因此更改產品或放寬閘門。原始失敗記錄保留在工作目錄診斷。

支援私有 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。公開證據只含合成資料、結果、位址及 hash，不附私有遊戲檔或原始反彙編 dump。
