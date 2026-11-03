# MO 原生地圖縮放、共用 Render 與側欄即時重畫

基線 `5983b943e90694f7b2630608bc464d6f8ee148e3`。採用 [MagicShiba/MO_ExperienceEnhancements `33ed558bd38f36b5012eddcc9c0fb109cc68e38a`](https://github.com/MagicShiba/MO_ExperienceEnhancements/tree/33ed558bd38f36b5012eddcc9c0fb109cc68e38a) 的 `ScaleBar`／`FullMap` 原生地圖縮放架構，整合既有 Hook 管理器、Host 與輸入。原生地圖 R 光柵→物理地圖 A，側欄和窗口 UI 維持物理尺寸；工具箱有自己的 UI，未移植上游的 GDI 滑條／VecText 字型擴展。

已讀 AGENTS、INDEX、早期 A/R 調查與實驗顯示筆記。Library notebook 不可用，研究保存於 repo。私有樣本僅唯讀，SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。未啟動遊戲、未對遊戲程序安裝測試 Hook、未做 DirectDraw 畫面或雙機驗收。

## 完整原生呈現

A 是物理地圖區，保留左側欄原點；隱藏側欄才使用整個地圖寬。R 大小為 `A×100/百分比`，縮放時 R 原點固定 `(0,0)`。`Set_View_Dimensions(0x4A8960)` 先以 A 配置 UI 和窗口 Z/ABuffer，保存當前 Tactical `+D64/+D68/+D74/+D78` 四個中心，再由 `0x6D5F60` 同步 R、`0x886FA0` 與 `0xB0CE28`。Native 路徑不改窗口解析度。

兩張獨立 R DSurface 和 R Z/ABuffer 供三趟地圖使用，避免放大像素餵回增量拷貝。游標擦除、趟間 chrome 及最後 UI／Present 使用窗口面；map 完成做一次 R→A 最近鄰重採樣。圖外只填本地背景，不改 shroud／reveal／規則資料。R 每軸最多8192、最多4000萬像素，並檢查 pitch、16-bit像素及 Z/A 資料範圍。非法新 request 不毒化舊 active view。50% 原生地圖會畫更多像素，不能宣稱此功能免費提升 FPS。

`native_viewport_policy.h` 管理17點：共用現有 `0x4F4480` 入口，另16點涵蓋 phase、input、tooltip 和5處物理 A operand。每處預查原 bytes、完整指令邊界與遮罩上下文；未知版本或第三方改寫拒絕啟用。data operand 僅允許 exact opcode immediate 指向 Runtime image 可讀資料；vtable／JMP 仍要求可執行 Runtime image。

只有一個 Render owner，Sidebar 和 Native viewport 共用 `0x4F4480`／`0x4F459D`，不各搶 JMP。`0x4F451B` 只表示 map 完成，後續 mouse/text/Present 還在執行；Windows callback 在 true leave 前不能 resize／關閉／釋放 allocation。所有 naked adapter 保存 GPR、EFLAGS、x87/SSE FX，再重放完整 displaced instruction。

## 輸入與 Host 握手

共同契約是 `native_viewport_projection.h`。WWMouse 原始位置保持物理；vtable+34 getter 回傳 R，edge-scroll 仍用物理。`0x6930A0` 使用有上限的 Runtime LPARAM shadow，不修改 caller 的 LPARAM；return thunk 保留原返回值和 RET16，以 LIFO 回收，最多16層。未知／busy／超深狀態安全拒絕。

程式內三條 `0x692300` 查格將 full-client 物理點轉 logical 點，再減 logical origin；前後核對同一 generation。`GuardedNativeViewQuery` 用 MSVC SEH `__finally` 回收 query depth，既有 WM Capture lease 也可用；query depth 防止已映射查格被 native click gate 再按物理滑鼠分類。異步 pointer 與 placement preview 綁 generation；無 generation 的舊 one-shot 在 active 時拒絕。

Host 按 R 裁世界、按 A 合成，地圖文字／標記落點只轉一次，文字等 UI 尺寸維持物理像素。橡皮框是 R-local。原生本身在地圖 pass 繪製的世界標註仍隨地圖縮放，沒有額外移植上游的字型替換 Hook。

Assist91 尾部共享 `viewportSerial/Active/Generation/HostCapabilities/Physical[4]/Logical[4]`。HostHandshake 高位代表本版握手；低1/2是 render/input capability。新 Host 先 high|0，等 Runtime 發布真正非零偶數 serial、非零 generation 的 inactive，再 high|3。memset零不是有效 inactive；保存75%也不會在握手前先跑整屏 legacy resize。

發布次序是 busy1→odd→寫資料→even→busy0。同一已驗證 mapping/lease 在正常 mutation 保留 capability，但 odd 取樣仍拒絕畫面／輸入；fresh、重連未知及 odd+busy0 故障不借舊授權。清零 wire 時私有 active 重發相同 generation，不假報 inactive。cap0 無握手舊 Host 保留 legacy 相容；必須等 Native 完全退休才可啟動 legacy。

## 側欄立即重畫及安全退役

幾何成功當次即 `RegisterDirtyArea(0x6D2790)`、`Flag_To_Redraw(0x4F42F0,2)`、`PostMessage(WM_PAINT)`，不在 callback 同步 Render。再補兩次以 `(simulationFrame, completeRenderSerial)` 去重，模擬暫停仍可刷新增量雙緩衝，不永久強制重畫。dirty ABI 是 ECX＋Rect-by-value＋bool，RET20，已直接執行私有原函式驗證。

焦點切換、暫停和觀戰保留同一進程所有權；明確關閉／停止才申請恢復。額外 SceneRefresh render 在 demand、active、retiring 都顯式互斥。mutation 前核對全13處 Sidebar、16處額外站點 exact owned，以及 Render/Wnd/query idle；tracking 丟失不能當作「沒有 Render」。任何 foreign surface／Z/A／phase 都不覆寫、不釋放可能被引用資源。

同代恢復 saved A 需要 Scenario、Tactical、frame high-water、非 Loading、窗口／螢幕／HWND 及 exact pair/buffers 都一致。Loading 是 recoverable wait，下一正常 Runtime wake 觀察穩定世界再恢復，不升級永久 conflict。換局／frame 回退／窗口變化不能回放舊 A 或中心：完整新 pair/buffers 只採當前原生 geometry；同址重用但仍 exact pair 時，經全 idle、容量足夠，以當前窗口和當前中心算 fresh A 收尾。窗口變大舊 pair 太小則等原生重建。

跨世代同址不代表舊所有權：兩條退役路都丟棄舊 Sidebar Owned/Saved/gadget journal，不析構可能已被引擎刪除後同址分配的物件、不回放 stale saved pointer。private R 只有全無 global 引用才釋放。fresh 收尾跳過舊 hide2 gadget journal，保存新 Scene 原生 flag0/1。這是保守放棄未知舊 journal，不能宣稱未知引擎重建時零資源遺留。

## 可重跑驗證與界限

- `assist/tests/native_viewport_runtime.py <game> <GameRuntime.dll> --output <json>`：編譯 Runtime/PDB＋管理器、17點真 adapter，原生 Render prologue/phase/leave；繪圖 primitive、allocation、Win32 與部分 native body 明確為模型。驗 GPR/ESP/EFLAGS/FX、三趟面切換、Wnd返回、Capture query、倍率/側欄、focus/pause/observer、world/gadget/foreign、legacy/scroll互斥。綁 DLL、匹配PDB、私有PE與 source SHA。
- `assist/tests/mo_viewport_native_contract.py <game> --output <json>`：唯讀17個原始站點、完整指令邊界及原指令 register/stack replay，不能替代 adapter 證據。
- `assist/tests/mo_viewport_native_projection.py <game> --output <json>`：10組真原生投影／查格及 A/R roundtrip。
- `assist/tests/native_viewport_projection.py --zig <zig> --out <json>`：Windows x86/x64 各51項、assert開啟。
- `assist/tests/sidebar_view_guard_windows.py --zig <zig> --build-dir <dir> --output <json>`：真 Windows own-process x86 MSVC SEH。保留 boundary19，新增21 query success/AV/nested/finally/reject、22 action/Unlock AV、Wnd/mouse guard。測試程序固定載入位置，以自身映像中的 BSS 預先保留 0x692300 的頁面，再寫入測試 stub；不載入／執行遊戲。
- cap0 原 `view_zoom_runtime.py`25組及 `sidebar_view_runtime.py`24組保留。

證據：`evidence/mo-viewport-runtime.json`、`mo-viewport-native-contract.json`、`mo-viewport-native-projection.json`、`mo-viewport-transform-windows.json`、`sidebar-view-guard-windows-seh.json`。最終 build/source binder 由整合報告綁定。這些不是遊戲視覺、DirectDraw driver 或 RA2OL 同步證明，入口保持實驗功能。
