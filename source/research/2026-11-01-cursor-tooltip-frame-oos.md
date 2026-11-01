# 高刷光標吞掉原生提示；RA2OL 第三組幀通知 OOS

後續更新：依使用者確認，[普通聯機幀通知與預設略過策略已回退，切屏修正保留](2026-11-01-frame-notification-policy-rollback.md)。下文保留當次版本的歷史證據。

日期：2026-11-01（UTC+8）。基線提交：`4e2e516e861e0f089de69272155e47ab30d2de61`。使用者確認提示消失時**未開渲染升級**。持久 Library 筆記在本次可用工具中不可存取；保留此筆記與證據供同一 SQLite 檔案後續回填。

## 原生提示為何消失

本輪只讀本機遊戲檔案，確認 SHA-256 為 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，與既有受支援樣本及使用者日誌一致。沒有修改、載入或發布該 EXE，沒有注入正在執行的遊戲。

檔案指令確認：

* `CCToolTip::GetToolTipText 0x479050`（thiscall，堆疊一個 unsigned ID，`ret 4`）完整 38 bytes：從 `0x887640` 取 WWMouse，呼叫 vtable `+0x28`，`test eax,eax; jge 0x479064`；若負數，直接返回 nullptr。
* 正常分支將同一 ID 傳給 `0x6D1800`，ECX 固定為 Sidebar `0x87F7E8`，再返回原生文字。沒有其他被跳過的邏輯。
* 原生 getter `0x7B89F0` 為 `mov eax,[ecx+0xC]; ret`，即 WWMouse 的 signed RefCount。符號／欄位名稱與 [YRpp 固定版本 WWMouseClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/WWMouseClass.h) 對照。
* 工具箱 `cursor/source/hide.cpp` 的 `initialHide()` 呼叫 `HideCursor 0x7B9930`，正常從 0 降為 -1，持有期間為抑制原軟體光標而維持此狀態。因此原生提示也被判為不可顯示。這是檔案指令與工具箱程式碼確認的故障機制；使用者畫面尚未現場複測。

## 修正與所有權

既有 CCToolTip vtable 槽 `0x7F74D8` 改由 Runtime 統一管理，高刷與建造時間後綴共用一次安裝。沒有增加 allowlist 站點。正常先執行原生 wrapper；只有返回空文字、工具箱仍持有同一 Mouse 的有效 hide、計數恰為 -1、getter 與完整 wrapper 簽名均符合時，才呼叫其原生正常分支 `0x6D1800`。額外遊戲 hide（如 -2）、替換物件、未知 getter／wrapper 或不能取得執行 lease 都保留原結果。

不寫 RefCount，不反覆 Show/Hide 光標，不改 HideName，不改提示內容的名稱／價格生成器；遊戲仍負責原來的測量、複製、排版和提示框繪製。建造時間為獨立可選後綴，沿用既有 guard。關掉後綴／Assist 脫離不移除 Cursor 仍使用的槽；最後一個使用者釋放才還原。還原失敗保留所有權並阻止 Runtime 換控制端；外來槽值不覆蓋。寫入成功但 flush／保護恢復報錯時，也從中央登記讀取實際所有權以便清理。

## 使用者日誌：首次 OOS 出現在幀通知組

證據：[去除私人路徑的時間線及原始日誌 SHA-256](evidence/ra2ol-frame-first-oos.json)。原始檔名 `hook-test-20261101-101023-27848.log`；不提交完整日誌或模組本機路徑。

| 時間 | 日誌觀察 |
| --- | --- |
| 10:10:24–10:11:05 | 只讀基線，使用者回答正常 |
| 10:11:06–10:11:40 | Windows 訊息 Hook + Runtime，功能全關；使用者回答正常，installed=0、conflicts=0 |
| 10:11:41.292 | 第三組「幀完成通知」 |
| 10:11:42.094 | `0x55DE81` 從原生 `mov [0xA8ED84],edx` 改為跳轉 `GameRuntime.dll+0x2680`；其餘測試站點仍為接受的基線 |
| 10:11:42.400 | 確認安裝後開始觀察 |
| 10:11:43.240 | 測試器觀察到原生 OOS 標記，停止 |
| 10:11:43.814 | Runtime 確認清理，全部站點恢復測前位元組 |

這一輪將優先範圍縮到 `0x55DE81` 安裝／通知執行及其後續作用。尚不能證明 RA2OL 掃描程式碼，也不能保證它是唯一問題；前兩組通過各自觀察窗，不代表永久排除 DLL／訊息路徑。沒有證據支持多個功能 Hook 在這一刻互相覆蓋，因為當時只有一個遊戲站點已安裝。

核對原生鄰近指令 `0x55DE73..0x55DE8E`：先讀／遞增 EDX，`cmp eax,edi`，被替換的是 6-byte store，繼續點 `0x55DE87` 的 JE 使用先前 flags。現有 adapter 重放 store 並保存 GPR／ESP／EFLAGS／FXSAVE；本輪受控執行確認這些狀態未被破壞。另補上通知函數所有提前返回路徑的 LastError 恢復。此處未找到可解釋 OOS 的寄存器／flags 錯誤。

正常版在聯機 Session 3/4 使用現有消息／幀號輪詢，不安裝此幀通知位置；單機 0/5 可沿用原生通知。不是恢復首次遠端同步門檻，也不改其他功能開關。測試器只在明確選中的幀通知步驟附加 `ExplicitDiagnostic` consumer bit，仍可單獨複測此位置；下一步可直接選「建造時間提示」，先走兩個基線，再測該槽，不必經過可疑的幀通知組。沒有隱藏 Hook 或規避平台完整性檢查。

## 驗證範圍

`tools/test_cursor_tooltip_windows.py` 在**獨立 x86 Win32 fixture** 執行生產 Cursor／Tooltip／Frame adapter：原生 38-byte wrapper 在自身固定地址區重現提示為空，測試單位 ID、建造欄 ID、原生 provider 接收者／參數、可選後綴共存、額外 hide、替換物件、簽名不符、讀 lease 拒絕、部分安裝錯誤、外來槽及還原重試。實際 frame adapter 測試所有 GPR／ESP／EFLAGS／完整 FXSAVE、native frame store、活躍／過期／空 callback；直接執行 production maintain 驗證普通聯機零 frame patch、明確診斷可安裝、撤回診斷即還原。固定 VA 僅修改自行產生測試 EXE 的 ASLR flag；未修改任何遊戲或產品二進位。

另驗證 685 項 Windows x86/x64 ABI、十二輪 HHOOK 清理／重用、測試器 21 步與 48+3 站點清單、完整產品與診斷建置、PE／匯入／manifest、診斷隱藏 UI 和 x64→x86 broker／DLL loader 握手。[建置證據](evidence/cursor-tooltip-frame-build.json)。

本次沒有實際遊戲懸停視覺複測，也沒有新版本 RA2OL 雙機對局。故障機制、合成執行、使用者提供的舊版真實日誌與新版現場驗證必須分開解讀。
