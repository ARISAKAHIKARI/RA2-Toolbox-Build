# 暫停逐幀、視角縮放與隱藏右側建造欄

基線：`2bb821569c19e79a5869ec043b2ee47fcade1d7a`。本輪將三項做成預設關閉的實驗選項；隱藏建造欄明確指右側原生建造欄。這些功能沒有引入第二個遊戲命令排程器。

先查閱研究索引、既有縮放研究及 MO 合併邊界。Library 筆記本在本環境不可取得，本筆記保留新發現供後續回寫。參考 [MO_ExperienceEnhancements 固定版本](https://github.com/MagicShiba/MO_ExperienceEnhancements/tree/33ed558bd38f36b5012eddcc9c0fb109cc68e38a) 的 `PauseView.cpp`、`FullMap.cpp`、`ScaleBar.cpp`；沒有載入該 DLL。

## 支援樣本與證據範圍

本輪新增的原生邊界核對只涵蓋使用者私有 `gamemd-spawn.exe` SHA-256：`247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0`。原檔只讀，沒有上傳 EXE 或原始轉儲。

新機制使用 Assist89、Shared6584840；Runtime4、Entry3、Cursor155 維持原協定。共享資料只在尾部新增暫停狀態／按鍵、原生建造預覽、建造欄實驗狀態。跨位數檢查鎖定全部實際欄位，不能只憑版本號混用舊 DLL。

## 單機暫停與逐幀

- 只允許 Campaign/Skirmish `Session=0/5`。LAN/Internet `3/4` 和未知模式拒絕。預設 F5 暫停／恢復、F6 請求一幀，使用既有 Runtime 按鍵入口；按鍵可改，重複 keydown 不疊加。
- `0x55D360` 是 MainLoop 入口，原指令讀取 `0xA8E9A0` 到 AL。正常分支重放它並回 `0x55D365`。**原合入候選錯把 `0x55D365` 當成 TEST；實際是 `SUB ESP,0x1B4`，TEST 在 `0x55D36B`。** 現已驗證完整棧配置及 `0x55DECF..0x55DEDB` 尾端恢復序列。
- 暫停分支在建立原生棧框架前直接使用 `0x55DEDB` 的 RET；保存／恢復 GPR、EFLAGS、x87/SSE。不能先執行原生 prologue 再直接跳這個 RET。
- 單步根據 `0xA8ED84` 實際觀測幀數完成。原生節流未推進時繼續放行；觀測到增加一幀後再次暫停，跨過多幀則保留暫停並顯示異常，不倒寫幀數。
- 暫停小循環保留 callback `0x48D080`、Mark `0x4F42F0`、Input `0x4F4320`、鍵盤 `0x55DEE0`、Render `0x4F4480`、Tactical vtable `+0x5C`。callback 會泵 Windows 消息，因此不跨 callback 持有 Capture；其餘原生調用各自取得短 Capture。
- 失焦或打開原生模態菜單會取消暫停和待執行單步、恢復原循環，保留已安裝入口。這也修正了候選在 SpecialDialog 非零後仍攔住原循環、菜單無法正常進入的問題。

## 實驗視角縮放

恢復既有 `DDEnableZoom`／`ChangeResolution(0x560BF0)` 五檔：50%、75%、100%、125%、150%。這是**整張原生畫面一起縮放**，包含側欄和原生介面，並不是 MO 的獨立地圖縮放架構。

本輪樣本完整 `0x58A` 字節函式的 FNV-1a 為 `B5377C6F`，加入既有 `858162CF/C0FFC84E` 相容指紋。fastcall 的 ECX/EDX 分別為目標寬／高、AL 回傳結果。只有能取得所需 CnC-DDraw API、比例一致且物理客戶區保持原尺寸才接受；否則拒絕／回滾。50% 約需四倍像素，會增加原生繪圖工作量。

縮放前先完成工具箱建造欄租約的安全恢復；Render 尚在執行、所屬權衝突或幾何狀態不確定時，拒絕重建顯示模式。縮放後如仍要求隱藏建造欄，等待下一輪完整 Render 再重新套用。

## 隱藏右側建造欄：完整兩態與十三處入口

只提供完整／隱藏兩態，沒有把雷達保留模式冒充已完成。本輪保留原生當前邏輯解析度，用原生 `Set_View_Dimensions(0x4A8960)` 與 Tactical `0x6D5F60` 同步地圖範圍、原生點擊與裁剪；沒有套用十七處獨立地圖縮放／滑鼠轉換補丁。原有宿主全客戶區線性映射繼續使用。

| 地址 | 作用 |
| --- | --- |
| `0x72AD60`、`0x72AD67` | 原生矩形不再預扣側欄寬度 |
| `0x65386B`、`0x654332`、`0x653A3F`、`0x6D06C7` | 欄位定位使用實際側欄列座標 |
| `0x692BB4` | 原生邊緣滾動寬度的完整指令替換 |
| `0x6A70E0` | 側欄繪圖／游標階段 |
| `0x6ABD30` | 原生側欄配置入口 |
| `0x4E1640` | 地圖輸入 gadget 範圍 |
| `0x692406` | 原生點擊路徑 |
| `0x4F4480`、`0x4F459D` | 完整 Render 的進入／退出追蹤 |

七處常量替換只允許明列的完整 before/after；六處入口只能跳入本 Runtime 映像。每處完整指令邊界及周邊指令雜湊均核對，拒絕外來補丁。登記容量改為 128，避免總可登記 66 處時，歷史已停用站點仍占槽而耗盡舊 64 槽。

**`0x4F451B` 只是地圖繪製結束，後面仍有滑鼠、文字、Present 調用。** 本輪使用 `0x4F459D` 重放最後的 current-surface 還原，接 `0x4F45A3` 的純 pop/add/ret，才結束 Render 租約。首次啟用先只安裝進出追蹤，觀察一輪完整 begin/end 後才更換表面；未觀察到 begin 的單獨尾端不算安全點。

需要時用原生 DSurface ctor `0x4BA5A0(this,w,h,1,1)` 建立全客戶區表面對，保留原有表面。遊戲原生交換兩個指標時仍維持同一租約。關閉只恢復已確認仍屬本組的指標及入口，不釋放或覆寫外來表面。既有表面已足夠大時也記錄其成對身分、實際窗口及套用範圍，不能略過代次檢查。

每次表面／幾何修改和恢復前，重新驗證全部十三處所有權，尤其兩處 Render 追蹤。外來追蹤入口可能漏掉正在執行的 Render，不能把 depth=0 當作空閒；此時保留可能仍在使用的表面，拒絕關閉、縮放及 stop 清理，直到遊戲進程結束。外來幾何變更也不回放舊窗口範圍。

表面虛函式限定在遊戲 MEM_IMAGE 可執行區，拒絕私有可執行記憶體方法。清圖限制單邊 8192、總像素四千萬與 pitch 上限。配置工作及入口各用短 Capture + 獨立 SEH；故障一定清除 `viewZoomBusy`。原生幾何重建開始後若未確認完成，不會換回可能過窄的舊表面；保留租約供遊戲進程結束釋放，避免以不明狀態強行回收。

## 驗證

- `assist/tests/pause_view.cpp`：x86/x64 各 53 項，覆蓋單步未推進／跨幀、讀檔／場景／失焦復位、單機白名單、簽名及所有權。
- `pause_view_runtime.py`：執行真編譯 Runtime 按鍵與入口，檢查原生服務順序、幀觀測、重綁、焦點／菜單／心跳及 stop；callback、輸入、繪圖和 MainLoop 主體是模型。
- `view_zoom_runtime.py`：真編譯 dispatcher／SEH 適配器，四會話倍率往返、真正世界銷毀恢復、介面與指紋拒絕、失焦／按鍵／聊天等待、失敗回滾；渲染器 API 和 ChangeResolution 本體是模型。
- `sidebar_view_runtime.py`：真編譯管理器、七處常量及六處入口；檢查寄存器／棧／固定續接點、完整 Render 安全點、單個或整對外來表面、借用大表面、外來 tracking／幾何、暫停 Capture 嵌套、失敗清 busy 與不明幾何保留。DSurface、原生布局及 Render 中段是模型；模型失敗處理不冒充 Windows SEH 派送。
- `sidebar_view_guard_seh_boundary.cpp`／`sidebar_view_guard_seh.cpp`：獨立 Windows x86 程序實際寫入 PAGE_NOACCESS 頁，透過產品原樣 `sidebar_view_guard.h` 觸發並捕捉存取例外。檢查 boundary19、故障地址、`__finally` 釋放 Capture、巢狀 LIFO、拒絕執行及 LastError 保留。Capture、原生調用和 failure callback 在此微測中是模型；真編譯 Runtime fixture 另行檢查實際 failure body。[來源及執行雜湊](evidence/sidebar-view-guard-windows-seh.json)。沒有載入遊戲。

最終編譯及來源綁定證據由本輪總構建記錄保存。沒有執行使用者遊戲進程、Windows 現場 DirectDraw 視覺驗收、硬體 GPU 或 RA2OL 雙機測試，不能用這些 fixture 宣稱聯機同步已獲保證。
