> 後續：依使用者要求新增僅寫House+1ED的控制標記選項，沒有把它宣稱為聯機完整控制的等價替代；配套升Assist85。見 [控制標記模式與分類校正](2026-11-01-control-house-flag-mode.md)。本頁一次刷新實作保留。

# 控制全部／科技全開：純資料方案核對與一次刷新

日期：2026-11-01（UTC+8）。修改基線：309e82aa6e4d1be7385bca39820603cddcc0e827。使用者詢問原始版本有無僅修改標記、不改遊戲程式碼的實現，並要求科技全開不再分段處理。

## 結論與來源

| 能力 | 查到的實現 | 能否等價改成只寫標記 |
| --- | --- | --- |
| 聯機控制全部 | 現有 50B6F0／6339BA／637E40 三個程式碼補丁；另有獨立的所屬方／網路選項 | 未找到已證實等價替代。標記與當前玩家指標是不同判定，保留高危，不把改旗標偽裝成聯機修復 |
| 科技全開：受控／經典 | 6A97D2／6AA781 兩個 CanBuild 呼叫點，配合原生 FindFactory、Sidebar::AddCameo(6A6300) | 已找到的 0.3.0 歸檔也有這兩個補丁；不是僅改側欄重畫標記。未找到可靠的一位元完整解鎖 |
| 科技全開：大師模式 | 44574A／445795／4457D0／445810／6AA788 五個站點 | 仍是程式碼補丁。本輪不改模式語義 |
| 科技加強（獨立開關） | 21 個指定類型的 Type+634，完整 int32 TechLevel 改為1並保存原值 | 已是不改程式碼的資料寫入，但不等於跨陣營、前置條件及完整側欄解鎖；仍標高危 |

固定 [YRpp HouseClass.h](https://github.com/Phobos-developers/YRpp/blob/d979554/HouseClass.h#L491-L505) 的 50B6F0 定義：Campaign 分支才使用 IsHumanPlayer／IsInPlayerControl；非 Campaign（包括遭遇戰、聯機）比較 this == CurrentPlayer。欄位為 House+1EC IsHumanPlayer、+1ED IsInPlayerControl；CurrentPlayer 為 A83D4C。另一路 50B730 人類控制判定會讀 IsHumanPlayer，因此亂設該位元並非只改本機選擇畫面。這是固定上游與本地源碼核對，本輪未執行遊戲指令重驗。

現有 player_test_bridge.h 的已停用 switchPlayerContext 會改 House+1EC 與 A83D4C，只是切換單一玩家上下文；SwitchAvailable=false，不當作可用的控制全部。直接改物件 Owner+21C、工具箱代發事件、選中異方物件、原生滑鼠全面操控也不是同一能力。參見 [控制實作](2026-10-22-control-133.md)、[修改大師實際捕獲](2026-10-22-ra2master-280-active-capture.md)。

已核對的 [無 inline 分支](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/87c8b05805619e4add9bee33bb2fda6704b7d23e/source/research/2026-10-27-no-inline-game-patches-plan.md#L37) 是管理器拒絕所有 Patch，沒有完成控制全部的等價替代；其設計明示這些功能不可用／降級。

本地固定 v0.7.20 基線 50f59d03fda6c37ce2c35927565cabb372d8e94e 的 sidebar.h::refreshTech 為還原後回報 -20 的停用 stub，沒有有效純資料科技全開；普通功能有效不能外推成此能力已實現。更早 0.3.0 的兩個側欄補丁及參數已記於 [科技歸檔研究](2026-10-22-tech-131.md)。尚未取得能證實不同實現的 0.3.2 身分來源，不否認使用者對其他舊版本的實測。

## 已落地：科技清單一次刷新

sidebar.h::refreshTech 的模式0／1改為一次遍歷全部 techAllowed 清單，刪掉每幀最多檢查32項、新增8個圖標的分批預算；完成後 techPending=false，完整發布 visible／unavailable。未允許項目不計成不可用。

保留同幀不重複、兩模式各自的工廠檢查參數、原生每頁75容量、既有圖標不重複新增、每秒維護以及模式2只觀察原生側欄。一次掃完並不代表無視工廠或上限強行顯示所有類型。本輪不改補丁範圍，不添加 Produce／Abandon 事件，不更改類型前置條件或 OwnerFlags。

TechEnhance 本來就單次遍歷全部21項，500ms是重新檢查間隔，沒有分幀，本輪不動它。直接改資料仍可能影響其他模擬消費者；不改程式碼不等於資料會自動同步。

## 驗證與邊界

assist/tests/tech_refresh_full_pass.py 抽取實際 production requestTechRefresh／restoreTech／refreshTech 執行30項檢查：80項超過舊32／8上限、512項上限、許可／拒絕、重複喚醒、下幀清單改變、補丁設置失敗、關閉及大師模式。使用修改前函式做對照有11項預期失敗。工廠與 AddCameo 為替身，這驗證刷新調度，不是完整引擎或畫面測試。

編譯、PE與診斷配套載入證據見 [本輪建置](evidence/tech-one-pass-build.json)。Assist84／Runtime4／Entry3／Broker1與Shared佈局均不變。沿用支援EXE SHA-256 268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0；未發布私有遊戲檔案。本輪未做RA2OL雙機、遊戲視覺或大型MOD一次遍歷耗時測量，不宣稱消除不同步。持久Library筆記目前無可用存取工具，按AGENTS保留repo記錄供回填。
