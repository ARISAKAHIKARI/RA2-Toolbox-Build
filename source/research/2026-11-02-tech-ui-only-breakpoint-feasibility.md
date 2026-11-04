# 科技：不改程式碼／規則的 UI 參數 Hook 候選

日期：2026-11-02。這是唯讀可行性研究，沒有新增產品模式、設置遊戲 DR 暫存器、註冊遊戲 VEH、修改正在執行的遊戲或推送提交。先閱讀 AGENTS、研究索引、`2026-11-01-tech-no-code-feasibility.md` 與 `2026-10-22-tech-131.md`；Library 筆記本仍無可用存取，保留本地研究供回寫。

本輪實際讀取的私有 PE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。只核對其原生指令與現有來源，未公開 EXE 或原始轉儲。不能把先前渲染測試使用的 `247f7288...` 樣本身分沿用到這次核查。

## 已核實的 ABI

`HouseClass::CanBuild(0x4F7870)` 為 thiscall，ECX 是 House；**入口原始** ESP 的布局：

| 入口位置 | 值 |
| --- | --- |
| `[ESP]` | 返回地址 |
| `[ESP+4]` | Type 指標 |
| `[ESP+8]` | buildLimitOnly，低位元組為 bool |
| `[ESP+12]` | includeProduction，低位元組為 bool |

`0x4F7870` 先配置 `0x3C` 棧空間，`0x4F7873` 讀取新 `[ESP+0x44]`，即入口原 `[ESP+8]`。此值非零時，`0x4F7883` 跳至 `0x4F7F9E` 的建造限額路徑，略過前面的完整科技條件。函式仍是原生函式，沒有被替換。

兩個原生側欄呼叫者的實參已核對：

| 呼叫點／返回地址 | 參數 |
| --- | --- |
| `0x6A97D2`／`0x6A97D7` | draw：Type，false，false |
| `0x6AA781`／`0x6AA786` | prune：Type，false，true；EDI 在此為零 |

現有 `placement_bridge.h::riskTechEntry` 也是只把這兩個 UI 呼叫的 buildLimitOnly 改成 true，保留原 includeProduction，然後進入原生 `0x4F7870`。因此，**入口硬體執行斷點＋VEH 在參數層面有可驗證的等價候選**：僅接受上述返回地址、當前本機 House、同一局且已核實身分的許可 Type，調整這一次呼叫的棧參數。這類攔截不必改 `.text`、Type 規則或虛表；不能籠統說不改遊戲程式碼便不可能 Hook。

這僅支持實驗原型可行性，未驗證 Windows 調試例外傳遞。Microsoft 的 [VEH 範例](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler) 說明 handler 可取得並處理例外上下文；[x86 CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context-r2) 列出 ESP、ECX、EIP 與調試暫存器等架構資料。它們不是遊戲相容性或平台允許此做法的證據。

待驗：每執行緒四個硬體槽與其他調試工具的衝突、既有調試例外的正確轉交、重入和上下文恢復、遊戲執行緒綁定／退出、異常時停用與卸載，以及大量 CanBuild 呼叫的例外開銷。入口斷點會先攔到其他呼叫者，再由 handler 篩選。沒有研究／實作繞過平台檢測，不能宣稱 RA2OL 不會檢查 DR／VEH 或不會不同步。

更聚焦的未驗證變體是將執行斷點設在兩條 UI CALL 本身（`0x6A97D2`、`0x6AA781`），保留原 CALL 執行。此時參數已全部 push，但 CALL 尚未壓入返回地址，故棧頂是 Type、`[ESP+4]` 是 buildLimitOnly、`[ESP+8]` 是 includeProduction；ECX 仍是本機 House。只調整本次 `[ESP+4]` 的 bool 參數即可保持相同原生呼叫語義，不能誤用函式入口的 `ESP+8` 偏移。它占兩個硬體槽，避免 CanBuild 全域入口令其他 AI／非 UI 呼叫都先觸發例外；仍須逐一驗原 CALL 指令／目標、House／Type 身分、調試槽衝突、例外與執行緒生命週期。這次沒有設置此斷點或實作 handler。

## 補圖示、點擊與真正生產分別判斷

1. `Sidebar::AddCameo(0x6A6300)` 能新增圖示，但不改變科技判定。原生 `0x6A97D2` 繪製與 `0x6AA781` 裁剪都直接 E8 呼叫 CanBuild；**只替換 IAT 或 CanBuild 的虛表不能攔截這兩條直接呼叫**。FindFactory 本身是 Type vtable `+0x94`，替換它亦不能自動跳過隨後的直接 CanBuild。
2. 原生裁剪會遍歷各 Strip 的現有圖示（count `+0x54`，項目 `+0x58`，stride 52），不合法項目可由 `0x6AA7E7` 產生 Abandon=`0x10`；同一失敗項目分支在 `House::FindFactory(0x500510)` 成功後，還可由 `0x6AA8CD` 產生 AbandonAll=`0x2E`。它不只是視覺更新。
3. `House::Produce(0x4FA350)` 在 `0x4FA438` 用 Type vtable `+0x94` 尋找實有工廠；`FindFactory(0x5F7900)` 在 `0x5F79A3` 呼叫 CanBuild(Type,true,true)。因此原生 Produce 事件能使用較寬的科技入口，無須先改 Type 規則；但仍保留真正工廠、產品／工廠 OwnerMask 相交、類別、海陸、電力與 BuildLimit。`0x4FA5D1` 才進入 `DemandProduction(0x4C9C70)`。沒有相容真工廠就不能把這條路稱為完整跨陣營全科技。

「在原建造欄位置覆蓋代理圖示、攔截點擊後送原生 Produce」可作另一個有限候選，但它是工具箱代理介面，不是已證實完整原生側欄流程。只在覆蓋層畫鎖定類型、完全不加入原生 Strip，確實不會讓**這個逐圖示裁剪迴圈**直接遍歷該類型；不能據此保證整條生產鏈不取消：原生 Produce／後續工廠關聯、UI 更新是否自動補入圖示，以及其他取消、生產完成、佇列、取消和放置路徑仍需端到端核對。若仍將鎖定類型加入原生列表，覆蓋亮色不會阻止背後的裁剪／取消。

## 收斂

在不改 `.text`、不改 Type 規則的條件下，**有可驗證的 UI 棧參數攔截候選，並非技術上完全不可能**。它可以重現 Classic 的兩個 UI CanBuild 參數語義；真實工廠、OwnerMask 與限額維持原生。這次只完成靜態 ABI 核對；沒有交付可啟用模式、實機硬體斷點測試、完整原生生產驗收或 RA2OL 雙機同步保證。

後續：已將此候選接為 Assist90 的第5種科技模式，實作、可操作入口與真正 Windows DR 命中證據見 [參數攔截實作](2026-11-02-tech-ui-parameter-interception.md)。本頁的「尚未實作」描述保留為當時研究狀態。
