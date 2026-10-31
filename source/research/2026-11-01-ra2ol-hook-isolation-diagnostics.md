# RA2OL 同步錯誤、獨立 Hook 觀察器與小地圖閃爍候選

## 問題與既有證據

使用者確認平台是 RA2OL，錯誤是遊戲同步錯誤；閃爍的是工具箱實景小地圖。要求把 Hook 依次開啟、每 30 秒詢問是否斷線。本次基於分支 `codex/crc-map-shadow-hotfix32-20261031` 的 `2214328080900dd2c177c3082b7538b4035fe1da`，已含 `17ee48810d33d47203d52893d3b0472d992f3a11` 的合併。

先檢索 INDEX、首次同步門檻／生命週期、CnCNet 衝突／Hardened／套件版本，以及原生迷霧與小地圖管線筆記。Library 未能解析到 `RA2-Reverse-Engineering-Notes.sqlite` 原文件；未讀到資料庫、不建立替代同名檔，本筆記保留待寫回。

沿用既有私人引擎樣本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的位址約定；本輪沒有執行或發布該樣本，沒有新增引擎版本支援認證。這是原始碼審查、本機 Windows 測試器啟動／broker 載入及合成策略測試，不是 RA2OL／雙機測試。

## 平台原因仍待實際模組識別

[RA2OL 官方首頁](https://www.ra2ol.com/) 在本次查閱時聲明使用 CNCNET 遊戲補丁並描述反作弊能力。這只能支持「需識別實際載入模組與補丁差異」，不能證明用戶部署的是既有研究中的 Hardened v17、正在掃描本 Hook 或本次 OOS 由偵測觸發。舊筆記的 Hardened 二進位證據不能直接套到未知 RA2OL 模組。

測試器在安裝前只讀記錄模組路徑／SHA-256、51 個站點位元組、已存在的跳轉目標／所屬模組。這能辨識已改寫位置、輔助版本比對，不能靠靜態讀數判定一切動態掃描。測試器不隱藏 DLL、不修改驗證器，也不強覆寫外部補丁。

## 新確認：相機補丁不依賴功能開關

`assist/source/bridge.cpp` 就緒路徑每次呼叫 `smallMapCameraTick()`。`assist/source/small_map_camera_bridge.h` 以 `smallcamera::needed(cameraGeometry())` 決定安裝 `0x6D8640`，不需要 `ViewZoom` 開關。因此「所有設定關閉」不能等同「零遊戲補丁」；小地圖尺寸／視窗條件不同可能使基線不同。這是來源程式確認，尚未在使用者 RA2OL 進程觀察到。

原生入口以 Point* 作為唯一堆疊參數，替代回呼 `bool __stdcall smallMapClamp(smallcamera::Point*)` 保留參數清理；原始方法不依賴 ECX。幾何讀取沿用 `87F8DC/87F8E4/87F8E8/87F8EC/87F8F0/886FA8/886FAC`；本次未修改 ABI 或幾何演算法。

測試版以 `RA2_HOOK_DIAGNOSTICS` 為該入口增加顯式 opt-in，版本 1 的獨立 24-byte shared mapping 按遊戲 PID、owner PID 與 2 秒心跳驗證。只有相機步驟能允許它，且仍需原幾何／簽名／所有權條件。正常產品未定義該巨集，行為保持原樣；測試器校驗配套 DLL/broker 的完整 SHA-256，不能混用正常 DLL。

## 觀察器流程與覆蓋

`hook_diagnostics/source/catalog.h` 是分組來源：21 步，包括只讀基線、只載入 Runtime 的基線、19 組功能觀察。48 個不同遊戲站點在排程中，其中相機為條件式；`51A4D8/51C32B/4A9840` 三個產品已停用位置不強開，14 個禁止的舊站點不測。Windows WH_GETMESSAGE／WH_CALLWNDPROCRET 是共用傳輸，在第二步一併安裝，不強拆彼此依賴的傳輸入口。

每一組以既有 Assist／GameEntry 路徑請求，核對實際位元組、跳轉落在 GameRuntime 模組、非預期位置未變、registry 數、即時 commandStatus 後才計 30 秒；不是計設定開關的時間。讀取或狀態異常不算通過，無回覆不前進。正常判定後先撤回並驗證全 51 站點與測前位元組一致、registry=0，再進下一步。退出還需 Runtime stop=2；失敗持續保留清理能力，不能強抹外部補丁。

宿主載入 Runtime 前沿用首次遠端 FrameInfo/CRC 證據門檻，Runtime 內仍有原有獨立門檻。關鍵狀態：Frame `A8ED84`、running `A8E9A0`、House `A83D4C`、Scenario `A8B230`、Session `A8B238`、原生 OOS `A8B8C2`；詳細事件／CRC ABI 參照 [既有首次同步筆記](2026-10-31-peer-sync-hook-activation-audit.md)。遊戲暫停時停止倒計時，換局／回退／OOS 停止新增補丁。測試程式不自動操作單位、不產生遊戲指令。科技組只走原有側欄 OpenTech，科技資料欄位加強保持關閉。

CRC 組只安裝八個 MapIsClear 適配點，不執行 Reveal，因此不涵蓋全圖 Cell／Visionary 寫入；直接地圖五點組排最後。每組程式位元組還原不能倒轉先前模擬事件；延遲觸發與組合衝突仍可能存在。可完整重啟後在下拉選單選可疑單組複測，兩個基線始終保留。30 秒正常不是整局／全功能保證。

## 實景小地圖閃爍：尚未確認的候選

`external_reader/source/radar_map.h` 的 `Sampler::step` 分批建立 `pending_`：單格 `read_` 失敗或座標不符直接 continue，完成指標掃描後仍發布 `terrain_=pending_`。鄰格 mask 讀取失敗也可成為當輪不透明／缺失遮罩。若讀取品質交替，下一個完成批次可能在缺失陰影與正常陰影之間切換；`radar_live_shroud.h` 亦需區分合法未知格與暫時讀取失敗。

這是源碼可疑路徑，不是已取得現場的故障證據。沒有改迷霧渲染程式，不能宣稱已修好；也不能把這種顯示問題直接等同於平台偵測或模擬 CRC 改變。後續應量測逐批讀失敗／遮罩完整性與發布序號，再決定保留前一完整遮罩或重試的策略。

## 已做與未做的驗證

* Zig 0.13.0 產出 x64 RA2HookTest、x86 診斷 Runtime 與 x86 broker。診斷 bridge 重編；其他基礎物件來自已驗證的同份 2214328 完整建置。Assist83/Shared6580496、Runtime4/Control176、Entry3/Request108 保持一致。
* Windows 上實際執行 `sequence.cpp`：來源清單 51=48+3、禁止 14、21 步、安裝後計時、人工判定、暫停、還原失敗／OOS 不前進。執行 `camera_gate.cpp`：預設關閉、PID／owner／版本／過期／計時回捲。
* 實際執行 `RA2HookTest.exe --ui-smoke`，建立隱藏視窗／控件，拒絕 PID 0、Idle 判定無效、配套檔案身份匹配；未嘗試遊戲附加。
* 實際執行 `--broker-smoke`，啟動 x86 broker 並在 broker 內 LoadLibraryW 診斷 Runtime，檢查匯出的協議資料；不等於 DLL 已在遊戲成功安裝 Hook。
* `tools/verify_hook_diagnostics.py` 檢查三檔 PE 架構、系統匯入、DLL 匯出／回呼執行節、版本、manifest 與協議。測試器 asInvoker；正式工具箱的 requireAdministrator 驗證未放寬。
* 未做 RA2OL 原生對局、雙機、現場小地圖、實際對端同步門檻可觀察性或每組 Hook 的遊戲內執行測試。新增 CI 定義不代表 CI 已執行成功。

可重現指令與完整步驟見 [測試器 README](../hook_diagnostics/README.md)，本輪 [建置身份與檢查記錄](evidence/ra2ol-hook-diagnostic-build.json)。輸出包附 PE 驗證、來源身份、檔案 SHA-256 與測試範圍；不含私人遊戲執行檔或 dump。
