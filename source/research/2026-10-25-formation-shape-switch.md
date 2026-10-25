# pre.6：靜止切換隊形與預覽原樣提交

## 問題與既有研究

使用者確認直接右鍵下令時不重新計算是正確行為，但切換隊形種類後，預覽及實際目的格仍沿用同一種排列。調查基於已發布的 pre.5：`308c619c6436b307eb0a4234f4b4d707c56359e6`。

調查前查閱 `INDEX.md`、持久 SQLite v40 中的 `2026-10-24-height-overlay-sticky-137-pre13` 與 `2026-10-24-tracking-formation-missions-137-pre13`。本次沒有新增原版函數、欄位位移或 ABI 推導，也未以新的遊戲 EXE 作原指令驗收；是工具箱隊形算法及輸入流程的受控回歸。原有平台／映像准入維持 pre.5。私有遊戲程序和記憶體不進入倉庫或 CI。

## 根因

`formation_bridge.h::formationSwitch()` 已正常更新 `shape`，同步呼叫 `formationLayout()`，再發布包含新 revision 和 slots 的 preview。這條路徑不需要滑鼠移動，也不經 16 ms 滑鼠節流。

錯誤在 `formation.h::layout()`：未拖拽時 `click=true`，使 `spacing` 固定為 1，`base` 固定為 `ceil(sqrt(count))`，並以 `else if(click || ...)` 攔截圓形和扇形分支。因此 shape 0–4 在這個狀態都得到同一緊密方陣；只有較早處理的空心框保留自己的布局。名稱和 revision 可以變化，目的格卻仍相同。

舊測試也固定了錯誤語義：`formation.cpp` 要求 shape 1/2/3 的 click 布局逐點等於 shape 0；`formation_runtime.py` 要求切 C 後 slots 不變，理由為 `zero-length preview stays compact until drag`。本次移除這些反向斷言，驗證使用者要求的行為。

## 修復範圍

- `click` 僅令拉伸量為 0，表示目前沒有拖拽。
- 矩形／正方形保留各自比例；分散隊形保留兩格中心間距；圓形和扇形進入各自的算法。
- 現有切換流程立即重算、量化、避開永久障礙、分配每個單位的位置並發布，鼠標靜止也生效。
- 不改 `formation_bridge.h` 的按下、放開及提交路徑。按下凍結現有預覽；小幅抖動仍屬點擊；放開直接提交這份逐單位 slots，沒有額外重算。
- 不以偽造 `dragging=1` 解決問題，因此未拖拽下令不會平白新增共同朝向或兩段接近格。

`formation_engine.h` 的視覺 signature 已包含 shape 與投影位置，Host 的最新快照工作者仍會取得新 preview；沒有發現另一份滯留的 shape。此次不改相機、光照、透明度或非同步渲染策略。

## 驗證與證據邊界

1. `assist/tests/formation.cpp`：新回歸先配 pre.5 算法，於「zero-drag preview must preserve the selected shape」斷言失敗；修復後驗證六形狀自然初始布局、分散間距、矩形／方形比例、圓形／扇形及空心框區別，並保留 6,144 組計數／形狀／拉伸與穩定分配回歸。
2. `assist/tests/formation_switch_test.py`：直接編譯生產 `formation_bridge.h`，僅替代 Win32、遊戲記憶體與原生查詢／命令提交邊界。19 組受控案例檢查 25 個單位、同一 clock 且無滑鼠移動時切換、同鍵循環、不響應長按 repeat，以及發布 slots、待提交 job 和逐 ID 命令目的格的一致性。此項與 Windows 數值測試都列入發布門檻。
3. `assist/tests/formation_runtime.py`：更正舊斷言並補上對應的實際 DLL fixture 案例。本輪本地缺少可供該套件使用的原始遊戲 PE 與 DLL，僅做語法檢查；不把它算作已執行的 DLL 回歸。
4. CI 增加 `formation.cpp` 和上述 bridge fixture；連同後續覆蓋層保活回歸，合計 43 項 portable 門檻；另以發布使用的 Zig 0.13.0 編譯 formation 數值測試，在實際 Windows x86 進程中執行。EXE／DLL 成套構建、版本／ABI、原有視窗和設定保存檢查仍為門檻。確切提交、Actions run 和成品雜湊由該版 `release-manifest.json` 記錄。

受控 bridge 測試不是原始 DLL、遊戲尋路、顯示器已呈現確認或雙機聯機測試。少量單位可能使不同形狀自然重合；地格量化和障礙避讓仍可能改動局部位置。25 單位空地案例用來確認各形狀不再一律退化成同一方陣。

## 已排除及未決事項

已排除切換按鍵未送達、必須移動鼠標才會重算、shape 不在視覺 cache key、以及右鍵另用一份舊目的地。直接點擊保持原樣提交是既有正確行為，未改動。

原生尋路如何繞過動態單位和全部 MOD 地形、實際顯示延遲及聯機最終到達形狀，仍需遊戲實景驗收。本次不把受控條件擴大為這些結論。
