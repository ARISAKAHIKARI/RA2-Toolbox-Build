# 小窗與小地圖的實時地圖可視邊界

## 問題與證據

使用者指出局部觀察窗和小地圖缺少地圖邊界處理，並提醒劇情觸發會改變地圖可視範圍。調查前已讀 INDEX、既有雷達／小窗及小地圖相機程式，並檢索同一持久 SQLite 中 MapClass、MapRect、VisibleRect、地圖邊界、87F7E8 與 6557F0 的紀錄。

支援的原版映像 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。使用私人 capture `RA2Master-Capture-20261025-003340-3568` 的 `02-on` 有效 bytes；不將 EXE、原始記憶體或遊戲素材放入 repository。公開 YRpp 的 [MapClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/MapClass.h) 與 [RadarClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/RadarClass.h) 提供名稱和入口線索，以下判斷另外核對捕獲的原指令。

| 地址／欄位 | 原版用途與證據 |
| --- | --- |
| Map `0x87F7E8 +0xEC..0xF8` | MapRect；全圖寬／高為 `+0xF4/+0xF8` |
| Map `+0xFC/+0x100/+0x104/+0x108` | 當前 VisibleRect 的 X、Y、Width、Height；不能用初始 INI 或整圖尺寸替代 |
| `0x568300` | thiscall `CoordinatesLegal(CellStruct*)`，AL 結果、ret 4，依全圖菱形範圍判斷 |
| `0x578540` | thiscall `IsWithinUsableArea(CellClass*,bool checkLevel)`，AL 結果、ret 8，使用 VisibleRect、格高 `+0x11B` 與頂邊坡面位元組 `+0x11C` |
| `0x578460` | 同一可視範圍判斷的 CellStruct overload；查表後使用同樣高程條件 |
| `0x567230` | SetVisibleRect 的 Map 基底實作；將矩形寫入上述 live 欄位，要求重畫並更新物件可用區判斷 |
| `0x654490` | Radar override 先呼叫 `0x567230`，再遍歷當前可用格、重算雷達 offset／矩形 |
| `0x6557F0` | 既有雷達座標投影；沿用 offset `+0x1490/+0x1498`、factor `+0x1488` 與尺寸 `+0x14A4/+0x14A8` |
| `0x6D8640` | 相機中心限制；沿用已驗證的 `assist/source/small_map_camera.h` 公式與小圖中點修正，本輪沒有再次聲稱完整相機函式實機驗收 |

捕獲快照的 MapRect 是 `{0,0,131,146}`，VisibleRect 是 `{3,5,124,135}`。這是該捕獲的值，沒有假設它與使用者新截圖必然來自同一對局。

## 根因

小窗原先只檢查 512×512 儲存陣列索引，沒有排除仍可讀但已在物理地圖／當前可視範圍外的格子。雷達雖有全圖菱形 `legal` 判斷，但沒有讀取 VisibleRect。幾何快取與完成畫面發布檢查亦未包含 VisibleRect，因此同一場對局、相同 cell 指標和 frame 計數下的縮界／擴界不一定淘汰舊畫面。

單純將相機座標限制在 0..511 不符合原版：這是儲存陣列，並非地圖形狀。只讀 MapRect 或初始地圖檔也無法處理使用者提醒的觸發改界。

## 實作

`map_bounds.h` 共用 live MapRect／VisibleRect 讀取、合法格與可用格判斷。矩形讀取兩次一致且尺寸有效才接受；未知／混合資料不被默認為全圖可見。正式取樣可能經頁快取，因此這項重讀本身不代表兩次 uncached 觀察；watch 和 radar 另外在整批渲染完成後用 readFresh 檢查當前邊界，並以真實 worker 測試採集後同 frame 縮界的拒絕。高程和頂邊坡面規則保持原版嚴格／非嚴格不等號。

- 小窗取樣同時限制物理地圖與當前可用格；refresh 即使沒有新的 simulation frame，也比較 live bounds 並要求重取。
- 小窗相機中心依原版可視區限制；取樣區也反投影到受限後的相機中心，另存 focus cell 供 refresh 判斷。角落反例有 12 個視窗內可用格超出舊 focus-centered radius 20，修正後全數覆蓋。整張可視區小於視窗時固定於中點。輸出裁去地圖範圍外邊緣。此處調整相機**位置限制**，不改角度、方向或 2:1 投影。
- 小地圖只納入目前可用格、物件和動畫；Geometry identity 包含 bounds。縮／擴界會使採樣 bank 和實景背景失效，已解碼素材可重用。
- 非同步發布再以 fresh read 比較當前 bounds；舊可視範圍的耗時渲染不能在觸發後重新發布。
- 雷達點擊／拖拽的相機中心也依當前可視範圍限制，並沿用小圖中點處理。

目標越過目前可視區不等於死亡；跟蹤槽位仍由其真正生命週期控制。本輪與三窗穩定修復整合，不能把短暫取樣失敗重新當成窗口刪除。

## 驗證與界限

`assist/tests/map_bounds_native.py` 在 Unicorn 執行捕獲的原版 `0x568300` 和 `0x578540`，並把同一組輸入送到實際編譯的 C++ `map_bounds.h`。四種全圖／可視矩形、五種格高、坡面開關、每條不等式邊界及隨機格，共 **31,640 組**；物理合法性及合併可用性均 **零差異**。結果與原指令區段 SHA 收錄 `verification/map-bounds-native.json`。這是受控記憶體中的原指令對照，沒有執行遊戲的觸發器或完整繪圖器。

Portable 回歸涵蓋：

- `map_bounds.cpp`：嚴格邊界、高程／坡面、相機極值、小地圖中點、邊緣清除及混合矩形拒絕；相機結果與既有 small_map_camera helper 一致。
- `watch_view.cpp`：同一目標、cell 指標和 frame 下縮界／擴界；reveal 也不能顯示物理地圖外格；clamp 後的角落視域完整覆蓋，包括舊採樣區漏掉的 12 格。
- `minimap_overlay.cpp`：同一 house、cell 指標和 frame 下舊發布拒絕、縮界 bank 清空及擴界恢復。
- `radar_scene.cpp`：輸出尺寸不變的縮界淘汰舊背景，擴界可重用 tile art。

未宣稱已在完整 Windows 遊戲執行劇情觸發、比對每個斜坡邊緣像素、測量 FPS 或雙機聯機。相機位置限制沿用原版公式，完整實景仍有既有素材／深度與 MOD 擴展的驗收界限。
