# pre.9 與攻擊腳下整合

此包以 `codex/hook-recovery-20261105@dde2f56b3c663f3355d1c7ca0c436f5a2ba8ee58` 為基準，移植 `0f15e817a8f07904a48db3fb90934a19c9442219` 的攻擊腳下功能，以及 `88c19f00a56309672fb85ad5a1a6ab0750badfec` 的 UTF-8 測試修正。

版本資源在兩邊已同為 `1.0.41-pre.9`，因此略過重複的版本 hunk；套件名使用 Chemical 區別。研究索引保留基準最新的 Hook 與例外政策記錄，再加入攻擊腳下連結。抽取式 reconnect fixture 的 NOOP 清單補 `resetOwnGround`，生產行為不受此測試替身改動影響。

獨立審核確認 17 個例外、Runtime、硬體斷點及入口所有權保護檔的 Git blob SHA 與基準完全相同。原分支較舊的完整檔案沒有覆蓋新版基準。

本次七組受控回歸全部通過：攻擊腳下批次／快捷鍵、Hook 恢復政策、fault reconnect、Windows x86 exception policy、Windows runtime scope、斷點交易政策及 Windows DR／VEH 交易。攻擊腳下政策含 27 項。完整報告見發行包的 INTEGRATION.json；編譯、PE／ABI 及包內檔案另以校驗值記錄。

本輪沒有重新執行私有遊戲 PE 的原生距離 fixture。原有攻擊腳下研究與證據保留為上游結果，不當成本輪新測試。完整化學兵開火、核彈／彈體行為及實機聯機尚未驗收。
