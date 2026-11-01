# 修改大師控制模式：原生站點、ABI 與還原順序

使用者要求把「控制標記」選項替換成修改大師模式。本頁是提供實作端的獨立原始指令核對；本研究本身不修改產品。它確認的是共用控制判定與兩個所屬編號查詢，不把控制成功等同於網路命令安全。

## 來源與身分

先查 INDEX、[1.0.33 實作](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/1e6e25d/source/research/2026-10-22-control-133.md)、[修改大師 2.80 有效捕獲](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/1e6e25d/source/research/2026-10-22-ra2master-280-active-capture.md) 與 [控制回歸審計](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/1e6e25d/source/research/2026-10-26-control-138-regression-audit.md)。Library SQLite 本輪無可用存取，保留 repo 紀錄供後續回寫。

本輪實際只讀核對的遊戲 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。沒有啟動／注入／修改遊戲，也不發布私有 EXE 或原始 dump。舊紀錄另一支援樣本 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` 只有既有核對依據，本輪未重新執行該 PE。

本機 2.90 樣本 SHA `0a689341457d6fcf36e5527dd21129f050152e0a442a3f71852685086300732d` 已核對；靜態檔案未找到上述站點的明文 DWORD，不能把既有 2.80 捕獲冒稱本輪 2.90 動態觀察。標為 2.10.3 的本機分發目錄目前只有說明、日誌與設定，沒有 EXE。既有捕獲中的 Master EXE SHA 為 `47d2792b8a322c2aa8009e338e79b84488d6471a53c5709ac8dfa9d19be70ec6`，是另一份證據。

## 已核對的完整指令

| 位置 | 原值 | 修改後／目的地 | 邊界 |
| --- | --- | --- | --- |
| `0x6339BA` | `E8 31 7D ED FF`，CALL `50B6F0` | `E8 57 7D ED FF`，CALL `50B716` | 完整五 byte CALL |
| `0x637E40` | `E8 AB 38 ED FF`，CALL `50B6F0` | `E8 D1 38 ED FF`，CALL `50B716` | 完整五 byte CALL |
| 原生展開入口的 `0x50B6F7` | `75 1D`，JNE `50B716` | `EB 17`，JMP `50B710` | 完整兩 byte 短跳；僅適用已確認的原生前綴 |
| 本輪支援 PE 的 `0x50B6F0` | `E9 FB 2A 67 00`，JMP `B7E1F0` | `E9 1B 00 00 00`，JMP `50B710` | 完整五 byte JMP，需保存真實原值 |

關鍵差異：本輪 PE 的 `50B6F5..50B6F8` 是四個 `CC`，因此照寫 `50B6F7` 並不在其實際執行路徑。原生展開前綴 `A1 38 B2 A8 00 85 C0 75 1D` 來自已記錄的捕獲；不能混稱本輪 PE 原始前綴。

實作可對精確允許的三種入口統一使用 `50B6F0` 的完整五 byte JMP：原生 `A1 38 B2 A8 00`、本輪 forward `E9 FB 2A 67 00`、舊支援 forward `E9 8B 2C 67 00`。這是修改大師**語義適配**，不是逐 byte 複製其舊捕獲。須同時驗證入口 variant 對應後續、整個原生尾段與兩個 CALL 上下文；任意既有 JMP 不在允許範圍。第二 forward 的目的地不會被新路徑呼叫，但仍應保存並精確恢復原值，不擴大支援樣本聲稱。

歷史捕獲第三處是 `6378E1`，不是 `637E41`。本輪重新確認 `6378DD` 開始的 MOV 長六 byte，`6378E0` 位於其絕對位址運算元內，不能視為 CALL 邊界。`637E40` 是 Toolbox 根據原生合法呼叫點做的對應修正，**不宣稱修改大師當時真的寫此位置**。

## 原生 ABI 與作用

`50B6F0` 是 House 控制判定。固定 [YRpp HouseClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/HouseClass.h#L501) 將其命名為 `IsControlledByCurrentPlayer`：原生 Campaign 使用 `IsHumanPlayer/IsInPlayerControl`，其餘模式使用 `this == CurrentPlayer`。本輪實 PE 已有 forward，故受控測試將實 PE 與捕獲前綴組合分開記錄。

- `50B710`：原生 `mov eax,1; ret`，完整六 byte `B8 01 00 00 00 C3`。
- `50B716`：原生 strict，完整十四 byte `8B 15 4C 3D A8 00 33 C0 3B CA 0F 94 C0 C3`。讀取 `CurrentPlayer[A83D4C]` 到 EDX，比較 ECX，返回 EAX=0/1。
- ECX 為 House，沒有 stack 參數，`ret` 不附帶參數回收。這是 thiscall leaf；EDX 是被改寫的暫存器，不是第二個語義參數。若 C++ 以 fastcall 包裝，不能據此推導原生函式接收兩個有效參數。
- strict 保留 ECX 及非 volatile 暫存器，EFLAGS 是比較結果；通用真分支只設 EAX。統一入口 JMP 比捕獲中的短跳少執行 GameMode 讀取／TEST，故 EFLAGS 不完全等同，但 flags 不是此 ABI 返回契約；兩個所屬查詢都在 CALL 後明確 `test al,al`。既有 Toolbox 真分支 thunk 也只 `mov eax,1; ret`。

`6339B0` 先讀 Object `+0x21C` 的 Owner，再讀 House `+0x30` 編號；受控制時會改用 CurrentPlayer 的編號。`637E37..637E55` 是另一段同類編號判斷。只把通用控制改真，會把外方編號折成本方；兩處 CALL 改到 strict 後保留原 OwnerIndex。strict 本身是單純指標相等，不是 House 有效性檢查（包括兩個 null 比較也為真）。

這三處不寫 Owner、CurrentPlayer 或 House 控制資料位元；但共用控制判定影響範圍大於 UI，不能說只影響選取。兩個編號查詢也不等於所有事件構造器；不能推論已保留或安全同步所有 Event.House。CommandOwner／NetworkHousePreserve 仍應獨立明確選擇，不能因模式切換自動開啟。

## 所有權與還原建議

1. 先清理舊控制標記及另一控制實作，保留失敗還原責任；沒有完成前不可安裝新組或回報可用。
2. 對全部站點與依賴做精確預檢。先安裝兩個 strict CALL，最後才把共用判定改真；未知／相同但無本方所有權的 after 不可直接認領。
3. Master 是直接跳向遊戲原生真分支，沒有 DLL 布林閘門可立即停用。關閉、回滾必須先還原共用 predicate，確認其已解除後才撤 strict CALL。若 predicate 還原失敗仍在返回真，先撤 strict 會重新出現編號折成本方的錯誤；應保留 strict 與 pending 狀態，稍後重試。
4. Patch 回傳失敗可能發生在已寫入之後，應查實際 byte／registry 所有權，不以單一成功布林丟棄還原責任。讀取失敗保留責任；外部 byte 改動不覆蓋；狀態不得虛報正常。
5. 若 EntryManager 需要允許原生目標，例外僅限精確的三個 site→target→bytes 組合，不能全面放開到遊戲任意位置的 detour。

## 本輪受控驗證

[audit_master_control_native.py](../tools/audit_master_control_native.py) 只接受支援 SHA；[master-control-native-audit.json](evidence/master-control-native-audit.json) 為 **777 項 PASS**。

實 PE forward／捕獲原生前綴組合各測原版、只改 predicate、完整 Master、統一五 byte 入口 Master；涵蓋模式 0/3/4/5、四種 House 控制資料組合、本方／外方。每例執行控制葉函式、完整 `6339B0`、`637E37..637E55` 編號片段；另外九組 strict 檢查返回值、ECX／EDX、stack 與非 volatile 暫存器，包括 CurrentPlayer 為 null。檢查的函式沒有非 stack 資料寫入。

受控 Master 兩種入口的外方控制結果都為真，兩個編號查詢仍返回外方 1；只放寬 predicate 的對照可返回本方 0。指令解碼另外確認 CALL／JMP 長度、目標及錯址不在指令邊界。測試只在模擬器內修改映像副本，沒有操作真正遊戲。

```text
python source/tools/audit_master_control_native.py --exe <supported-private-exe> --output source/research/evidence/master-control-native-audit.json
```

這不是已編譯產品的 ownership 測試，也不是完整移動／開火／出售、事件入隊、Windows 遊戲或雙機同步驗收。仍需由實作端另驗模式切換與部分寫入／還原失敗，產品保留高危標示；本輪不能證明這會解決使用者的聯機掉線。
