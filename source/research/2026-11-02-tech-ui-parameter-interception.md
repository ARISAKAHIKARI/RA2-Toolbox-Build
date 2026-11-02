# 原生面板科技：不改指令或規則的參數攔截模式

問題：在不改遊戲程式碼段、虛表、Type 規則資料的條件下，能否保留原生面板的圖示、點擊與生產，重現 Classic 的兩處 UI 參數放寬？本次把先前的[可行性研究](2026-11-02-tech-ui-only-breakpoint-feasibility.md)落實為 opt-in 實驗模式 4。這仍是 Hook；不能把「沒有 `.text` 寫入」解釋為沒有攔截、平台一定接受、聯機一定同步或無額外成本。

支援樣本：私有 `gamemd-spawn.exe` SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，只讀分析，未附遊戲檔。最終受測 `GameRuntime.dll` SHA-256 `f7a0dd9bf0a48216c2fc8fabd12915010bc18ddfc8d1657c48a97b81e4e07555`。Library notebook 本輪不可用，新發現保存在本 note 與測試報告，待後續寫回。

## ABI 與實際改動範圍

| 原生位置 | 精確語義 | 本模式處理 |
| --- | --- | --- |
| `0x6A97D2`，返回 `0x6A97D7` | 原生繪製資格路徑 CALL `House::CanBuild`，`ECX=House` | 兩個 hardware execution breakpoint 之一，在原 CALL 執行之前分類 |
| `0x6AA781`，返回 `0x6AA786` | 原生 cameo prune 路徑 CALL 同一函式 | 第二個 execution breakpoint，保持 prune 自己的 includeProduction |
| `0x4F7870` | thiscall `CanBuild(Type*,bool buildLimitOnly,bool includeProduction)` | 函式本身完全保留，也不在全域入口設斷點 |
| CALL 前 `ESP+0/+4/+8` | Type、buildLimitOnly、includeProduction | 只把已確認為 false 的 `ESP+4` 設為 true；不動 Type、House、includeProduction、返回位址或原 CALL |

繪製路徑的 includeProduction 是 0，prune 是 1。進入 CanBuild 後因原 CALL 推入返回位址，對應參數變為 `ESP+4/+8/+12`；不能套用函式入口的偏移到 CALL 前斷點。完整指令簽名 FNV-1a：`0x6A97C3` 長 20 = `D3C40F9D`，`0x6AA777` 長 15 = `26F9619E`，涵蓋完整三參數 pushes 與原 CALL；CanBuild 入口 `0x4F7870` 長 16 = `6995B50E`。早期 13-byte 窗口從指令中間開始，本次已替換，避免漏驗 push opcode。

分類還要求：目前模式 4、UnsafeTech 許可、非 stop/fault、原生 Running、同 Scenario/Session、未倒退 Frame、本機 House 指標與 UID、非觀察者/已結束 House、允許清單中的 Type 指標/UID/vtable、兩個呼叫點各自原始 includeProduction。callback 只讀固定欄位與預先建立的有界雜湊表，變更當次 stack bool；不分配、不等鎖、不呼引擎、不發事件、不寫規則。

圖示仍用現有原生 `FindFactory`、`AddCameo` 與 native sidebar maintenance。`Produce` 接收端、真工廠、OwnerMask、類別、naval、電力及 BuildLimit 等原生條件仍保留。這不是跨陣營無條件生產所有項目的保證；沒有相容真工廠的科技不會憑空有產能，也沒有另外的工具箱生產清單冒充原生面板。

## Debug-register 與生命週期所有權

`tech_ui_breakpoint_core.h` 以真實 thread handle 綁定現有 Runtime 遊戲執行緒。非阻塞 helper thread 短暫 Suspend/Get/Set/讀回/Resume；Runtime 呼叫者不等待安裝完成。只選 address=0 且該槽所有 DR7 控制位皆空的兩槽；不足時拒裝，不偷用 debugger 或平台的槽位。DLL 在登記自己的 VEH 前 pin，避免殘留 callback 指向已卸載程式碼。

VEH 僅消費本執行緒、本槽完整位址/DR7、相符 EIP 及純本方 single-step 原因。若 Dr6 同時存在其他 breakpoint/TF/BD/BT 原因，整個 CONTEXT 逐 byte 不改，繼續交給其他處理者。一般非己例外也繼續傳遞；只移除自己取得的 VEH handle。

安裝與撤回皆讀回驗證。`SetThreadContext` 回傳成功本身不足以宣稱撤回完成：讀回失敗，或仍讀回己方槽，會保留 lease/VEH 與 cleanup 責任再試；已被外部替換的槽不覆寫。每 500 ms 的維護核對外部 context 變化。stop acknowledgement 必須等自己的 DR 租約、helper 與 VEH 真正退休。切出視窗不拆 Hook。停用 flag/stop/mode 等亦由 callback 即時判斷，不等下一輪 dispatcher 才停改參數。

模式間轉換會在存在非原生可建的在製產品時回報 `-43`、保留舊模式，避免異步撤/裝期間 native prune 取消產品。正常原生可建的忙碌工廠不因這個保護被一律封鎖。規則資料模式的 journal 必須先還原，舊指令 Hook 必須完整退休，才啟動模式 4；離開模式 4 也先確認 DR 退休，才准許新的規則/指令實作。

獨立 TechEnhance 是另一個規則寫入者，模式 4 全程暫停它並保留使用者設定；進入前先還原其原值，退出後原設定可恢復。不能只在第一次開模式時還原，然後下一 tick 又改回規則。

Assist 協議升為 90，Shared `6584880` bytes，新增 tail diagnostics：`int32 techParameterStatus`，其後 `uint32 techParameterError/Slots/Exceptions`；本輪還包含 Preview 尾端四個 int32 camera/viewport 值及 small-map opt-in/status。766 項真 Windows x86/x64 wire layout 相符。status 0=off、1=ready、2=pending、-1=原生簽名不符、-2=不足兩個空 DR 槽、-3=context API/驗證失敗、-4=外部 context 改動、-5=執行緒所有權問題。Slots 是數量；Exceptions 是分類之前已確認屬於本方的硬體命中數，不等同成功生產次數。UI 的 `-45` 表示參數攔截尚未 ready 或不可用。

## 測試與實際限制

真 Windows、自有獨立 x86 程序測試使用同一 production core，`-O2 -UNDEBUG`，並有 `#ifdef NDEBUG #error` 防止斷言被最佳化移除。[可提交證據](evidence/tech-ui-parameter-windows.json)記錄來源/EXE SHA、編譯旗標和 13 類檢查：真 DR 命中、兩種原 CALL 的 stack 語義、House/Type gate、原始碼 bytes 不變、另一 VEH 的例外傳遞、混合外因 CONTEXT 不改、foreign 槽保留、四槽已滿拒裝、foreign replacement 保留，以及兩條 restore fault seam。最後兩條以真已安裝 DR 配合注入 Get-readback 失敗/Set 假成功，確認無法驗證時不早刪 VEH，再實際恢復。

同一 fixture 連續做三次兩 caller 各 10000 呼叫，每次精確 20000 exceptions/參數改動。QPC 增量為 8.408095、8.525345、8.917160 μs/exception，中位 8.525345。這是此機當時負載下、簡化 classifier 的 exception overhead，**不是遊戲 FPS，也不是完整 production callback 耗時**。若一幀有數百次 UI 查詢，這條路徑仍有可觀成本；因此保持明確 opt-in，不能用沒有寫 `.text` 推論它更快。

`tech_ui_parameter_runtime.py` 對最終已編譯 DLL 執行 14 組 x86 受控測試：SP/MP 合成世界的開關與 cameo、真正 compiled callback 的局部 stack 變更與 UID/世界/即時停用 gates、完整簽名拒絕、DR 槽拒裝、七種雙向在產轉換保護、TechEnhance 全程協調、stop 等確認還原。Windows debug API 在此 driver **有明確模型 seam**；真 DR/VEH 由前述 Windows fixture 另外驗證。受測記憶體中的遊戲 executable sections 與三個完整 Type 物件 bytes 在模式 4 前後 SHA 相同，不改模擬事件或規則的承諾可在這個範圍檢查。

重現命令：

```text
python source/assist/tests/tech_ui_breakpoint_windows.py --zig <zig.exe> --build-dir <temporary-directory> --output <windows-report.json>
python source/assist/tests/tech_ui_parameter_runtime.py <private-game.exe> <matching-GameRuntime.dll> --output <runtime-report.json>
```

未向使用者遊戲程序安裝斷點，未啟動遊戲、未測 RA2OL 的檢測或聯機同步，未宣稱可規避平台檢測。本實作不包含隱藏 debug registers、攔截平台檢查、覆蓋 foreign VEH/DR 事件等做法。完整平台相容性、實機圖示/生產體感與多人同步仍待正常實測。

Windows API 參考：[VEH 範例與注意事項](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler)、[x86 CONTEXT 欄位](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context-r2)。
