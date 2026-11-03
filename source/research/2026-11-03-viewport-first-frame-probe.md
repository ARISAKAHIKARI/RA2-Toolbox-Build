# Native viewport 首幀探測與重連握手

問題：新 Host 尚未看過 active R，`shared=nullptr` 原先會被視為 identity。若遊戲仍有上一個 Host 留下的 Runtime，首次唯讀採樣及等待舊橋接卸載時會錯投影。`ensureBridge` 已存在映射分支也必須在 stop/unmap 之前觀察 prior，不能等之後 paint 才發現連線消失。

本輪只新增 Host 的唯讀 mapping 探測與受控驗證，不新增遊戲原生地址。遊戲程序沒有啟動、開啟或附加。

## 接線與權限

- `sampleExternal` 在任何 transform／場景採樣之前先 `probeNativeViewportMapping()`；`ensureBridge` 在建立新 mapping 及 retry gate 之前也探測。
- 探測使用既有 protocol 的名稱與已綁定 PID/creation 身分，只以 `FILE_MAP_READ` 開啟。只有 `ERROR_FILE_NOT_FOUND` 表示 Absent。權限錯誤、大小／magic／version／PID 不符、zero／odd／busy／active publication 都保持 Unknown。
- `ERROR_ALREADY_EXISTS` 分支先 map/validate prior，再 `observeNativeViewportPrior`，之後才允許 stop、移交 retired lease 或 unmap。
- 首次與 probe 過期前不回傳無映射 identity。只有沒有已觀察 R 的 Absent，或穩定非零 serial/generation 的 inactive 確認可放行；Absent 不能清掉先前 active/lost latch。同 PID/creation 的重新連線也不能清掉它。
- 沒有 owned Shared 時以 250ms cadence 重探，包含 Unknown；採用 helper 的 `steady_clock` 預設時鐘，不把 32 位 Win32 tick 與另一個 epoch 混用。已有 owned Shared 時由直接 seqlock 取樣驗證，不再打開第二個 mapping。

Source review 同時核對：Host 先寫 `HostHandshake` 高位標記；Runtime 不依賴低位 cap3 就首發有效 inactive，marker-only 分支等待握手而不先做舊版整屏 resize。Host 只從穩定 inactive 授予低位 Render/Input 3；active 只能保留同 mapping/lease epoch 的既有授權。短暫 busy1/odd 保留既有 grant，busy0/odd 撤回；重連 Unknown 不因 active／zero remap 提早升權。這是來源審查，最終 compiled Runtime 另待 root 綁定。

## 真 Win32 受控測試

`assist/tests/native_viewport_probe_host.py/.cpp` 抽取真實 main 的兩個 probe helper，以及 `ensureBridge` 的實際 prior map/validate/observe 前綴；另檢查呼叫確實位於首幀採樣、create、stop/unmap 之前。測試只為 fixture 自身 PID 建立真 Windows 命名 mapping，不會接觸遊戲 PID。只有 access-denied 錯誤使用 IO seam，其餘 Open/Map/Unmap、短 mapping、錯誤 protocol/PID 及 publication 狀態均使用真實 mapping。

x64／x86 各 **101 checks PASS**，`-UNDEBUG` 與 source `#error` 保證斷言啟用。涵蓋首幀、Absent、短 mapping、協議、zero/odd/busy/active、250ms 到期、同 PID 重連、舊 active 失去 mapping、清理後 inactive，以及舊身分 callback 被拒。測試綁定 main SHA-256 `7f4ab4fa300630a668895f350d15b5703c79578b49b41a6dfc2fd6ee28e2c364`。

[證據 JSON](evidence/viewport-first-frame-probe.json) schema `ra2.viewport-first-frame-probe.v1`，SHA-256 `0da57eb14ff114ded902fabc624a657c727ea8cf7bb3776d6459cd06dd6b0067`，包含每架構結果、原碼、抽取片段、EXE、日誌 hashes。最終完整 Host 與 Runtime 編譯／產物綁定待 root；此處不宣稱 Windows 遊戲畫面、原生 Render 或 RA2OL 雙機同步已驗收。

```powershell
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/native_viewport_probe_host.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out work/ra2-hook-audit/diagnostics/next-ui-ore-view/native-viewport-probe-host
```
