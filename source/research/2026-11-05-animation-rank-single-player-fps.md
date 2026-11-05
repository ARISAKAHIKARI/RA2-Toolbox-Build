# 建築末幀、星級縮放與單機自訂 FPS

使用者回報建造廠夾子等停留末幀的動畫消失、三星標誌未隨 SHP 縮放，並要求填補單機 4／5 速間隙。先查 INDEX、動畫對齊／建築元件／雷達終端標誌／建造速度筆記及持久 SQLite。基線 `2ca6b9635bf27fb1af59e66038f75691fbcc2d70`；版本 `1.0.42-pre.2`。

## 支持映像與證據範圍

本次私人擷取的主映像 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。只讀取有效性遮罩完整的位元組並反彙編，未執行該映像，未發布私人 EXE、記憶體或完整反彙編。欄位對照 YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2`。舊速度筆記的另一映像 SHA 不代表本次已重新執行；安裝時仍逐位元組驗證當前入口。

## 建築動畫

* `Building::PlayAnim=451890` 將絕對座標動畫放進 `Building+55C` 的 21 槽與 `+5C8` 的 8 槽；`Anim+CC` 可以為零，`+118` 為建築動畫標誌。
* `Anim::Draw=422CA0` 的原生選幀使用 `Animation.Value(+AC)+AnimType.Start(+2B4)`，不以 `TimeToDie(+179)` 作為獨立剔除條件。`PausedAnimFrame(+11C)` 不是替代繪製幀。
* `424B0E/424B31` 的末尾通知可設定 `TimeToDie`；`UnInit=4255B0`／`ObjectUnInit=5F65F0` 後的 `Alive(+90)`、`DestroyNthAnim=451E40` 及 `PointerExpired=451B40` 清槽，才提供確定的失效依據。
* 正式採樣保留仍屬於同一存活建築槽／相同 ID 的終止通知動畫，仍檢查 Alive、Invisible(+19D)、vtable 與槽身分。瞬時全域動畫維持原有終止剔除。允許負 relative Value 與非負 Start 合成合法幀，且拒絕任何讀取失敗。

`assist/tests/building_animations.cpp` 受控驗證終止／暫停旗標下保留末幀 240 次觀測、死亡／清槽移除、負相對幀、錯讀及原有槽／身分／劇院檢查。私人舊擷取未包含使用者本次故障現場，因此這是已修正的原生語義差異，尚未證明它是實機消失的唯一原因；不以猜測幀或延遲垃圾保留掩蓋。

## 星級 SHP

移除 Scene 的獨立 rank 註記與面板終端固定像素繪製。星級現在經 `Layers::rankGlyph` 解碼成普通唯讀 SHP Image，參與同一 `ObjectCanvas` CPU／GPU 命令、快取失效和最終面板縮放；保留原生 UI 不測 Z／不寫深度語義。主視口已有同倍率 UI 合成，這次沒有另改其素材比例。

`rank_sprite_scaling.cpp` 直接呼叫正式 SceneRenderer：升星、移動、消失、不可變舊畫面，以及 0.5×／1×／2× 像素結果。`radar_output_scene.cpp` 在 Windows CameraAssets 路徑補相同檢查。舊筆記的固定物理像素策略由本次明確需求取代。尚未完成使用者遊戲畫面驗收。

## 單機 FPS

* 原生 `55D79E` 讀 `GameOptions.GameSpeed=A8EB60`，`55D7A8` 呼叫 `6C8C40` 的粗粒度時鐘，`55D7BC` 是六位元組 `mov [887350],esi`，後續 `55D7C2` 分辨 Session 0／5 單機。原生 16 ms 格數只能形成大幅間隔，直接設定整數 GameSpeed 無法提供 40／45／50 FPS。
* 新增同一 `techbp::Service` 的邏輯 tag 5（mask 32），仍共用實體四槽，沒有第二 VEH、沒有修改遊戲指令、vtable 或 GameSpeed。前綴 `55D79E..55D7C1` 共 36 位元組 FNV-1a 為 `A0B5F183`；安裝與每次命中都驗證。
* 每次受管命中在原生遊戲執行緒以 QPC 整數餘數期限限速，使用高精度 waitable timer（不可用則一般 timer），最多 4 ms 分段等待；不泵送訊息、不呼叫引擎。成功只替代本次 duration 資料寫入為 0，EIP 前進六位元組到 `55D7C2`，保留 ESI／算術旗標；受管例外原有 RF／Dr6 管理保持不變。下一原生迴圈自然重新寫入期限，無需持續覆寫或還原 GameSpeed。
* 每次資料寫入前檢查 Session 0／5、存活遊戲、場景／幀不倒退、Host 500 ms 心跳、stop／fault、有效目標與完整簽名；關閉、切到聯機、失聯或不相容時回到原 MOV。關閉／退出沿現有受驗證槽退休流程處理，保持其他功能槽與外部槽。
* 設定 `[SinglePlayer] FPS`：預設 0 原版，允許整數 10–240；9494 設定、9495 狀態，同步現代控制台與顯示頁。它是遊戲邏輯 FPS，與渲染 Hz／補幀分開。僅此功能也會要求連接 Runtime。建造提示只在此功能實際運作時採目標 FPS 換算秒數。
* Assist ABI 升至 97，Shared 23,141,856 bytes，映射 `Local\\RA2Assist_v097_`；Host／Runtime／入口程式成套編譯與 PE 驗證。其餘協定不變。

驗證入口：`single_player_fps.cpp` 檢查 10／30／40／45／50／60／144／240 的十分鐘整數期限、暫停／遲到／目標變更、模式及槽預算；`single_player_fps_windows.cpp` 在自有程序固定資料頁呼叫正式 callback，檢查寄存器／旗標／原版速度不變、單機／聯機／失聯／變更／關閉等待與資源清理；`tech_ui_breakpoint_windows.cpp` 另外驗證 tag 0 與 tag 5 的真實兩槽安裝／命中／退休。這些不代表實際 RA2 FPS、平台相容性或聯機驗證；遊戲本身算不動時不能保證達到目標。
