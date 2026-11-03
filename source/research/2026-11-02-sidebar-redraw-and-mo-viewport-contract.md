# 隱藏側欄立即重畫與 MO 地圖縮放整合邊界

> 本文是早期調查階段，後續完整 Runtime／Host／輸入整合見 [2026-11-03 實作與驗證](2026-11-03-mo-native-viewport.md)；下文「目前」指當時階段。

基線 `5983b943e90694f7b2630608bc464d6f8ee148e3`。本輪先實作側欄成功調整幾何後的原生重畫通知；同時重新核對 MO 的地圖縮放。**目前產品仍保留原先整畫面 ChangeResolution 縮放，尚未換成 MO 的 map-only 縮放。** 沒有新增未能工作的縮放選項或 A/R 共享欄位，也沒有放行半套 native viewport Hook。

先讀 AGENTS、研究索引與[既有側欄／縮放記錄](2026-11-02-experimental-view-controls.md)。Library notebook 本輪未能取得，新發現保存在此與兩份只讀證據。支援私有樣本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`；只讀原檔，未啟動或向遊戲程序安裝任何測試。

上游固定為 [MagicShiba/MO_ExperienceEnhancements `33ed558bd38f36b5012eddcc9c0fb109cc68e38a`](https://github.com/MagicShiba/MO_ExperienceEnhancements/tree/33ed558bd38f36b5012eddcc9c0fb109cc68e38a)，查閱 `src/gui/FullMap.cpp`、`ScaleBar.cpp/.h`、`notes/地图缩放.md`、`notes/隐藏侧栏.md`。上游聲稱聯機安全並不是本分支聯機驗證結果。

## 已實作：側欄尺寸成功變更就發重畫請求

原生地圖是增量繪製：把 ViewBounds 或 DSurface 變寬，不代表新露出的區域已進入 dirty list。先前只設側欄的兩個重畫旗標、清 `0xB0CD44` 和工具箱 scene cache，沒有把原生新地圖區域整片標髒。因此不能指望換完尺寸後恰好有滑鼠移動或其他原生變更替它補畫。

目前 `sidebarViewGeometry()` 在 `Set_View_Dimensions(0x4A8960)`／Tactical 視口同步及 gadget 更新成功後，當次直接：

1. 用 `Tactical::RegisterDirtyArea(0x6D2790)` 把完整可用窗口與視口聯集標髒。
2. 呼叫原生 `GScreen::Flag_To_Redraw(0x4F42F0,2)`，要求原生完整重畫。
3. 對 `0xB73550` 的遊戲 HWND `PostMessage(WM_PAINT)`。這是非同步通知，不在幾何函式內呼叫 Render 或同步派送 Windows 消息。

之後只補兩次，覆蓋增量雙緩衝切換；以 **(模擬 Frame, 完整 RenderSerial)** 去重。同一幀、沒有新完整 Render 的重複 Runtime wake 不做額外重畫；PauseView 的模擬幀凍住而 Render 繼續時，仍可完成這三次補畫。不會永久每幀強制重畫。

新增簽名預查：`0x6D2790` 頭 12 bytes FNV `CB533DD6`、`0x6D2B3F` 的 3-byte return FNV `6395A2F9`、`0x4F42F0` 頭 16 bytes FNV `BDBC694C`。簽名不符時，在新側欄表面配置之前拒絕啟用。恢復幾何失敗也不能繼續換回／釋放表面，現已檢查 `sidebarViewGeometry()` 的回傳值。

**ABI 核對：** `RegisterDirtyArea` 以 ECX 帶 Tactical，矩形四個 int **by value** 加一個 stack bool；`0x6D2B3F` 明確 `RET 0x14`。原生 caller 先 push bool、留 16-byte 矩形、填 ECX 再 CALL。產品使用 `__thiscall`，不照抄上游含混的 `__fastcall(void*,Rect,int)` typedef。測試直接執行私有原生函式，把一筆 `{0,0,800,600,true}` 寫入合成 dirty vector 並核對 RET20 的棧平衡、Frame 與單位內容不變。

`assist/tests/sidebar_view_runtime.py` 保留原有 20 組完整群組、foreign pair、tracking、SEH/busy、PauseView Capture 等受控測試，新增立即 dirty/redraw/post、same-wake 去重、PauseView 固定模擬幀補畫、真 native dirty ABI、簽名拒絕。DSurface、Windows 派送與 Render 中段仍有明確模型，不冒充實機顯示驗收。

## MO 的縮放架構與現有功能不同

目前功能是 `DDEnableZoom` 加 `ChangeResolution(0x560BF0)`：地圖、側欄與整張原生 UI 一起縮放。MO 是兩個矩形：

```text
A：窗口內實際地圖顯示區，側欄／原生 UI 維持物理尺寸
R：原生光柵視口，R = A × scale，縮放時 R.x = R.y = 0
地圖按 R 畫 → 三趟地圖完成 → R 重採樣至 A → 原生 UI／游標 → Present
物理滑鼠 A → 逆變換至 R → 原生查格／下令
```

縮小視野時，MO 使用 R 大小的兩張離屏面，ZBuffer/ABuffer 也必須至少 R 大；只調 ViewBounds 而保留 A 小的深度緩衝會越界。放大時則必須避免上一幀已放大的像素再進入原生增量拷貝；上游使用乾淨 R 快照還原。全倍率都用獨立 R 雙表面可以避開像素回授，但也是需要完整驗證的新實作，不能直接宣稱與上游逐行相同。

Set_View_Dimensions 必須以 A 執行原生 UI 一次性配置，再把 R 與 `0xB0CE28/2C/30/34` 同步回一致值，並保存／恢復 Tactical `+D64/+D68/+D74/+D78` 四個中心欄位。不然原生查格、hover、渲染與中心會分別使用不同幾何。上游另外明列橡皮框、原生超武清單、tooltip 及 edge-scroll 的 ViewBounds 依賴，不能只改地圖像素。

## 十七個候選位置：原始 ABI 已核對，替代 adapter 尚未驗證

[原始位置證據](evidence/mo-viewport-native-contract.json)由 `mo_viewport_native_contract.py` 在上述私有樣本上執行：逐一核對全部原 bytes、完整指令邊界，再在隔離 Unicorn 執行**原本被覆蓋的指令**，檢查寄存器、棧與固定續接點。tooltip 的虛調用有明確 stub；滑鼠虛表位置另外執行原 `0x7BA360` 本體，驗證原生物理點與 RET4。這是原始 ABI 證據，**不是新 Hook adapter 或視覺效果已獲驗證**。

| 地址 | 候選用途 |
| --- | --- |
| `0x4F4480` | 完整 Render 入口 |
| `0x4F4492`／`0x4F44CB` | 游標擦除用窗口面／回到 map raster |
| `0x4F44FD`／`0x4F4505` | 三趟之間的 chrome pass 暫借窗口面／回到 raster |
| `0x4F451B` | map 完成，恢復窗口面並 R→A 呈現 |
| `0x692B60` | edge-scroll 的物理死區 |
| `0x692300` | 原生查格入口與範圍分類 |
| `0x6930A0` | 滑鼠按鍵消息的 lParam 本地影子 |
| `0x6D7840` | shroud 當前繪圖面 |
| `0x6A7089` | tooltip 處理／繪圖時機 |
| `0x7F7B60` | WWMouse vtable+34，原目標 `0x7BA360` |
| `0x478BD1/0x478BD7/0x478C17/0x478E4A/0x478E50` | 物理 A 的滑鼠／提示框範圍讀取 |

前述候選的 Render 入口與現有側欄追蹤**同為 `0x4F4480`**。兩方各裝一個 JMP 會互相衝突。更重要的是，舊候選在 `0x4F451B` 就把 inRender/depth 清掉；原函式其後還會畫 mouse/text、執行 Present，直到本分支已核對的 `0x4F459D` 才進 epilogue。這會重新引入「Render 尚未完成卻釋放 surface」的問題。

## 可審的整合契約

完整實作應先合併成一個本分支 presentation dispatcher，保留現在 `0x4F4480`／`0x4F459D` 這一對唯一所有權：

| 時機 | 表面與生命週期 |
| --- | --- |
| Render 入口 | depth 增加；確認完整 tracking/phase sites 所有權與 generation，再備份窗口 Composite/Map/current 與 Z/A pair，按需要裝 R |
| cursor／chrome | 暫還窗口 pair；保留同一個外層 Render depth，不宣稱空閒 |
| 三趟 map 完成 `4F451B` | 確認仍為本方 R pair，保存最新 raster 互換次序；恢復窗口 pair，重採樣 R→A；**depth 仍大於零** |
| 完整尾端 `4F459D` | 重放 current-surface 還原；才減少 depth／發布完整 RenderSerial |
| resize／off／stop | depth=0 且兩 tracking sites exact-owned，確認幾何與全部 pointer generation 相符，才還原幾何及回收 |

窗口 pair、R pair、Z/A pair 都要獨立 journal。外部只換一個指標，或 tracking 被平台改寫，不能把未記錄的 Render 當 idle；保留仍可能被引用的己方資源、不寫 foreign pair、不以 stop ack 假裝已清理。任何 SEH 後幾何未知時，也不能把 R 大邏輯視口換回較窄的 saved surface。配置／列索引表準備應在安全點完成，Render callback 不新增引擎分配或長等待。

還需要一份 seqlock 的 A/R transform 同步給 Host。預計介面是物理 A、邏輯 R 兩個 `{x,y,w,h}`、active、serial；尚未加入產品協議。它必須同時接入工具箱主視圖疊加、游標裁切、建造預覽、下令查格及副視圖位置，不能只讓原生 WndProc 看起來正常。原生 WndProc 只能把自己的第 4 個實參指向本次本地 lParam 影子，不能改寫引擎原共享 lParam／WWMouse 物理座標；編程查格必須有「已映射」scope，防止反變換兩次。

[原生投影證據](evidence/mo-viewport-native-projection.json)用原 `6D2140/6D2280/6D6590` 的平地合成矩陣核對 10 組 A/R：A 在 x=0/168、50/75/100/125/150%。映射後都回到原格 `(50,60)`；直接把物理 A 點塞進 R 的查格，會得到其他格。這證實 Host／工具箱輸入映射是必要整合內容，不能留待「之後再修」。該證據不涵蓋高地、坡、橋、native message adapter、實機 DirectDraw 或多人。

本輪仍缺：統一 Render dispatcher 的新 compiled adapter 逐站 replay、全部暫借／還原／foreign-generation 的合併測試，以及所有 Host／程式化查格路徑對 A/R 的端到端整合。因此尚未啟用上述十七處。這些是明確的整合與驗證工作，不是「MO 做不到」或「Hook 一定改程式碼段」的結論；後續應先完成契約，再替換現有縮放。

## 重現

```text
python assist/tests/sidebar_view_runtime.py <private-game.exe> <matching-GameRuntime.dll> --output <sidebar-report.json>
python assist/tests/mo_viewport_native_contract.py <private-game.exe> --output <native-abi-report.json>
python assist/tests/mo_viewport_native_projection.py <private-game.exe> --output <projection-report.json>
```

沒有 Windows 現場視覺、GPU、RA2OL 檢測或雙機同步驗收；所有報告均清楚區分原生重放、模型邊界與尚未實作的替代機制。
