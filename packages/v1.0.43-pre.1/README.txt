RA2 Toolbox 1.0.43-pre.1

首次使用：完整解压 Full 包，运行 RA2Toolbox.exe。
已有完整安装：退出游戏和工具箱，将 Update 包覆盖到原目录。
保留原有 Toolbox.ini、Assist.ini、settings.ini、assets 和 cache。
主程序、GameRuntime.dll、GameRuntimeHost32.exe 和 WebView2Loader.dll
必须成套更新。本版 Assist 协议为 97，请勿混用 1.0.42 的运行组件。

需要 64 位 Windows 和 Microsoft Edge WebView2 Evergreen Runtime。
游戏本体、地图和 MOD 不包含在本包中。

本版主要更新：
- 显示设置新增单机游戏 FPS：0 恢复原版，10–240 可配置，如 40、45、50。
  此设置控制游戏逻辑速度，与渲染刷新率分开；联机不生效。
- 保留建筑有效槽位中已停止、仍由原生绘制的动画及其末帧。
- 星级标志使用普通 SHP 的缩放和合成，与其他素材一起缩放。
- 保留并整合基地车展开、运输、驻军和坦克碉堡等副相机跟踪修复。
- 合入 CPU 局部重绘、不可变地形任务缓存和共享工作线程优化。

本版本为预发布版；1.0.42 仍为正式 Latest。
已执行的离线和受控回归范围见 GitHub Release。
实机视觉、帧率、物理 GPU 性能与联机效果尚未完成验收。

