# 戰網部署與 DLL 辨識：官方套件樣本含加固機制，使用者現場仍待確認

日期：2026-10-27（UTC+8）。

## 問題與結論

使用者提出「戰網可能沒有使用 Hardened，只是普通 CnCNet」，並詢問 DLL 的檔名。這是部署假說，不是已確認的現場版本資訊。

**這次分析的檔案叫 `CnCNet-Spawner.dll`。官方普通 CnCNetYR 和 Hardened v0.0.0.17 發布包中的檔案同名。** 因此檔名中沒有 Hardened，不能用來辨認 variant；`gamemd-spawn.exe` 的名稱亦不能代表載入的 DLL 類型。較舊的 CnCNet/Ares 組合可能另見 `cncnet5.dll`，那是不同組件，不把其名稱等同於本次 Spawner。

本輪進一步取得官方客戶端倉庫的 stable tag 與 develop 固定快照所附帶 DLL，兩者都已確認有程式碼 CRC、指定資料檢查及刻意的異常失敗路徑。因此不能籠統說官方客戶端只使用前輪那份普通 DLL。另一方面，這仍然不能證明使用者正在使用任一被分析樣本。

## 1. 樣本固定與來源邊界

倉庫：CnCNet/cncnet-yr-client-package；路徑：`package/CnCNet-Spawner.dll`。

| 樣本 | 固定 commit | 大小 | DLL SHA-256 |
| --- | --- | ---: | --- |
| yr-9.3.5 tag | 2c0d58dab10ebcd429a17bfdf6c33e0d1e1222e3 | 423936 | 72229ff26ef037261a8190707d3a71f13d7bbaabeb9955b8a9cb0697b94bfabd |
| develop 固定快照 | c0a7af45562128a6f75912167878c613d673cd3e | 770560 | bf38996016a51637a4f1fce9620a6f8b684209e41fb791e821426e7ab16947e7 |

下載後分別核對 Git blob SHA：`04d41571d12bd7190f14b7e6c3b9b4b5620dccad`、`7d613bd7e12888951e5d046b7ca049cfa7924bba`，與 GitHub tree metadata 一致。沒有執行 DLL，沒有下載遊戲 EXE。

這是對**官方倉庫特定 tag／commit 附帶檔案**的結論，沒有另下載及核對完整安裝包 ZIP，也沒有核對使用者本機檔案或對局載入模組。develop 不作 stable 的替代證據；兩者分別分析。

兩份 DLL 均不同於前輪 v17 普通版（SHA-256 `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`）與 v17 Hardened（`62466e9e534ceb15c5d71fb05c62ad98a2b2b4903c569ed0c82189ef86ea3b7d`）。不以大小或相同 RVA 的有無代替實碼比對。

補充：stable 與 develop 的 `gamemd-spawn.exe` 在本次 Git tree metadata 中為相同 blob `800f578a705e55ae2df7021daa8d7b40cff02e5b`，而 DLL 是不同 blob。這是 metadata 比對，沒有下載 EXE；也說明僅看主程序檔名或主程序身分不足以辨認 DLL。

## 2. 兩份官方套件 DLL 中已確認的加固行為

| 行為／位置 | yr-9.3.5 DLL RVA | develop DLL RVA |
| --- | --- | --- |
| 50B6F0 的控制判斷替換目標 | 103D0 | 4A630 |
| 程式碼 CRC 基準建立 | 10240 | 4A3E0 |
| 程式碼 CRC 輪巡比較 | 10040 | 4A540 |
| 683E94 場景成功返回 helper | 103B0 | 4A520 |
| 67E6E0 讀檔完成 helper | 102E0 | 4A4D0 |
| 55DE9A 指定資料比較 | FD50 | 4A050 |
| 共用故意異常函式 | FC50 | 49F20 |
| CRC 失配直接呼叫位置 | 10113 | 4A60F |

各相關描述表項已連到 CRT initializer 產生的實際目標，再檢查函式控制流。兩份程式碼驗證均為 248×16KiB，涵蓋 `[0x401000,0x7E1000)`，在 Session 3/4 比較並輪巡。

stable 透過常數 `0xFFBFEFFF` 取反取得掃描起點，CRC 表透過指標指向 `0x81F7B4`；這些資料值已核對。不能因為搜尋不到直接的 `0x401000` 立即數，就說它沒有掃描。

兩份都比較 BuildingType 的 `+EB4/+170C/+157A`（Adjacent／PsychicDetectionRadius／ClickRepairable），並包含 CurrentPlayer／Observer 相關檢查。**develop 在部分 House 檢查之前多一個可跳過該組檢查的 DLL 布林條件，本輪沒有追出其語義。** 不宣稱所有啟用條件與 stable 或 v17 一致。

兩份故障函式均可見改寫 ESP、清零 EDI/EBP、跳向零位址的指令，各有三個資料檢查 caller 與一個程式碼 CRC caller。根代理另外獨立核對兩份故障形狀、RVA 與 Git blob SHA。這證明機制存在，不證明使用者曾命中，也不等同原版 FrameInfo 同步錯誤。

完整固定樣本與控制流分類：[套件 DLL 證據](evidence/cncnet-client-package-variants.json)。

## 3. 如果使用者實際是前輪普通 v17，哪些推論不適用？

前輪普通版的描述表沒有 Hardened 新增的程式碼 CRC 掛接鏈、指定資料比較掛接、那組 House／事件驗證、50B6F0 同址補丁。其實際 UDP 函式已核對為直傳或 4-byte tunnel prefix，不含 Hardened 那項內容衍生 payload 轉換及額外 payload CRC。

其中 `67E6DA` 在普通版已有共用 LoadGame_AfterInit hook；差異是 Hardened 額外的 snapshot helper，不能泛稱普通版該地址沒有 Hook。普通版沒有這組新增機制，也不能外推成沒有任何原生規則、其他檢查或不同步風險。

兩份 v17 都有 `5C0E30/2 = B0 01` 的官方 special-cell shroud desync 修復。原版 FrameInfo 狀態摘要仍需獨立分析。

若確認使用者現場載入前輪普通樣本，應從該對局歸因中撤下「已啟用 Hardened 新增 E530／codeCRC／50B6F0 patch」的前提；不能把針對另一份 DLL 的異常旁路當成修復。

## 4. 現場辨識與排查順序

優先取得對局實際載入的 DLL 路徑與 SHA-256；只在某個目錄找到同名檔案，還不能證明該對局載入了它。找到檔案後可以直接比對二進位，無須要求使用者自行分辨 Hardened 字樣。

若雜湊與已分析樣本均不符，分類為另一份 build，不能強行二選一，也不能沿用別的 build 的 DLL RVA。現有使用者的原版同步錯誤、透明／半透明迷霧問題仍是獨立證據，不因本輪確認官方套件有加固而全部改歸因於反作弊。

本輪沒有改產品、安裝或執行 Hook、實作異常旁路，亦沒有新的 Windows／多人行為驗證。

## 來源

- [官方 yr-9.3.5 tag 的 DLL](https://github.com/CnCNet/cncnet-yr-client-package/blob/2c0d58dab10ebcd429a17bfdf6c33e0d1e1222e3/package/CnCNet-Spawner.dll)
- [官方 develop 固定快照的 DLL](https://github.com/CnCNet/cncnet-yr-client-package/blob/c0a7af45562128a6f75912167878c613d673cd3e/package/CnCNet-Spawner.dll)
- [官方 PR 978](https://github.com/CnCNet/cncnet-yr-client-package/pull/978)：已合入，說明更新反作弊及網路處理；本輪具體機制以實碼為證，未以 PR 說明代替反組譯。
- [v0.0.0.17 發布](https://github.com/CnCNet/yrpp-spawner/releases/tag/v0.0.0.17)
- [舊 CnCNet/Ares 組合](https://github.com/CnCNet/cncnet-for-ares)：README 使用 cncnet5.dll 名稱，與本輪 Spawner 分開。
- [前輪反作弊機制及故意異常恢復限制](2026-10-26-cncnet-anticheat-principles.md)
