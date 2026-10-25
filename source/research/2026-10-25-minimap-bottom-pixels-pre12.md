# pre.12 原生雷達底邊與小地圖完整幀

## 問題、原版範圍與方法

使用者確認：本工具實景小地圖比遊戲原生雷達多顯示南邊幾行；原版在玩法範圍接近全圖邊界時會向內保留最小距離。先查研究 INDEX、持久 SQLite v46 的 pre.5／pre.6／pre.9 筆記，再對支援映像的私人 capture 反組譯與受控重放。支援的原版映像 SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。私人捕獲不入庫；本記錄的座標與計數只代表該捕獲，不當作使用者新畫面的原始資料。

## 原版的兩層邊界

`MapClass::SetVisibleRect @0x567230` 是 thiscall，ECX 為 Map，stack 傳入矩形指標，`ret 4`。先於 `0x421B60` 取交集，再於 `0x56726E..0x5672D3` 強制 live VisibleRect 左／上至少 2 格，右至少 2 格，南至少 6 格：

```text
left = max(clipped.left, 2)
top = max(clipped.top, 2)
columns = min(clipped.columns, MapRect.width - left - 2)
rows = min(clipped.rows, MapRect.height - top - 6)
```

這些結果已寫在 Map `+0xFC/+0x100/+0x104/+0x108`，外部讀到的 live 矩形不可再向內縮第二次。`native_visible_inset.py` 對三個 capture stage 各跑 13 個矩形，39 組與原指令零差異；原版片段 SHA-256 與合成矩形見 `verification/native-visible-inset-pre12.json`。例如 30×40 全圖請求變成 `{2,2,26,32}`，四邊恰為 2／2／2／6。

`RadarClass::SetVisibleRect @0x654490` 先呼叫上述函式，再遍歷 `IsWithinUsableArea(CellStruct*, true) @0x578460` 求 Radar offset `+0x1490/+0x1498` 與輸出尺寸 `+0x14A4/+0x14A8`。原生 RGB 底圖的整數寬高另存於 Map/Radar `+0x1240/+0x1244`；配置路徑 `0x65486B..0x654877` 寫入兩欄。私人 capture 的 live Rect `{3,5,124,135}`、MapRect `131×146` 已有南差 6；raw 為 `248×276`，生成雷達面為 `97×108`，倍率約 `0.39130434`。從生成尺寸除以浮點倍率反推 raw 邊端會產生誤差。

## 多出南行的直接原因

原版全幀底色 `RadarClass::FillTerrainColors @0x654EA0..0x655041` 是 thiscall、stack 兩個矩形指標、`ret 8`。它遍歷實體 Cell，於 `0x654F20..0x654F4F` 先把未扣高度的平面格矩形裁至 raw RGB 區：

```text
flatX = cellX - cellY + radarOffsetX
flatY = cellX + cellY - radarOffsetY
cellRect = [flatX, flatX + 2) × [flatY, flatY + 1)
rawRect = [0, rawWidth) × [0, rawHeight)
```

只有交集非空才呼叫 `Cell::RadarColor @0x47C060` 並寫入底圖 `+0x123C`。原始 `0x421B60` 交集函式受控端點重放確認 x=-2 拒絕、-1 接受，x=rawWidth-1 接受、rawWidth 拒絕；y=-1 拒絕、0 接受，rawHeight-1 接受、rawHeight 拒絕。原版增量更新 `@0x655250` 另在 `0x655291` 用 mode 1 的 `0x578460` 篩選 Cell；不能把增量規則冒充全幀逐格遮罩。此前 pre.6 的 `Tactical @0x6D5226` 只用 `CoordinatesLegal @0x568300`，也不能代替 Radar 全幀准入。

工具此前按 signed Level 先將 TMP 提升，再讓畫布按生成尺寸／倍率裁切；flatY 超出 rawHeight 的格雖應被原版底圖拒絕，Level=2 卻使 TMP 上移 30 原生像素，落入我們的南緣。既有 capture 中，按合法 Cell、座標身分、Level=2、60×30 名義 TMP 與 raw 底線縱向跨越且橫向相交的條件，可重現 **99 格**（fallback tile 71，非 fallback 28）；全數 flatY=277、non-usable，會被原版 flat 准入排除。這是名義素材矩形統計，不是實際不透明像素或使用者新畫面的計數；sanitize 結果見 `verification/native-radar-boundary.json`。新 `Geometry::flatVisible` 先做 2×1 raw 半開交集，兩種黑幕模式均適用；signed Level 只留給後續 TMP 繪製。`Geometry` 身分、完成幀發佈、實景原尺寸、示意圖與導航改用實讀 raw `+0x1240/+0x1244`，raw 暫時失讀時不發佈半成品，保留已驗證的同世界畫面。

原版初填確有落於 raw 矩形內的合法但 non-usable 格：私人 capture 有 662 格。它們的 Cell `+0x120` 在該場景均未探索，原生黑幕使玩家看不見。工具在顯式無黑幕選項下另以 live `Bounds::usable(level,slope)` 限制揭露範圍，避免把原版未探索且不屬玩法區的邊緣格顯出來；這是本工具對使用者可視範圍的顯示策略。原生黑幕模式仍可顯示已探索、在 raw 內的實體合法邊緣，不對全圖套 usable 裁切。

另一個已確認的工具像素缺陷是 `CellArt.type=0` 時仍畫無來源的替代菱形；現在只對實際解析到的 tile 類型畫素材或暫時替代。這條路徑在上述私人 capture 的南緣沒有無效 tile 索引，因此不作為該 capture 的成因。

## 回歸、閃爍與未決

`radar_boundary.cpp` 測 raw 248×276 的四側半開端點、flatY=275/276 與 Level=2 向上進畫布的反例，另以四側 2／2／2／6 的合成 live Rect 測無黑幕玩法邊界、raw 內 non-usable 的已探索／未探索格。`minimap_overlay.cpp` 測 raw 尺寸參與幾何和發佈身分、暫時失讀、縮界與導航；`radar_scene.cpp` 測 type=0 缺圖格不生假底邊。原版 inset 受控重放與原生 TMP/座標測試另驗證各自的指令片段。這些 host 回歸不能當作使用者同場景 Windows 截圖對位。

單位整體閃爍方面，checked actor vector 將讀取失敗與有效 count=0 分開；跨幀採樣、原尺寸渲染暫時失敗、原生 raw 欄位或 publication context 暫時失讀時保留最後完整畫面，確認換世界／結束才清圖。`minimap_rate_test.py` 驗證暫失讀、真正空列表和換世界。個別 actor 的 live/type/owner 必需欄位仍可能失讀而讓單個圖標短暫消失，需另行建立狹窄的有效性判斷。

若新版本在使用者同一場景、同一黑幕選項仍有多餘南行，需同場景原版雷達和本工具畫面，連同 live Rect、raw/generation 尺寸、底邊 Cell 座標／Level／visibility／tile 索引，才能繼續定位實際素材透明度或最後呈現差異；不需要提供私有 EXE 或完整記憶體。
