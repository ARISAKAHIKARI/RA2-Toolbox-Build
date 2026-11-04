# 放置預覽持續性、座標投影與原生收尾

日期：2026-11-02。基線：8437c37b4c7d63cfca5aef4860cb366575721b2a。私人 PE SHA-256：268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0。只讀檔案及隔離 fixture；未啟動、注入或操作遊戲進程。Library 筆記未連接，本筆記供後續回寫同一資料庫。

## 持續預覽與嚴格提交

原 InputContext 要求即時遊戲 frame 與工具快照完全相等。滑鼠訊息先到下一幀時，唯讀預覽把正常的一幀差當作失效，Renew 直接 Reset；按住快捷鍵後移動幾格便消失。preview=true 現允許即時 frame 向前，仍要求 session、scenario、match、House、產品 UID 及 Type 身分一致。真正 Place 的 Renew、InteractiveValid 和提交沿用嚴格相等，不沿用過期預覽下單。回退 frame 或 scenario 改變仍取消。

離開可讀地面和超出可用格只改當前預覽狀態，移回可讀格可恢復。地圖內點擊維持成對攔截，失效位置不洩漏成原生單位命令；點建造欄則歸還原生輸入。未知地面不宣稱合法，最終落地合法性由原生 Place 接收鏈決定。未加入副作用尚未完整排除的 CanPlaceHere(464AC0) 預查。

## Foundation、地形高度與 viewport

原生 SetActiveFoundation(4A8BF0) 的偏移為 -floor((width-1)/2)、-floor((height-1)/2)，不是 -floor(width/2)。Map+1174/1176 保存中心格、+1178/117A 保存偏移；MouseUp(4ABA16..4ABA2B) 相加形成 Place 原點。預覽與事件共用 native_placement_geometry.h 的原點，建築與防禦欄產品保持分開。

ProcessClickCoords(692300)→ClientToCoords(6D2280) 輸出 Z=0。這不是地形高度；使用原生 GetFloorHeight(578080)，12-byte FNV-1a 8FFFDB45，相容檢查失敗就不畫未知位置。投影採用遊戲現值 zScale，讀不到時使用 .0866025403784，舊 .25 會放大高度偏移。

原生 CoordsToClient(6D2140)、ClientToCoords(6D2280)、CoordsToCell(6D6590) 在合成平地及互逆矩陣中受控重放：world(12928,15488,0)、camera(-400,1465) 得 viewport-local(100,200)。viewport origin(0,0)/(168,0)/(0,32)/(168,32) 的 full-client 輸入分別須加該原點，四例均反解 cell(50,60)。Overlay 現加真正 viewportX/Y，且使用同一預覽世代的 camera，避免和較舊快照混合。證據 diagnostics/placement-view-hotfix/native-projection-roundtrip.json；腳本 native_projection_probe.py。

## 確認落地後清理相同產品的 UI

原生 MouseUp 在 Place 入隊後把 active 游標搬到 copy 游標；成功接收方在 4FB2CC..4FB33F 清除相應組。工具先前只清自己的 modal，缺少這個 UI 收尾。只呼叫 SetActiveFoundation(nullptr) 也不夠：它清 foundation 的畫面標記，並不清 CurrentBuilding/CurrentBuildingType。

active 三欄為 Map+11A4(product)、+11A8(type)、+11AC(House array index)，匹配時用 4A8BF0(nullptr) 並清三欄；copy 為 +1190/+1194/+1198，匹配時用 4A8D50(nullptr) 並清三欄。4A8BF0 的 12-byte FNV C44D9D5E；4A8D50 為 DC84E30E。House 欄位是 +30 陣列索引，不能拿 House UID 或指標比較。避免呼叫會取消其他工具且要求 Limbo 的 4AAD30。

收尾監看只認凍結的 scenario/session/House index、原產品 UID、Type UID、Owner 及 Alive/OnMap/非 Limbo。入隊本身不清 UI；失敗、逾期、換代和產品身分不匹配不清其他游標。不發假點擊、不重發 Place、不改規則。

## 驗證與限制

舊實作 frame=301 的首個 WM_MOUSEMOVE 會取消預覽，獨立反例 exit=3。修正後連續前進到 316 保留預覽；過期幀的 left-up 不提交，新鮮快照後的新點擊可提交。正式 production-header fixture 明確 -UNDEBUG 並以 NDEBUG #error 防止空斷言，涵蓋地圖／側欄輸入、未知地面恢復、foundation、viewport、地形高度和 scenario 換代。完整命令、來源 SHA 和新核心二進位配對證據見 parameter-placement-followup-build.json。

上述是原生受控執行、正式 C++ 標頭及已編譯 DLL 的隔離驗證，不證明真遊戲畫面或 RA2OL 同步安全；未做雙機實測。

補充高度投影對照：正式腳本 assist/tests/native_placement_projection_runtime.py 額外對 0/104/208/416/728/832 六種 Z、三種明示投影 scale（.0866025403784/.15/.25）重放原生 CoordsToClient，共18例，比對 Overlay 共用 screenHeight 的公式；這個試驗明示模擬 Runtime 才會初始化的 B0CD48，不把原始 PE 的零值當成實戰投影。
