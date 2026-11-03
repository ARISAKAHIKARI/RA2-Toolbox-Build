# 原生卸礦完成後接管最近礦格

問題不是重複發卸礦命令。既有 `external_planner.h` 會把原生已返場／正在卸礦的車加入卸礦 job，但貨艙歸零後只累計 `oreDelivered` 並刪除 job，沒有後續採礦階段；因此原生先開始卸礦時，即使工具箱沒有再發 Enter，倒完仍走原生尋礦。

本輪加入 `ore_harvest_bridge.h` 與純幾何 `ore_harvest_search.h`。兩種來源共用同一份實際卸礦證據，不以工具箱曾發 Enter 或事件入隊作為完成證明。每個卸礦 episode 保存 Unit pointer/UniqueID、Type pointer/UniqueID、原本 manual token、原煉礦廠 pointer/UniqueID/type、House pointer/UniqueID、Scenario、Session、match 與 frame。必須觀察到原生 `Unit+6D1 Unloading=true`，且找到相容、同 House 的原生煉礦廠。之後貨艙 `+33C` 四個 float 歸零，Unloading 降下、Unit 存活／OnMap／非 Limbo 且 radio 已釋放，才準備採礦。partial read、貨艙未知、只離開工廠或只有完成回執都不算成功。

原生與工具開始卸礦的差別不再影響後續接管；EXE／Runtime 的卸礦排序計算選擇也不改這份 game-thread 觀察。它不新增 Hook、不改程式碼段／規則／工廠／任務欄位、不呼叫原生尋礦或路徑函式。唯一模擬效果是經現有 `admitPlannerOrder → nativeMission` 提交普通 Harvest 事件（`OreHarvestOrder=7` 僅新增 purpose 枚舉，IPC 結構尺寸不變），共用玩家身份、事件預算、actor arbiter 與 SP/MP 事件入隊。

後來的玩家命令優先。manual token 改變會丟棄舊 episode；即使玩家明確點 Harvest/Return，既有輸入路由會 `resumeOre`，仍不能把新的 token 收編進尚未結束的舊卸礦 episode。Unit inventory 因其他單位出生／死亡而重建時保留該 episode token。原生卸礦退出分支 `73E0E3`、`73E201` 只在 queued=-1/Harvest10 時自動續採；本輪同樣保留其餘 queued 任務。一般 Move/Attack、Sticky、EMP／載入、死亡、Owner/Type/ID／世界替換或 rewind 均不接管。成功入隊後只移除本地觀察，不重播或補發；不知道完成便保留原生行為。

## 最近礦格與原生參數

支持的私有映像 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。

- 原生 Action::Harvest=6 的 switch arm 是 `4D7E8E`。該分支呼叫 Map::GetCell `5657A0` 後，透過 vtable `+378` 提交 `ClickedMission(10, nullptr, clickedCell, nullptr)`。所以新事件使用 mission10、空 target、cell destination/tag11、空 nearest；沒有套用 Move 的 nearest-accessible override。對原始指令的受控執行驗證了四個參數及棧收尾，座標查詢／最後 virtual event 邊界為明示替身。
- `485020 Cell::GetContainedTiberiumValue` 經 `5FDD20` 找註冊資源範圍，回傳 `Tiberium+ B8 Value × (Cell+11E OverlayData + 1)`；**OverlayData=0 仍有一份資源**。原始函式的 0/1/255 對照結果為 25/50/6400；無 overlay 與非 Tiberium 回 0。新實作純讀 `A83D80 OverlayTypes`、`B0F4E8 TiberiumTypes`、Image `+E0`／ArrayIndex `+294`／Frames `+E8/+EC`，不呼叫會記錄未知配置的原生 fallback。
- 輸出原點凍結在本次煉礦廠 dock：X+1／替代 dock X+2，Y+2。搜尋按平方歐氏距離排序，不按已開始向原生舊礦區移動的礦車位置排序。每一圈走完後，以未訪問圈的距離下界證明沒有更近候選；相同距離按編碼 cell ID 固定排序。
- 候選核對 cell-array pointer、XY、註冊 Tiberium overlay、原生 usable area、ground level/slope、建築／地面障礙，以及 `89EA40 LandType×SpeedType` 的正數速度係數。成功讀到 null cell 代表沒有格子／資源，直接跨過；短讀才停在原格等重讀。送出前再次驗證資源及身份。這是最近的靜態可通行資源候選，跨河／分離區域等實際連通路徑仍交由原生事件接收處理，未宣稱幾何距離證明路徑可達。

固定參考：[YRpp UnitClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/UnitClass.h)、[CellClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/CellClass.h)、[FootClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FootClass.h)、[Mission 定義](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GeneralDefinitions.h)。本機原始指令另確認 `73DFDA` 設 Unloading，`73E0D2`／`73E1F6` 清除，`73E13A`／`73E24D` queue Harvest10；不是只沿用欄位名稱猜測。

## 觀察成本與驗證

`oreHarvestObserve` 放在既有 dispatcher 的 cadence early-return 前，只讀 native Unit::Array `8B4108` 的指標陣列/hash；陣列／礦車配置改變立即重建 type 清單，未變時也最多 **30 個 native frame 或 500ms** 重驗一次 metadata。這是因為最後一個非礦車可在兩次觀察之間刪除，同一地址立即分配新 UID 礦車，陣列 pointer/count/hash 可能恰好全部未變；只靠 pointer hash 會永久漏接新礦車。定期重驗仍保留舊卸礦 episode token，不會重新收編玩家後來的採礦命令。既有礦車的 ID/type/owner/unload 每 native frame 觀察，剛看見卸礦才讀原生 Building/radio。它不建立 planningWorld、不採樣每座煉礦廠四圈等待格、不掃礦圖、不發事件；只有完成後 ready 的 `oreHarvestNeedsWork()` 讓完整 tick 到期。全局每幀最多訪問 2048 個搜尋節點，稀疏 job 可利用剩餘預算；候選讀失敗、資源空或送出失敗不造成無界忙迴圈。ready 超過 8 秒／不明觀察 15 秒退回原生；不取消原生已開始的新一輪採礦。

- `assist/tests/ore_harvest_handoff.py` 直接編譯真 production observer/search header，抽取完整 `externalOrderValid/admitPlannerOrder` 及 eligibility 函式，x86/x64 各 **68 個真 checks PASS**，原64項全部保留。明示 `-UNDEBUG`、`#error NDEBUG`、fail-fast assert。涵蓋原生卸礦無工具 Enter、兩種計算位置、零貨艙但仍在卸礦、radio 保護、原點優先、alternate dock、null 格、water/building/temporary unit、UID/Type/Owner/House/scenario/session/epoch/rewind、短讀、chat／預算、後來手動採礦、inventory 重建、queued5/12/16 保留，以及 100 格外的最近性。新增固定 pointer/count/hash 下的新 UID 礦車、30 幀／500ms 兩個界限，以及定期重建不能收編舊 episode 的後來 token。`--observer-header <frozen-header>` 可隔離重跑舊 header 反例；`6104154f...` 基線確實在新礦車 30 幀後仍 minerCount=0 而 exit3。測試的 nativeMission 邊界明示為事件 seam，不冒充原生已執行。
- 一個礦車的 100 幀 idle 只讀 1228 次，map cell 讀取 0。另在隔離 Win32 fixture 的私有 buffers 上，read seam **實際呼叫本進程 ReadProcessMemory**：129 礦車×100 幀，67909 次讀，本輪兩次結果 x86 約 210–218µs/frame、x64 約 135–143µs/frame。加入定期重驗前為 65200 次讀／約205與134µs；沒有改成每幀重新掃全部 Unit metadata。這只量該測試進程的觀察／RPM 開銷，不是遊戲 FPS 或真戰局效能。
- `assist/tests/ore_harvest_runtime.py` 載入共用 Shared ABI 描述並核對 DLL 匯出大小，執行真 compiled Runtime、原生 EventClass constructor 與隊列。中間 Assist91 的核心 3 cases PASS：AllRuntime+scanMs1000 卸礦中 worldSeq/computeFrameSerial 不變，貨艙0但 flag 未降無事件，完成後唯一 Harvest packet 到最近 dock 礦；實際 SetGuard 命令取消舊 episode；Scenario 替換不送舊事件。None-tag 的 opaque ID bytes 保留原生格式，不強制改為零。
- 新增第 4 case 保護未經 input capture 的 native queued Guard；收緊前中間 DLL 上確實只此 case FAIL，證明原來的寬鬆 queued allowlist 有反例。修正後 source68 已通過；最終 DLL 應再跑同腳本 4 cases。打包證據會區分該中間 DLL 與最終 DLL 的 hash，不能把 source68 或舊3cases算成 final4cases 已驗收。
- 獨立 `ore_harvest_transport_runtime.py` 在 SP0／MP3 × event backend0／1 × nativeMegaGateway 開關共8組均通過：gateway確實啟用、自己的 Harvest 不被回收成手動命令、唯一事件到正確 DoList／OutList；最終 DLL 仍須以該腳本重跑並按 DLL hash 記錄。

可重跑：

```text
python source/assist/tests/ore_harvest_handoff.py --zig <zig-0.13.0/zig.exe> --game <private-exe> --out <isolated-output>
python source/assist/tests/ore_harvest_runtime.py <private-exe> <GameRuntime.dll> --output <runtime-report.json>
python source/assist/tests/ore_harvest_transport_runtime.py <private-exe> <GameRuntime.dll> --output <transport-report.json>
```

完整結果、來源 SHA、原始 log、原生指令對照及中間反例保存在 [evidence/ore-harvest-handoff.json](evidence/ore-harvest-handoff.json)。本機原件在 checkout 同層的 `diagnostics/next-ui-ore-view/ore-harvest/`、`ore-harvest-runtime.json` 與 `ore-harvest-runtime-before-queued-guard.json`。未操作遊戲進程，未發布私有映像或 raw dump，未進行 RA2OL 雙機或真場景採礦驗收。持久筆記庫本輪不可用，本筆記供後續回寫。
