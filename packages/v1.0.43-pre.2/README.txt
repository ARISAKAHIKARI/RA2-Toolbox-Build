RA2 Toolbox 1.0.43-pre.2

首次使用：完整解压 Full 包，运行 RA2Toolbox.exe。
已有完整安装：退出游戏和工具箱，将 Update 包覆盖到原目录。
保留原有 Toolbox.ini、Assist.ini、settings.ini、assets 和 cache。
RA2Toolbox.exe、GameRuntime.dll、GameRuntimeHost32.exe 和 WebView2Loader.dll
必须成套更新。本版 Assist 协议为 98，请勿混用旧版运行组件。
需要 64 位 Windows 和 Microsoft Edge WebView2 Evergreen Runtime。
游戏本体、地图和 MOD 不包含在本包中。

矿车自动点杀蜘蛛
- 入口：自动功能 → 矿车自动点杀蜘蛛。默认关闭，独立于矿车自动卸矿。
- 额外索敌距离 N 可设 0–30 格，默认 1 格。
- 敌方地面寄生单位进入矿车武器射程＋N 格时，工作中的己方武装矿车
  自动下达攻击命令；目标离开射程＋N＋1 格或威胁解除后恢复原工作。
- 通过游戏原生命令恢复采矿、卸矿或等待卸矿；玩家手动命令优先。
  工具箱维护的排队意图会保留，游戏原生卸矿队列可能需要重新排队。

单机自定义 FPS
- 入口：视频 → 光标与渲染 → 渲染与效果 → 单机游戏 FPS。
- 保留 pre.1 的设置：10–240 可配置，例如 40、45、50；0 恢复原版。
  控制单机游戏逻辑速度，与渲染刷新率分开，联机不生效。

本版为预发布版，1.0.42 继续作为正式 Latest。
实际完成的构建与验证范围见 GitHub Release 和附件清单。
离线检查与受控测试不能代替真实游戏和联机验收。
