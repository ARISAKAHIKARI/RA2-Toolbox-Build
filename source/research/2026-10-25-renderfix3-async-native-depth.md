# RenderFix3：原版深度圖、瞬時動畫、素材常駐與非同步覆蓋層

先查閱 INDEX、RenderFix2 筆記及同一持久 SQLite。問題：建築被地塊切掉、動畫缺失、回選單重載素材、所有覆蓋層希望離開功能/UI 執行緒，小地圖上限 1000%。

## 樣本與證據級別

私有快照 EXE SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。使用現存 01-off、02-on、03-off；原始 EXE、記憶體、資源及重放畫面不提交 GitHub。`verification/renderfix3/native-animation.json` 記錄原指令區間 SHA 與受控執行數量；`capture-replay.json` 記錄帶 validity mask 的生產讀取、地形與建築合成。這些不是完整遊戲實機播放、Windows 像素一致性或多人測試。

## 已處理的 BUILDNGZ 不等於磁碟原檔

原版 `BuildingType::Fetch_Z_Data 0x45E8F0`，thiscall/no stack args：載入全域 `0x89DDBC`，`0x45E94D` 對每個非零 byte **減 0x41（65）並原地寫回**。0x45E940..0x45E95C 的原版迴圈用受控 0..255 共 256 byte 執行，每份快照全部符合 wraparound 減 65。讀取遊戲的 BUILDNGZ 必須將 byte 解釋為 signed delta，不能再次當 unsigned 原檔扣偏置。

此前錯誤：RenderFix2 優先讀記憶體，但與磁碟共用 `value-66`，造成大幅錯誤的前後深度及陰影覆蓋。現在 ActorArt 保存深度圖來源；磁碟沿用上游量測 `value-66`，記憶體使用 `int8(value)-1`，零 byte 保留既有無偏移分支。零 byte 的原始 0/65 已不可逆區分，單一深度單位的邊界一致性尚未完整量測，不宣稱逐像素等同遊戲。

真實 TMP/色盤/建築/已載入動畫合成：02-on、03-off 各有 **6258 個建築像素改變**，相較重複扣偏置的舊路徑；圖像檢查基地/電廠底座已不被同層地面切掉。實際載入的 GACNST_A/GAPOWR_A 仍均可見。這證明本次樣本的遮擋路徑修正，不代表所有 MOD 的遮擋已驗證。

## 建造、售賣、全域動畫

- `Building::GetImage 0x4513D0`：BState `+0x534` 為 0 時改取 BuildingType virtual +0xC0；`0x465960` 讀 **type+0xE00** 的 Buildup SHP，檔名 **+0xE5C[16]**。`+0xDFC` 是 foundation 資料，不是 Buildup（已拒絕按未對齊欄位順序推算的假設）。生產路徑在 construction variant=-2 下讀 Buildup、依 NewTheater 選檔，與正常本體分離快取；相關 fixture 使用不同本體/建造檔。
- `Building::GetCurrentFrame 0x43EF90` thiscall/no args：實際 Stage.Value 是 **object+0xF8**；不是原版 body 的固定 healthy/damaged 0/1。BState 0 顯示 Stage；selling mission 19 用 type+F04 的 start 加 +F08 count 減 stage 減 1。Gate type+16B7 先反轉；laser fence/firestorm 的 +16BF/+16C0 先使用 object+618/+61C。CurrentMission virtual+184 是 `0x5B3040`，+AC=-1 時改讀 +B4。
- 對真實 GACNST 的模擬記憶體只改 BState、Stage、Mission：原指令在 02-on、03-off 各驗證 3 個建造幀與 3 個售賣幀。01-off 沒有符合資源狀態的 GACNST，計為 0。這是受控幀選擇，沒有模擬整個建築 Update。
- 舊採集僅 Building::Anims/DamageFire，所以沒有母建築槽的爆炸根本不進渲染。增加 **AnimClass::Array 0xA8E9A8**，跟建築槽去重、按已揭示地格篩選、驗證 ID/Type/Alive/Invisible/TimeToDie，再依原 Layer/YSort 插入場景。記憶體重放全域合併後動畫 marker 為 84/88/90；不將這些數量宣稱為全部成功解碼。
- 無父槽動畫 Owner house 為 +180，Location +9C；真正 OwnerObject+CC 使用 owner 中心。樣本非建築 owner 的 center vtable+48 為 `0x5F65A0`（Location），建築為已驗證 `0x447AC0` foundation center。附著隱形/失效 Techno 會拒絕。ANIM.PAL、建築/替代色盤及自訂色盤沿各自路徑。
- 原版 Draw_It 0x43D81D..43D82F 把 type+1548 的 signed short ExtraLight 加到建築亮度，本次補入。此樣本查看到的 ExtraLight 為 0，不把它當作發黑的已證明主因。

## 光照與相機判斷

繼續採用上輪核對的 CNCMaps VPL 原始 normal、朝向、漫反射及鏡面項。固定視線方向參與鏡面項，並不等於平移相機位置就重算光源。SHP 表面明暗來自素材，另外套用原版地格強度與色調；不能把所有 SHP/VXL 任意加亮或重複疊加高度光照。來源版本、EA FA2 與 CNCMaps 的具體差異仍見 RenderFix2。最新檢索再次確認兩個 primary repositories，詳細算法依 pinned source code，不採二手討論當證據。

## 非同步與快取

`latest_worker.h`：每個工作者最多一個執行中工作和一個可替換待執行快照；完成後發佈不可變結果。主介面不 join 等待一幀；epoch invalidation 拒絕舊世界完成，結束程式時才 join，之後釋放 GDI+。觀察窗、隊形解碼、戰場合成、統計、建造面板、飛機指示器已分離；小地圖原有工作執行緒保留。Windows 視窗輸入、位置與完成畫面的提交仍在各自視窗擁有執行緒。讀取器、解碼器、字形/圖示快取均各執行緒獨立；傳遞 frame/preview/設定/文字副本，不把可變 UI 全域或借用 Image* 交給工作者。

MIX 成功讀取新增每 Context 64 MiB 有界最近使用快取；檔案/索引指紋、PID/對局變化失效，loose override 每次優先檢查而不被舊 archive cache 遮掉。保留離開視野的 type/pose，在預算壓力下回收不在當前畫面的項目。動畫循環不再每次刪掉前幀重解碼。觀察窗 source 上限 128 MiB，像素 32M；小地圖 source 上限 128 MiB、actor 像素 8M，避免 32 位程式無上限吃記憶體。小地圖 hide 是 pause；同局回選單再進來保留 sampler/renderer/素材，尺寸變化僅換背景畫布；示意底圖依不可變 terrain bank 與尺寸快取，100 次相同請求重用同一結果，避免 1000% 下每幀重做縮放；初載不再把因本輪預算跳過的地塊永久烘焙成占位塊。

1000% 同時修改 UI 驗證、INI 載入、布局與真實 raster 尺寸（最高 2048×2048，並受遊戲視窗可用區域限制），保留預設 200%。

## 回呼與等待

這次讀取既有 `bridge.cpp`、`cooperative_bridge.h::cooperativeTick`：未準備好時直接 return，已完成計畫按當幀事件預算提交，ACK 是完成標記；`WaitForSingleObject(handle,0)` 是程序存活探測，不是等待外部計算。本次沒有改遊戲 ABI/命令語意，也沒有新增遊戲等待渲染的路徑。需要當場返回值的 Hook 若改為同步等工作者，仍會阻塞；可延後的計畫適合快照→背景計算→下次回呼驗證/提交。不能將非執行緒安全的遊戲物件操作直接搬到外部執行緒。

## 驗證界限

新增真正雙執行緒測試：將第一個工作刻意阻塞，呼叫執行緒仍可連續提交 998 個快照、取結果和 invalidate；實際只執行 1 與 1000，舊 epoch 不可發佈。檢查 shutdown 後結果/預覽圖像生命週期、資源快取 50 次重讀零新增 file Read、resize 不重讀 TMP、1600px 真實畫布、construction/selling/無父槽動畫解碼、原指令與帶地形快照重放。本機全部既有 23 組 assist C++ 回歸、7 組 Python UI/素材測試、3 組 unit-monitor 回歸、新工作者測試及 architecture audit 通過。工作者、watch/formation 生產引擎、建築動畫、radar scene 亦通過 AddressSanitizer/UBSan（容器不啟用 LeakSanitizer）。Windows smoke 增加真實 831 控制項 1000% 保存和重啟，以及啟動生產統計演示、等候非同步 layered surface 完成、檢查 UI 回應並在工作者使用中關閉；Windows 執行結果另由 CI 記錄。沒有使用者機器上的實際 FPS、首次載入毫秒數或完整遊戲動畫播放錄影，不能給出虛構加速倍數。
