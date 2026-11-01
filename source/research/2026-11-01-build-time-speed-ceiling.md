# 建造提示按速度上限換算

使用者要求聯機按允許最大幀率；單機速度 1–5 按檔位上限，速度 6 按速度 5。舊實作單機讀 `FPSCounter.CurrentFrameRate=ABCD44`，聯機讀 `Session.DesiredFrameRate=A8B558`；前者是實測速度，後者可隨 FrameInfo 協商調整，均不符合這次固定上限要求。

先查 INDEX 的 Hotfix27 計時筆記與固定 [YRpp GameOptionsClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GameOptionsClass.h)、[FPSCounter](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FPSCounter.h)、[SessionClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/SessionClass.h)。本次 Library SQLite 不可用，保留倉庫記錄供回寫。

支持樣本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。

* 原生 `4E1E14..4E1E1B` 把介面速度轉成 `6 - 顯示值`；`GameOptions.GameSpeed=A8EB60`。
* `79730D..7973B6` 按內部 0–5 選擇聯機上限 `60,45,30,20,15,12`，寫 `Session.PrecalcDesiredFrameRate=A8B570`。這個暫存後續會清零，不能把它當永久設定直接讀。
* 運行期協商 `647C43..647C74` 直接讀 `A8EB60`，內部 0 給 60、1 給 45，其餘 `60/speed`，再與適應速率取小值；六個原始分支已受控執行。原始遊戲事件 `4C794B..4C7954` 只更新 live `A8EB60`，不改 setup `A8B268`，所以局中變速也必須讀前者。
* 單機 `55D79E..55D7BC` 將 GameSpeed 複製到計時器 duration；`6C8C46` 使用毫秒右移 4 位的粗粒度時鐘。顯示沿用標準 60 tick 的名義速度上限，不將 16 ms 量化、卡頓或顯示器 Hz 當新的生產規則。速度 6 按 5 是本次明確的估算約定。

| 介面速度 | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| 單機提示換算 FPS | 12 | 15 | 20 | 30 | 60 | 60 |
| 聯機提示換算 FPS | 12 | 15 | 20 | 30 | 45 | 60 |

只改提示秒數的分母；原生 TimeToBuild、54 格截斷、低電與多工廠倍率、真實工廠剩餘期限均保留。未知模式或非法檔位顯示幀數，不猜秒數。

驗證：`tools/audit_build_time_rates.py` 執行原始六個聯機表分支、原始介面速度轉換並核對單機計時簽名；`assist/tests/build_time_rate.cpp` 驗證正式換算與未知輸入。證據：[JSON](evidence/build-time-speed-ceiling.json)。不代表實際生產牆鐘計時、RA2OL 或修改原生速度限制之平台的驗證；私人遊戲檔與反彙編完整轉儲未發布。
