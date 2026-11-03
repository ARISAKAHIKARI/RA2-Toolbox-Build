# 原生放置的高地底邊檢查與待確認產品重複啟動

基線 5983b943e90694f7b2630608bc464d6f8ee148e3；支援私有 EXE SHA-256 268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0。先查 INDEX、2026-11-01-placement-event-reception.md 及 YRpp ObjectTypeClass/GetFoundationData： https://github.com/Ares-Developers/YRpp/blob/master/ObjectTypeClass.h 。未接觸遊戲進程，Library notebook 不可用，保留此記錄回填。

## 實際差異

Place 接收方 4FB0E0→440580→6F6CA0→5F4EC0→449440→464AC0→716150→47C620，對地圖格以 578540(Cell*,true) 檢查。上一輪12個接收實驗把該函式作明示替身，不能據此斷言與工具公式一致。這次原始指令加追蹤，15次 native 呼叫參數 true，仍保留除 map-range 以外的地形／佔用 native 檢查；診斷 next-ui-ore-view/placement_boundary_probe.py。

工具箱 targetUsable 調用 mapbounds::Bounds::usable，但後者先做 CoordinatesLegal(568300) 的底邊 x+y<=width+2*height，578540(true) 本身允許到 width+2*(top+rows)+2+level，並在頂邊考慮坡度。當 VisibleRect 觸及地圖底邊時，前者錯誤拒絕原生允許的格子。合成 full map 64×64、Cell (104,89) 的原生 578540(true) 返回真，但原工具為假；此條件可隨level延伸。對中心縮小的 rectangle 67,625 組和全範圍邊界 89,225 組，執行支援樣本的真578540，固定多個level和slope值。可重跑 source/assist/tests/placement_usable_native.py；正式報告 source/research/evidence/placement-height-boundary.json（舊判斷錯拒 3,515 組，新公式 0 差異）；兩者是 synthetic map cell 對照，不等於真地圖這些槽已分配。

修正只針對 native Place：先保留正數／512上限、可讀 Cell 指標、Cell座標吻合、同一陣列／範圍的前後身份校驗，再按原生578540(true)的邊界公式評估；其他原生地形、佔用、地基、工廠及接收檢查不改，renderer用途的通用 Bounds::usable 未改。native_placement_bridge.cpp 隔離測試涵蓋普通底邊、level=6高地差異及錯誤Cell座標拒絕。

## 事件入隊後按鍵重啟

已排隊 Place 仍未由遊戲處理時，Factory Product 可能保持完成且在 Limbo。原 Host offers 和 Runtime arm 沒排除已有 completion receipt 的同一 Product；使用快捷鍵可再次啟動相同放置，但真正落地後重試自然無效。新守衛以 match、House UID、Product pointer/UID及Type pointer/UID識別待確認產品，從 Host offers 隱藏、拒絕互動與一次性重複提交；其餘產品不被影響。10秒期限／換局／成功落地由既有 observer 管理。本項只證明一條可重現的重入路徑，是否涵蓋使用者 Q 現象還需實機核驗；沒有添加無根據的全局Q攔截或清除他人原生游標。

受控 C++ 生產標頭與真 enqueue 測試原有 114 checks，並新增 6 項縮放座標、過期世代、查高度時切換視角及互動放置測試，共 120 checks PASS；不證明完整遊戲地圖、實際畫面、RA2OL雙機或同步。

## 原生 Q／W 快取的實際根因

根據支援映像的 SetStructureTabCommandClass 虛表，Q 的原生 action 535D90 執行建築欄路徑並呼叫 7342A0，取出 B0FE5C 的最近完成建築；W 的對應 getter 7342B0 取 B0FE60 防禦快取。側欄 6A8E48→734250 依 BuildingType+E08 的 BuildCat=5 區分 W 與 Q。遊戲原生 MouseUp 在 4ABBD7 呼叫 734270(product) 清掉快取，直接發 Place 事件不經此 MouseUp。這是原生 Q 再顯示已完成建築的具體原因，與先前工具箱待確認重入是兩條不同路徑。

734270 的 thiscall ECX 為產品指標；非零時只清與該指標相等的 Q 或 W 快取，若兩槽皆相同，一次僅清一槽；ECX=0 則同時清兩槽。原函式 734270..73429C 45 bytes FNV-1a 61003681。工具現在僅在原產品 UID、Type、Owner、存活、OnMap 且非Limbo得到確認後，且 native工具空閒、原簽名匹配時，以非零產品指標呼叫此函式；外來產品保留，讀取或簽名異常保留receipt重試。兩槽同指標需兩次安全 tick。這不發假按鍵、不重新下事件、不改遊戲指令。

原始 734250/734270/7342A0/7342B0 在合成記憶體受控執行七組：建築/防禦分流、兩種單側清理、不同產品保留、null會清兩槽、雙槽相同需兩次。見 [placement-quick-native.json](evidence/placement-quick-native.json) 及 assist/tests/placement_quick_native.py。獨立 C++ 生產路徑再驗成功之前不清、成功後只清同產品、不同產品保留、簽名短讀不呼叫以及恢復重試。這些不等於已在 RA2OL 現場驗證。
