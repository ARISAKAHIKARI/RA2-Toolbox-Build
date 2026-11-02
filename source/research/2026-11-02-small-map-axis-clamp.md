# 小圖單軸視野固定與原生拖屏

問題：地圖只有左右或只有上下小於視窗時，原生拖屏仍反覆夾到兩邊而閃爍；只有工具箱小地圖定位的本地 clamp 不會修復遊戲自己的拖屏呼叫者。

已讀取原始碼 `MagicShiba/MO_ExperienceEnhancements@33ed558bd38f36b5012eddcc9c0fb109cc68e38a`：`src/gui/ScaleBar.cpp` 的 `SB_MapPx`／`SB_ClampFix`（1520–1605）分別判定 `degX`、`degY`，小於視口的軸固定地圖自身正中，另一軸保持原生區間。`src/gui/VecText.cpp` 2730–2745 只在縮放功能啟用時替換 `0x6D8640`，並非只處理兩軸都退化。

本分支原有 `smallcamera::clamp` 已分軸，但普通產品的 `smallMapCameraTick` 無條件還原後返回，只有診斷模式才會安裝。這就是新功能沒有作用到原生拖屏的確定程式碼原因。另原先 `(min+max)/2` 在奇數視口上會偏移一個像素；現在以地圖自身像素矩形算中心，與上游一致。

改動：新增獨立預設關閉的「小地圖相機邊界修正」實驗開關，明示高危原生入口。普通產品以 Shared `smallMapClamp` 請求；任一軸退化即透過既有統一管理器替換 Clamp_Center，單軸／雙軸皆可。大圖、恰好等尺寸或關閉時還原本模組擁有的入口；不覆蓋 CnCNet／Phobos／平台既有相機 Hook。診斷版仍由獨立測試 channel 控制，普通設定不會破壞零補丁基線。縮放及側欄改變的目前視口尺寸每次重新判斷。

Shared 狀態：0關閉且已還原；1單軸／雙軸修正已安裝；2目前不需修正；3等待有效幾何；-2入口／版本衝突或還原尚未完成。沒有改地圖探索、事件、Owner、CRC、RNG 或單位狀態。

原生實證：本輪私有樣本 `D:/Ra2Game412/gamemd-spawn.exe` 實際 SHA-256 為 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，測前後一致；不能沿用上一輪其他檔案的247f…雜湊。`0x6D8640` 190 bytes 的入口為 `A1 E4 F8 87 00`，入口後185 bytes FNV為 `A41008F5`，與既有 manager 的 CameraClamp ABI探針一致。

- 唯一參數是 Point*，原生及替代均彈出4 bytes，ECX不用；測試確認ESP與非揮發暫存器。
- 輸入欄位：`87F8DC` 地圖基準寬，`87F8E4/E8/EC/F0` 可視格矩形，`886FA8/AC` 目前視口寬高。
- 原生地圖像素左／上為 `30*(2*x-mapWidth)`、`15*(2*y+mapWidth-5)`；寬／高為 `60*width`、`30*height+135`。非退化軸照視口半寬高產生原生min/max；退化軸固定地圖自身中心。

`assist/tests/small_map_camera_axes.py` 把 production C++ helper／bridge 編成x86和x64，並在唯讀原版映像的x86模擬中實際執行原生Clamp_Center。每架構1960個案例：147水平單軸、490垂直單軸、539大圖／等尺寸、490奇數視口；正常軸結果與原生完全相同，退化軸固定，重複套用不再改動。另24項實際C++ bridge回歸涵蓋預設零補丁、正式版opt-in、視口尺寸變化、側欄1152→1280門檻、關閉／stop、無效幾何、Capture拒絕及外部所有者衝突。這是視口輸入的互動測試，沒有執行真CnC-DDraw縮放切換。結果：`diagnostics/placement-view-hotfix/small-map-axes/result.json`。

`assist/tests/small_map_camera_runtime.py` 另用最終編譯DLL驗證真管理器安裝、原生入口跳轉、單軸／雙軸、幾何變動還原、預設不安裝及平台既有Hook拒絕覆蓋。由最終整體建置執行並保存報告。

這些是幾何、ABI與受控執行證據，不是Windows實際拖屏畫面或RA2OL雙機驗收。Library SQLite在此環境不可用，本記錄保留供後續回填。
