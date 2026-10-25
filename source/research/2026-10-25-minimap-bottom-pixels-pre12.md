# pre.12 小地圖南緣多餘像素與採樣閃爍

## 問題與依據

使用者回報實景小地圖底端多繪製遊戲中未見的格子，並且每幾秒單位會閃一次。本次先查 `INDEX.md`、持久 SQLite v46 的 pre.5／pre.6／pre.9 邊界筆記，再檢查目前 `Sampler`、`SceneRenderer`、`NativeCanvas` 和工作線程。支援的原版映像 SHA-256 為 `7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`；本次沒有增加原版指令結論。

已有原版證據：`0x6D5226` 的地形入口用 `0x568300 CoordinatesLegal`，不以 `0x578540 IsWithinUsableArea` 裁切。pre.9 又驗證 Cell `+0x11B` 為有符號高度、原生 TMP 相位以及 `0x87F7E8 +0x1488/+0x14A4/+0x14A8` 的精確雷達跨度。故不能把南部所有 non-usable 格子刪掉，也不能把 ceil backing 最後一行當作地圖面積。

針對既有私人 pre.9 capture 的 `01-off`／`02-on`／`03-off` 做唯讀南緣 120 原生像素欄位盤點：三個階段一致，取景底帶內有 278 個有效 tile 格與 522 個 `tile=65535` 原版 fallback 格屬 non-usable；名義 TMP 足跡跨越輸出底邊的 55 個有效 tile 格與 143 個 fallback 格也都屬 non-usable。這些格的 Cell `+0x120` 都不是 `-1`，即處於黑幕下；fallback `0xAA10B0` 指向合法 tile 0。這是 capture 欄位與名義 TMP 輪廓盤點，沒有執行完整原版像素合成；它並非使用者新截圖的同一場景，不能據此認定新畫面中哪個格子多出來。開啟本工具的無黑幕選項會依既定策略顯示這些實體合法格；原生黑幕模式會排除未探索格。

## 本工具已確認的像素缺陷及修正

`radar_map.h::Sampler::step` 接受有顏色的合法 Cell，但 tile vector 暫時未就緒、索引無效或素材類型為零時，`CellArt.type` 會是 0。舊 `radar_scene.h::SceneRenderer` 無論 type 是否存在，都用 `pending_.diamond` 畫一塊彩色替代地表；該 type=0 格從未加入 incomplete 重試，因此可以永久形成假南緣。現在只在 `cell.type!=0` 時載入或繪製 TMP，缺資產但類型有效時仍保留重試用替代菱形；overlay 的處理不變。這是本工具源碼可重現的缺陷，不宣稱既有 capture 的南緣就是 type=0：其底帶沒有無效 tile 索引。

`radar_scene.cpp` 的受控南緣格測試把物理合法、玩法 non-usable 的 Cell 放在最後 15 個原生像素行：type=0 不產生地表；type=1 的真實素材仍畫到矩形底邊。`radar_boundary.cpp` 檢查同一格在原生黑幕下排除、顯式 reveal 下顯示。這兩個 portable 測試不是 Windows 實機截圖對位。既有原生跨度與高度測試仍通過。

單位閃爍另有純工具端原因：向量 header／payload 讀取失敗以前等同真正空列表；跨兩幀的採樣以前會清空所有標記；同世界渲染暫時失敗以前會丟掉已完成的畫面。現在單位列表提供有效性、失敗採樣不發布空標記、同一 reveal／幾何／輸出尺寸保留最後完成的 scene。`publicationContext` 也區分讀取失敗與確認切換世界，短暫失讀不清空上一張畫面。真正讀到 count=0 仍移除已死亡單位。`minimap_rate_test.py` 和 `minimap_overlay.cpp` 覆蓋這些轉換。

## 尚待同場景檢驗

若用戶新畫面在原生黑幕模式仍露出多餘 TMP，需對應同一場景的底邊格 Cell `+0x120`、tile 索引、有效素材像素和原版／工具畫面，再定位是否為縮放、資產或投影差異。現有資料支持修復 type=0 假地表，不支持裁去所有 physical-legal non-usable 邊緣。

逐個單位的 live／type／owner 欄位仍以預設值處理個別失讀，可能造成單個圖標短暫缺席；本輪處理的是整個向量與發布狀態失讀造成的全體閃爍。若仍有孤立圖標閃爍，應對必需欄位單獨建立讀取有效性判斷，避免把可選姿態欄位的失讀當作整幀失效。
