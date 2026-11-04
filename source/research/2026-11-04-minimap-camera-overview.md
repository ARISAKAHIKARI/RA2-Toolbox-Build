# 小地圖頂部裁切與底部導航不完整

基線：`main@1e52021a20f9372727a3df7ea748610c533ea7a3`（含 1.0.41-pre.5）。使用者提供兩張 506×275 小地圖畫面：上方視野框遭裁切；移至底部後框底仍與面板底緣留距。截圖沒有當場記憶體欄位，不能由此斷言使用者當時的縮放或側欄狀態。

先查 `INDEX.md`、持久筆記本 v126 及 [Tactical 邊緣](2026-10-25-renderable-map-borders.md)、[Radar raw 邊界](2026-10-25-minimap-bottom-pixels-pre12.md)、[活動投影](2026-11-02-trigger-radar-active-projection.md)。本輪為現行來源檢查及工具箱受控 fixture，沒有重讀私人遊戲映像或操作遊戲進程。既有原指令依據的支援映像 SHA-256 為 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；活動投影後續樣本為 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。不把既有原指令測量改稱本輪實測。

## 兩處來源問題

1. `minimap_overlay.h` 的導航與框繪製仍用 `snap.width-160`、`snap.height` 和較舊的 snapshot camera。它們忽略真正 Tactical 視口 `0x886FA0` 的 x/y/width/height；縮放時應用 Host 發布的邏輯 R 尺寸。沿用 `nativeviewhost::sample()`，雙讀 Tactical `+0xB0/+0xB4` 相機及視口，檢查 Host transform 世代、目前 House 與 live bounds。目的端 x/y 不再加到世界相機原點。
2. 小地圖沿用 Radar 的 raw crop，但 Tactical 相機完整可見範圍與它不同。以 64×64、live `{2,2,60,56}` 的已知平坦案例：Radar 世界區為 `[-1770,1830]×[1035,2745]`，既有 `cameraExtent()` 為 `[-1800,1800]×[945,2760]`。因此只修視口尺寸，仍不能補回最上方。單純取兩個區域的聯集又可能留下相機永遠到不了的底部區域。

`MapClass::SetVisibleRect @0x567230` 已規範化 live `+0xFC/+0x100/+0x104/+0x108`；不二次內縮。相機限位 `0x6D8640` 及 `Tactical::SetTacticalPosition @0x6D6000` 沿用既有行為；本次不改遊戲 `.text`、可走區或原生相機。

## 修正範圍

小地圖專屬 `tacticalOverviewProjection()` 精確使用既有 `cameraExtent()`：`offsetX=W-2*left`、`offsetY=W+2*top-5`、`rawWidth=2*columns`、`rawHeight=2*rows+9`。這是工具箱面板的呈現策略，**不是聲稱原生 Radar 使用這個 crop**。`Sampler::step()` 以明確額外參數選入；原本 native backing 及 ActiveProjection 預設行為保留。實際 `+0x1240/+0x1244` allocation 與 native offset 仍作來源身分，調整後 bank 不發布到共用 SceneBank。

`minimap_viewport.h` 共用實讀視口於導航和 `mainview::crop()` 框；GDI/GPU 框的 right/bottom 是排除端，正規化一應到面板內緣排除端，修正原本少一像素。讀取中斷、跨 House、同 frame 改界或 transform 不穩定時不產生猜測座標。

## 驗證與限制

`assist/tests/minimap_viewport.cpp` 使用真正視口採樣、投影與導航 helper，保留固定減160錯誤的反例，覆蓋側欄、目的端原點、奇數尺寸、邏輯縮放、排除端及不穩定讀取。

`assist/tests/minimap_overview.cpp` 使用真正 Sampler/SceneRenderer/發布判定；測出 native crop 不含而 overview 可畫出的北緣 TMP 像素，覆蓋上下相機端點、奇數視口、原生來源身分、私人 bank、同 frame 劇情改界和原投影不變。這些是受控記憶體／資產替身的工具箱行為，不是使用者場景的 Windows 截圖、真實 GPU 或雙機聯機驗收。

本輪新版本為 `1.0.41-pre.6`；精簡的配套 Windows 編譯流程為 `.github/workflows/compile-minimap-hotfix.yml`。兩項蜘蛛自動功能仍暫停；本次修正不聲稱解決此前仍未經使用者驗收的注入問題。

## 配套編譯與交付

Windows x64 的兩項 focused fixture、完整 Host/x86 Runtime/broker 編譯及 PE/ABI/版本檢查通過。產品來源為 `f7c28255b1bd90b3537ffc063c201aceaaebf19d`，Actions run `41859071422`、artifact `13339713279`。更新包 `RA2Toolbox-1.0.41-pre.6-x64-Update.zip` 為 3,590,025 bytes，SHA-256 `5a75a849368b480f5e2f9fdbd86424a96145930175df762e1bb568a5c63882fc`。

額外嘗試的舊 `minimap_overlay.cpp` 在第138行 catalogue 斷言失敗；隔離回置 main 的原始 radar_map blob 後同樣失敗，已確認為本輪之前存在的 fixture 失敗，不把它列為通過。新兩項 fixture 及本輪實際 Tactical 模組交叉編譯均通過。此文件檢查點僅補交付證據，不變更已編譯產品程式碼。
