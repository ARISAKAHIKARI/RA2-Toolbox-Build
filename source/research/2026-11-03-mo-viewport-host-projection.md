# MO 地圖縮放的 Host 投影、畫面換代與握手

本記錄接續[原生側欄／MO 整合契約](2026-11-02-sidebar-redraw-and-mo-viewport-contract.md)，記錄本輪 Host 實作與受控驗證。歷史對照為 `5983b943e90694f7b2630608bc464d6f8ee148e3`。Runtime 的原生 Render adapter、輸入逆映射與釋放證據另由各自測試保存；本記錄不把 Host 測試視為原生 DirectDraw 或聯機驗收。

## A 與 R 的用途

Assist91 發布 seqlock 的 `viewportSerial/Active/Generation`、物理顯示矩形 A、邏輯光柵矩形 R；原生縮放活動時 ViewBounds 與 `B0CE28/2C/30/34` 維持 R。Host 的 `mainview::View` 保留原有欄位作為 R，追加完整 Transform；`physical()` 才取得 A。原本未啟用的呼叫仍有相同欄位與預設行為。

| 內容 | 處理 |
| --- | --- |
| 世界場景取景、原生世界座標投影 | 以 R 的寬高採樣／裁切；SceneRenderer 直接輸出 A 尺寸 |
| 世界端點、單位標籤錨點、乘員氣泡錨點、橡皮框 | 原生 map-local R 座標只映射一次到 A，再裁切至 A |
| 字型、氣泡內容、游標圖像、原生訊息／計時文字 | 維持物理像素尺寸；不再跟 R 的光柵尺寸一起縮放 |
| 建造預覽 | 使用預覽保存的 camera、R 原點及 viewportGeneration；代際不同立即拒畫 |
| 超武獨立面板／側欄提示錨點 | 以 A 的右緣佈置，不讀放大的 R 右緣作實際窗口位置 |

ViewLease、非同步 request/frame、合成 Context 與 compositor group token 都包含完整 Transform 或由其計算的 projection key。世代改變、不穩定 seqlock 或原生 ViewBounds 不等於所聲明 R 時，撤去舊畫面並等待重取。`render_upgrade_host` 首次讀 Transform 與真正採樣 View 之間也核對同一 Transform；兩次各自穩定但代際已切換仍會拒絕，不能把新 View 配上舊畫面 key。

## 映射遺失與啟動探查

`native_viewport_host.h` 的 Lifecycle 綁定遊戲 PID 與讀取連接已取得的 process creation time。切窗、讀取器重連、pid 暫為零或同一 PID／creation 都不當成原生幾何已恢復。

一旦看過 active，或在 mapping 解除前發現 active／odd／busy／未發布值／仍有 Render 授權，`mappingLost()` 保留 unknown；同址重映射也不能繞過。只有 Runtime 明確發布非零偶數 serial、非零 generation、idle inactive，或真正更換 PID／creation 才能解除。釋放直接發生在 `releaseBridge`，不依賴後續畫面是否來得及看見空 Shared 指標。

新 Host 即使還未建立自己的 Shared，也先探查既有同名 mapping。純 helper 的 API 為：

```cpp
ProcessIdentity processIdentity();
bool probeDue(uint64_t nowMs = probeClock());
bool probeAbsent(ProcessIdentity expected, uint64_t nowMs = probeClock());
bool probeUnknown(ProcessIdentity expected, uint64_t nowMs = probeClock());
template<class Shared> bool probeExisting(ProcessIdentity expected,
    const Shared*, uint64_t nowMs = probeClock());
```

回傳 bool 只表示回報身份仍匹配；`Existing` 也可能分類為 unknown。實際 Win32 wrapper 只將 `ERROR_FILE_NOT_FOUND` 視為 Absent，驗證 magic/version/PID 後才把 mapping 交給 helper；存取、尺寸、協議錯誤與 active／odd／零值全都保守拒絕。首次尚未探查或觀察已超過 250ms，空 Shared 不能直接當成 legacy identity；主程式在讀外部畫面及建立命令 mapping 前以有限 cadence 重探。Absent 不會清除已存在的 active/lost latch。這是同機 Host 服務觀察間隔，並非跨程序原子互斥保證。

## 能力握手

`HostHandshake=0x80000000` 表示新 Host 參與本版握手；低兩位依然為 Render=1、Input=2。新 Shared 清零後立即寫 marker，早於 Runtime 掛接。Runtime 等待 marker 的低位授權，不在等待期間退到舊整窗口 resize。Host heartbeat 在喚醒 Runtime 前發布 `capabilityRequest()` 的決策。

首次低位 0→3 必須從有效已發布、idle inactive 狀態取得。active 只能保留先前在相同 mapping 指標與 lease epoch 已授權的 3；fresh active、指標重用、重新連接或低位已清零都不能升權。Runtime 按 `busy1 → odd → commit → even → busy0` 發布時，僅相同已授權 lease 可在 busy1 保留 3，以免 Host 自己在 mutation 中反覆撤權；繪製／輸入依然拒絕 odd。odd 且 busy0 是故障狀態，撤掉低位而保留 marker。全部判斷以低位 mask 處理 `marker|3`，不把它誤判成不等於 3。

## 可離線重跑的驗證

以下從 `source` 目錄執行，`<zig>` 指 Zig 0.13.0 可執行檔，`<out>` 為各自的可寫輸出目錄。測試只操作合成讀取資料；Win32 身份例只讀 fixture 自身並啟動隱藏的自身副本，未啟動或附加遊戲。

```text
python assist/tests/mo_viewport_host.py --zig <zig> --out <out>/core
python assist/tests/mo_viewport_drawings.py --zig <zig> --out <out>/drawings
python assist/tests/mo_viewport_scene.py --zig <zig> --out <out>/scene
```

結果格式均為 `results.{x86_64,x86}.{passed,checks,...}`、`sources`（相對 `source/` 的路徑至 SHA-256）、`changed_during_run`（必須為空）及 scope。runner 在編譯前與執行後核對列出的產品與 fixture 源檔。歷史兩個 header 已固定收進 `assist/tests/fixtures/mo_viewport_baseline_5983`，manifest 保存 commit 和原檔 hash；只在測試中改 namespace，產品不 include 它們，不需網路或工作目錄 diagnostics 基線。

| 測試 | x64／x86 各自結果 | 具體對照 |
| --- | --- | --- |
| Host | 390 checks PASS | A.x=0/168、50/75/100/125/150%、非等比947×851→631×567、舊 View 四次拒絕擴大 R、舊 sampler 遺漏 R 中可見單位；合成 CPU compositor 像素、租約／換代、取樣中切換、motion reset、能力握手／mapping 釋放／首次探查 |
| Drawings | 21 checks PASS | 實際 placement/MO orders/formation/air path 函式配記錄 GDI+ I/O，驗一次映射、A 裁切、凍結預覽與代際淘汰 |
| Scene | 20 checks PASS | 實際 Sampler/CPU SceneRenderer 五倍率，輸出固定632×568；與完整 R 尺寸參考比較1,213,038個穩定像素，0差異；錯用 A 作世界裁切在非100%時有明確反例 |

最新完整 Host translation unit（含主程式的 probe、release、marker 與 heartbeat 接線）以 x64 O1 編譯通過，源檔編前／編後無變更；這是編譯整合檢查，不是成品程式執行驗收。

證據保存於 [mo-viewport-host-projection.json](evidence/mo-viewport-host-projection.json)。這些測試證明相應 Host 座標、CPU 輸出及換代邊界，沒有量測真遊戲 FPS，也不證明 Windows DDraw/GPU 顯示、RA2OL 平台或雙機同步已通過。原生 MO 模式改寫遊戲程式碼，入口依使用者既有規則標為高危、實驗性；不以這組測試降低其分類。
