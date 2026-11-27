# Offline native toolbar extensions (development branch)

Two independent, default-off options under 游戏规则 → 维修与变卖. Sessions recognized by the existing event delivery gateway (0/5 single player, 3/4 multiplayer) and live local-owner targets are admitted. Multiplayer uses the common native EventClass submission path. Native Repair/Sell modes are observed, not changed. Existing native allowed cursors keep their own input; the extension is offered only on the corresponding denied cursor and a validated hover target. Click uses an observed down/up pair and the original event gateway, with current mode/owner/object/viewport/camera/UID revalidation. No new hook, game code write, building type write or direct money/HP edit.

Vehicle sale retains the existing building-cell requirement of the verified native receiver. Repair uses the building's Repairable and ClickRepairable attributes as read-only observations. The preview is not an event receipt. Real Windows game verification is still required for exact cursor timing, game builds, repair toggling, and sale refunds. Multiplayer admission is implemented; real peer synchronization and platform compatibility remain unverified.

Protocol 117 appends the point and hint structures; replace Host and Runtime together. This branch must not publish over 1.0.53.1 automatically.
