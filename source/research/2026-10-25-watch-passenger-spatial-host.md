# 跟蹤步兵入舱／駐軍後場景停止更新，以及不可逆誤清槽

## 問題、基線與證據界限

使用者先回報左側跟蹤窗口無故消失，隨後給出明確復現：步兵進入建築或載具後，觀察窗停止更新。基線為 `v1.0.38-pre.7` 原始碼提交 `04fd222f0912f0c0c7c55d3f694c6018c6c9986a`。

調查前已查閱研究 INDEX、`2026-10-25-watch-layout-lifetime.md`、pre.13 跟蹤組筆記，以及持久 SQLite 的對應條目。沿用支援映像 SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。這次有現行生產碼檢查、構造記憶體中的真實 renderer／worker 執行、私人 capture 原始指令反彙編及欄位選擇 slice 受控執行；沒有遊戲實戰錄影、完整原版進出樓／舱執行、Windows 合成或多人聯機驗收。

## 可重現根因

`watch::follow` 已保留仍存活的 limbo／乘員身份，但 `watch::refresh` 和 `watch::sample` 都以 `valid(focus)` 作為整個場景的准入條件。`valid` 要求焦點自己在地圖上、非 limbo、沒有 Transporter。步兵入舱或駐軍後這些條件必然不成立，因此生命週期保留了窗口，renderer 卻只能重放入舱前的整張 Frame，旁邊仍在移動的單位也停止更新。

用同一受控記憶體先取得完整畫面，再把原步兵切為 limbo/off-map 並設置載具關聯，舊生產碼保持 `shown=true`、Track 存在，但 `view.tick` 不再前進。舊碼斷言 `boarding must continue sampling the scene at its last known center` 失敗；修正後同一案例通過。新 `watch_passenger_test.py` 另驗證完整宿主路徑與場景更新，不以單純窗口仍存在冒充畫面更新。

另找到兩個可造成永久誤清槽的分支：一次成功但互相矛盾的 vtable／ID／Health／Alive 讀值會直接提交死亡；真正死亡後，原組接替者任一生命欄位失讀，`valid` 會把它當成不存在，因而清槽。新增回歸在舊 `watch_slots.h` 失敗，修正後通過。這證明程式存在此類錯誤路徑，沒有證據把使用者每一次「無故被殺」都歸於同一底層讀取原因。

## 跟蹤身份與取景宿主

新共用 `watch_focus.h` 分開原始 `Target` 身份與 `Anchor` 空間來源。原步兵 pointer＋ID、原選中組、窗口槽位均保持；宿主只是取景中心，不加入死亡接替組。

- 步兵可見時由自己供應坐標；出舱／出樓後自動回到同一原步兵。
- 載具內使用步兵的 `Transporter +0x11C`，並由載具 `Passengers.Count +0x114`、首節點 `+0x118`、乘員 `NextObject +0x30` 反向確認；所有成員的 Transporter 必須回指同一載具，原 pointer＋ID 必須在有限且無環的完整乘員鏈中。
- 步兵駐軍時逆查有效 Building 的 Occupants 成員；先嘗試前一個已驗證宿主，只有首次／失去關聯時才掃描 `BuildingClass::Array 0xA8EB40`。不以目的地、距離或附近建築猜入住關聯。
- 宿主需要可見存活身份和有效坐標；Occupants header、成員數組及原步兵 ID 前後一致才採用。失讀、壞指標、非成員或關聯尚未完成時保留最近有效取景坐標，並在該位置繼續採樣整個場景；不保留凍結 framebuffer，也不把隱藏步兵畫在舊世界坐標。
- Occupants 的成員數組及 header 都會重讀比對，並在採樣後核對原步兵 ID、於取得宿主坐標時核對宿主 ID。在生命週期 `readFresh` 路徑，重讀是獨立來源；普通 renderer 採樣仍用任務頁快取，其重讀只表示同一 snapshot 內一致，不能稱作獨立原子快照。
- 未知駐軍宿主的發現每個 simulation frame 最多一批、每批最多 64 棟，保存 cursor 在後續 frame 接續；空建築只讀一份 Occupants header，不讀完整生命欄位。失敗紀錄由 Track 傳到 Frame 共用，避免 follow 和 scene 各自反覆掃全體建築。這只約束宿主發現：已知宿主的成員／位置及周邊場景仍每個 overlay tick 刷新。回歸在同一 frame 連續 25 次更新確認沒有追加發現，下一 frame 僅追加一批；1,000 棟的案例首批恰好 64 份空樓 header，20 次同 frame 呼叫不追加，下一批續查找到原成員所在樓。初次宿主發現可能跨數個模擬幀，期間場景在最近中心繼續刷新。
- Frame 保存原焦點與目前 Anchor；refresh 比較宿主身份和格子，宿主切換／跨格走完整重取，同格移動直接刷新。相機、插值、焦點圈使用宿主的有效可見圖像；建築仍使用既有 -15 像素繪圖錨點，地圖相機限制不變。
- 單擊導航可對可靠的載具／駐軍宿主求相機中心；未知關聯不猜導航目的地。
- 導航把現有 Track／Frame 的 Anchor 傳給共用 camera helper，避免有界發現時已定位的第 65 棟之後宿主又從頭掃描。Windows handler 回歸使用 100 棟、宿主位於 index 70，確認有已驗證宿主的點擊仍可立即導航。

## 原版 Occupants 欄位：反彙編與受控 slice

`assist/tests/native_watch_occupants.py` 使用 `native_bib_capture.Capture` 的有效位元檢查讀取私人 `RA2Master-Capture-20261025-003340-3568`，輸出僅為欄位／範圍雜湊、計數與限制。

| 原始地址 | 本次直接核對用途 |
| --- | --- |
| `0x458098` | ESI 為 Building：讀 `+0x694` Occupants.Count，再減一作逆序索引 |
| `0x4580A9..0x4580B4` | 讀 `+0x688` Items，從 `[Items + index*4]` 取得實際 Infantry 指標 |
| `0x458704..0x458716` | 讀 Building `+0x684` 的 vector vtable、`+0x68C` Capacity，令 vector this 為 Building `+0x684` |
| `0x4581F0..0x4581F6` | ECX 為 Building，直接返回 `+0x694` Count；probe 在 ret 前停止 |

因此 vector 基底 `+0x684`／Items `+0x688`／Capacity `+0x68C`／Count `+0x694` 不再只是 header 型別推導。原函式 `0x457DE0` 的卸載路徑在 `0x4581E2` 為 ret8；本次僅執行上表讀欄位 slice，不執行其虛函式、卸載、刪除、尋格或遊戲更新。

三個 capture stage 各 11 個受控數值案例，共 **33 例**，涵蓋 Count=1/2/5/128 的末成員讀取、vector base/capacity，以及 Count=0/1/128 的 getter，全部通過。三份實際 capture 的有效存活 Building header 為 **48／53／57** 個，Count／Capacity 全部有效；**三份都沒有非空駐軍樣本**，不能據此宣稱 capture 已完成實際入住關聯驗收。結果與 code-range SHA 保存 `verification/native-watch-occupants.json`。

一手型別名稱／結構對照固定於 YRpp 提交 `d9795546950fcc75a94325ffc83b44d16ec34ce2`：

- [BuildingClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingClass.h)：Occupants 與 BuildingClass::Array。
- [ArrayClasses.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ArrayClasses.h)：DynamicVectorClass 的 Items、Capacity、Count。
- [FootClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FootClass.h)：入樓命令旗標和 UpdatePassengerCoords；沒有把命令旗標或 BunkerLinkedItem 誤當作步兵駐軍 backlink。

## 終止決定與失讀

生命週期 follow 現在由 production `readFresh` 讀取，不再用 renderer 的 4 KiB 頁快取作永久決定；普通場景／素材採樣仍使用原任務快取。所有生命欄位需要成功取得，即使 Health<=0 也不跳過其他欄位。相同 ID 的未知 vtable 歸類未知，不獨立作為死亡證據。死亡／ID 更換要再取一次獨立、完全一致的終止樣本才提交，沒有新增毫秒等待或邏輯 tick。

獨立 review 另重現了原來 control revision 保護仍有的具體漏網：worker 已採完「存活」畫面但阻塞於末端發布時，前台點擊導航可 fresh 判死並修改 UI Track；其後一份不同指標的新 Result 仍帶著較早的存活內容，由於 control revision 沒變，會把目標復活。`applied` 只防同一結果重複消費，無法防這種新完成的舊內容。新 `watchhost::followTracks` 統一前台生命週期更新；任何 Target 更換／清除都 invalidate worker、增加 controls、清對應 view、重新排版，pin 即使 NoTarget／Full 提早返回也不漏掉已確認的身份變化；navigate 使用同一 helper。

接替者有未知讀取時保留槽位，待可判定後才按原組、五格最近者決定；可靠的真正死亡且沒有可接替者仍關閉。載具內的原焦點死亡不以其可能過期的隱藏坐標蓋掉已驗證的最近宿主坐標。其他原組邏輯、身份更換保護、手動關閉及 control revision 防舊回寫保持生效。

## 驗證與未決

已執行通過：`watch_slots.cpp`、`watch_view.cpp`、`watch_host.cpp`、`watch_passenger_test.py`、`watch_windows_test.py`、`watch_async_test.py`；另有前述 33 例原版 field-selection slice。

新乘員回歸執行真正 production Renderer，確認載具連續移動跨格、同一 simulation frame 的旁觀單位仍每次更新、進出舱／樓、原 pointer＋ID／cohort 不變、非成員樓不接管、失讀或不存在的入住關聯只保持坐標仍刷新場景、過期頁快取的零 Health 不清槽，以及真正死亡移除。Win32/GDI／素材後端仍是邊界替身，不將這些結果寫成完整 Windows 遊戲實景、FPS 或雙機驗收。

既有 async gate 的 readFresh 阻塞點已收窄到地圖邊界發布讀取；否則新增的前置生命週期 fresh 讀會讓測試在錯誤階段暫停。原本不變 control/frame 下的縮界拒絕、慢渲染保留三窗、單次消費完成結果、關閉拒絕舊完成及快速選項切換回歸均仍通過。

`watch_async_test.py` 新增在真實 worker 採樣完成、末端發布仍阻塞時，前台 fresh 確認死亡並提交新的 control revision，再釋放舊完成，要求它被拒絕；舊版獨立 probe 曾於目標復活斷言失敗，修正後通過。`watch_windows_test.py` 另外執行 production navigate 的死亡分支，要求 controls 僅增加一次、槽位清除、沒有相機命令。

```sh
g++ -std=c++17 -O2 source/assist/tests/watch_slots.cpp -o /tmp/watch-slots && /tmp/watch-slots
g++ -std=c++17 -O2 source/assist/tests/watch_view.cpp -o /tmp/watch-view && /tmp/watch-view
g++ -std=c++17 -O2 source/assist/tests/watch_host.cpp -o /tmp/watch-host && /tmp/watch-host
python3 source/assist/tests/watch_passenger_test.py
python3 source/assist/tests/watch_windows_test.py
python3 source/assist/tests/watch_async_test.py
PYTHONPATH=/path/to/native-deps:source/assist/tests python3 source/assist/tests/native_watch_occupants.py /path/to/private-capture --output /tmp/native-watch-occupants.json
```

入樓／舱與出樓／舱的所有 MOD 特例、瞬間切換的實機畫面、宿主被刪除時的遊戲行為及長時性能仍需實景驗收。未修改遊戲物件、模擬、指令、Hook 或 DLL ABI。
