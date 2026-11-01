# 恢復幀完成通知的原啟用策略，保留切屏修正

日期：2026-11-01（UTC+8）；修改基線：`77790c4f314e22dc2df3f25746c6c4f3b0dd8e1b`。

使用者確認撤回「聯機時強制改用輪詢、不安裝幀完成通知」與測試器預設略過，並再次明確要求保留切屏修正。本輪是啟用策略回退，沒有新增反組譯結論。歷史 OOS 日誌及其不確定性仍保留；不能用策略回退推論 RA2OL 聯機已通過。

## 目前行為

* 有幀取樣需求時，支援的 Session 0/5/3/4 均可安裝既有 `0x55DE81` 幀完成通知，不再要求聯機附加 ExplicitDiagnostic。該 bit 保留為既有通道的診斷標記，不能繞過本地就緒／簽名／所有權限制。
* 移除「普通聯機必須撤回現有幀通知」分支。保留 `77790c4` 的生命週期修正：已確認控制端存活且功能仍開時，短暫切屏、run flag 暫停及心跳不新不反覆拆裝；新安裝與 callback 工作仍要求新心跳。
* 保留 adapter 的完整暫存器／EFLAGS／FXSAVE 與 LastError 恢復；不回退提示修正，不恢復首次遠端幀同步門檻。
* 測試器新的預設排除清單為空，基線仍預設關閉，因此「全部」排定 19 個功能組。任何項目仍可手動略過。既有 hook-test.ini 的使用者選擇會保留；若沿用先前設定，可手動取消幀通知的略過。
* Cursor／建造時間提示的切屏生命週期、觀察暫停／恢復、快照重試、停止原因、提前正常、還原確認與 OOS 後新對局再測均保留。

## 地址、證據與驗證邊界

沿用已支援 EXE SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的既有位置：幀 store `0x55DE81`（6-byte `mov [0xA8ED84],edx`）、counter `0xA8ED84`、Session `0xA8B238`、House `0xA83D4C`、Running `0xA8E9A0`、OOS `0xA8B8C2`。沒有修改共享欄位或 ABI（Assist83／Runtime4／Entry3／FrameSignal1），沒有發布遊戲 EXE 或 dump。持久 Library 筆記在本次可用工具中不可存取，保留本記錄供同一 SQLite 後續回填。

`assist/tests/cursor_tooltip_windows.cpp` 實際執行 production maintain，確認普通 0/5/3/4 安裝、診斷 bit 加入／移除均不拆裝、暫停／過期 callback 保留所有權但不執行取樣，以及控制端結束的站點還原；尚未就緒／已有 OOS 不新安裝。原有 tooltip 與 adapter 保存狀態檢查仍通過。`hook_diagnostics/tests/sequence.cpp` 與隱藏 UI 驗證預設包括幀通知、手動排除仍可用，其他測試流程不變。

[本輪建置與回歸結果](evidence/frame-notification-policy-rollback-build.json)。完成本機正常產品與測試器編譯、PE／匯入／協議核對、實際隱藏 UI／broker 載入握手；所有執行均在測試程序內，沒有連接遊戲。尚未做新版 RA2OL 雙機或遊戲視覺實測。
