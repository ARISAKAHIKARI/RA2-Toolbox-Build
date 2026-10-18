# RA2 Toolbox 1.0.8 测试版

- 修复建造预定图标、文字、位置和点击区域在 2K 游戏画面放大到 4K 窗口时未统一缩放的问题。
- 建筑、防御各支持 5 项预定，显示 1–5，支持重复项；左键追加，右键取消该项与后续项。
- 没有电厂但正在造电厂时，可以预定灰色兵营；取消当前电厂会清空后续预定。修复前置预测中的国家哨兵值、父国家、偷科技标志。
- 生产前由 GameRuntime 检查原生图标、空闲队列和原版 CanBuild；使用既有生产构造通道，没有新增 Hook。
- 停工提醒仅建筑与载具栏，读取游戏实际加载的页签 SHP 和调色板换色闪烁。防御、步兵不闪烁。

36 项实际 DLL/构造器模拟用例、宿主队列和 SHP 读取 ASan/UBSan 测试、6 种分辨率变换及 UI/架构验证通过。尚无真实 Windows/LBW/平台双机联机验证，具体 LBW 重绘是否有裁切或黑边仍待实机确认。

[本版实现与验证](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/main/source/REVIEW-1.0.8.md) · [源码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/main/source)

Windows.zip 为完整运行包；已有 1.0.2 或更新完整目录可使用 Update.zip。退出游戏与工具后配套替换 EXE/DLL，保留配置和缓存。Release 附件仅运行文件。
