# 紅警工具箱 1.0.41-pre.10

本版改善小地圖、副相機、隊形／移動規劃預覽與渲染升級的共用 CPU／GPU 路徑，並包含 pre.9 Hook 修復和化學兵「攻擊腳下」。

## 本次更新

- 修正 Live INI 索引在尾部資料失讀或重名後反覆完整重建的高成本路徑。同次呈現合併失敗嘗試，每次查詢仍驗證目前 root/header；下一次呈現立即重試。
- 共用單次呈現的物件照明資料，保留各物件、位置、橋面與強度差異。
- CPU 靜態地形快取命中時直接恢復完成平面，並修正延伸前段後的失效；素材失效改為批次處理。
- GPU 使用自適應分箱並重用 metadata 容量；超過 65536 個 draw 時完整回退 CPU，避免 16-bit 索引別名。
- 增加分階段讀取、palette 命中及 INI 索引診斷。

## 驗證範圍

受控反例中，失敗查詢約 36.9 萬次 RPM 降至 8,286 次（約 179→4.1ms）；1024 actors 照明約 10.4→6.5ms；密集 GPU 場景約 10.5→7.2ms。數值是各自測試範圍，不能相加或當作整局 FPS 增幅；部分小負載持平或略慢。完整結果保留在更新包 `diagnostics`。

CPU 四平面、照明 State、真 GPU 畫面 byte 對照、索引邊界與失敗恢復均通過。未完成同一遊戲場景的前後 FPS／聯機驗收，也未證明 Live INI 是現場半秒小地圖耗時的唯一原因。詳細記錄見 [共用渲染效能研究](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/v1.0.41-pre.10/source/research/2026-11-05-shared-render-performance.md)。

## 下載與更新

下載 `RA2-Toolbox-1.0.41-pre.10-RenderPerf-x64-Update.zip`。退出遊戲和工具箱後，將所有內容完整覆蓋至原 x64 安裝，成套更新 EXE、DLL、broker 和 WebView2 loader；保留 INI、assets、cache。需要 Microsoft Edge WebView2 Evergreen Runtime。

「攻擊腳下」位於控制頁，預設 `Ctrl+Alt+F`；已有自訂快捷鍵時以介面為準。礦車自動點殺蜘蛛仍未實作。

原始碼另附 `-Source.zip`，雜湊與大小見 `release-manifest.json`。本版使用已驗證的同一批本機產物，發布流程會核對編譯來源與所有附件並回下載驗證。專案採 GPL v3；第三方授權與聲明隨包保留。
