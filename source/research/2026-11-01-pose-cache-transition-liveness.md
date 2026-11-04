# 舊姿態保留造成的 VXL 快取自鎖

新增失敗診斷前的既有小地圖日誌，曾長時間 `render ok=0 / actors=0`，但仍有 decode 耗時。那些日誌沒有精確失敗原因，因此不能據此確認本頁缺陷就是該次實機卡住的根因。本頁另用實際 production SceneRenderer 建立可重現對照，不以單張畫面本來超出預算充作缺陷。

## 確認的必要條件與機制

穩定的光照／palette revision、相同 object identity／type／owner／kind／variant，只有姿態 key 改變。Renderer 將已顯示的舊姿態和新姿態一起加入 wanted，避免解碼尚未完成時缺圖；但普通 live VXL 要求全部當前姿態就緒才繼續發布。若 old + current cache 超過 PosePixels，部分新姿態入庫後會在 `VoxelPoseBudget` 返回，尚未到更新 displayed 的迴圈。

結果是舊 displayed 始終被當成 wanted，重試的逐出邏輯也不能移除它們；即使當前畫面本身完全放得下，仍無法完成。移出視野、改變 palette／lighting revision、reset 等其他清理可能打破此狀態，所以不是任何條件下都永遠失敗；但相同條件下單純重試不會自行恢復。

## 修復前的受控證據

outside-checkout fixture 使用真實 SceneRenderer 與 72 個合成 VXL 素材，每個姿態 512×512 RGBA，正常完整輸出為 120×60：

| 項目 | 計數（pixel 單位，不是 byte） |
| --- | ---: |
| 每張當前畫面 | 18,874,368（18 Mi） |
| PosePixels 限額 | 33,554,432（32 Mi） |
| 舊＋新姿態 | 37,748,736（36 Mi） |

舊畫面先成功；更換姿態後 cache 固定在限額，連續 64 次都是 `VoxelPoseBudget`、completed actors=0。**相同的新畫面，用全新 renderer 能完整成功顯示 72 個物件。** 這個控制組排除了單張畫面本來就超預算的解釋。

原始重現程式、log 與當時 source hashes 保留於本機 `work/ra2-hook-audit/diagnostics/pose-budget-liveness/pose_budget_liveness.cpp`、`test.log`、`source-hashes.json`。沒有使用私有遊戲素材、啟動遊戲或修改遊戲記憶體。

## 最小修復

只有普通 live VXL 即將因像素預算失敗時，才從本次 markers 建立 current＋frameFrom 必需 key 集合。正常畫面不增加這個集合或額外完整複製。

壓力路徑一次掃描快取，移除不在必需集合、art 可確認是 VXL、且 `kind < 5` 的舊姿態，再清理指向已移除項目的 displayed 映射。這些普通 VXL 原本就不允許以舊姿態完成新畫面，因此不再讓它們的備援保留阻擋整個轉換。當前 key 受保護，插入中的 map reference 不會失效。

SHP 仍可能因解碼時間預算依賴舊畫面；暫態 VXL 的原流程也允許當前素材失敗後使用舊圖。因此兩者都不在這次逐出範圍。frameFrom、真正當前姿態及不可辨識 art 同樣保留。若退掉可捨棄項目後仍超額，維持原本拒絕，不跳過常駐物件、不降解析度、不降低刷新率，也不增加預算。

既有發布的 CPU 成圖不依賴 renderer map；GPU／world packet 的不可變 Image 另有 shared_ptr 持有。移除 renderer 的 cache 引用不修改這些已發布資料。

## 修復後驗證

`source/assist/tests/pose_cache_liveness.cpp` 直接使用 production SceneRenderer，x64：

- 同一 72 物件場景，第一次姿態切換及後續 64 次重試全部完成，完整 RGBA 與 fresh renderer 相同，舊 CPU 成圖不變。
- 混合 SHP 舊姿態、可 fallback 的暫態 VXL、必要 frameFrom：74 個物件全部繪製；下一次 cache 計數精確保留 72 current＋1 frameFrom＋2 fallback。
- 真正單張畫面需要 129 份素材、超過 32 Mi pixels 時，仍返回 `VoxelPoseBudget`。
- 持有舊 world packet，在新姿態完成並逐出舊 cache 後，其 Image 指標與像素仍保持原值。

結果：全部 PASS，log 位於同 evidence 目錄的 `after-test.log`。本輪修復快照 radar_scene SHA-256：`ee26218f2b727b38458c00f1e2f4a3e3b32dc434556e66a7913e894d415db2dc`；fixture SHA-256：`87d7f6eb01df5ce1dfcdde8d5827bd10adde1db2827196f92ba5a2f46a7edb96`。

這證明快取狀態機缺陷與修復，不是 VXL 原生畫質、實機 FPS 或舊日誌原因的驗收。新版本的 failure reason 日誌仍需用來判斷實際對局是否命中這個分支。
