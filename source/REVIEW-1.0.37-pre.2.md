# 1.0.37-pre.2 实现与验证

基于 main `b6911ba562010ab3b5b07f766b34625e72f811fe`（1.0.36），包含 pre.1 全部改动。文件版本 1.0.37.1，Assist 62 / Runtime 4 / 入口管理器 3。升级须一起替换 EXE/DLL，保留现有配置与资源。

## 高刷

「画面设置」高刷模式现在有四项：仅光标、光标＋拖屏、全部、强制高刷（实验性 · 联机待验证）。第四项在单机、局域网和互联网会话均可选择，没有单机限定。它按目标节奏请求原生全场景重绘，再调用原绘制入口，不主动推进游戏逻辑帧。

普通「全部」可能重用现有表面；强制模式额外触发 FlagRedraw(2)。未知或被替换的关键函数签名会拒绝启用。失焦、聊天、菜单、关闭功能会停止额外绘制；退出恢复受管理的拖屏适配器。界面「绘制调用 Hz」不是显示器实测 FPS。单位移动/动画仍受游戏逻辑频率限制，这一版不做世界状态插值，也未验证真实联机同步。

新增「配置 DDraw 高刷…」和「恢复 DDraw 配置…」。游戏运行且工具箱显示启用后，识别已加载的 CnC-DDraw / TS-DDraw，再选择该游戏实际使用的 ddraw.ini。CnC 修改当前游戏节及其变体的 maxfps/minfps；TS 修改 TargetFPS。默认/自动目标使用显示器刷新率。修改后须重启游戏，vsync、游戏逻辑速度等原配置保持不变。保留原始备份；若文件之后又被其他程序修改，自动恢复会停止并给出备份路径，不覆盖这些修改。没有内置或替换第三方 DDraw DLL。

## 局部观察窗

「战场信息」新增左中 11×11 格观察窗和危险载员自动跟随开关。默认 Ctrl+Alt+W 固定鼠标悬停单位，未悬停时使用首个选择单位；再次对同一目标按键解除。支持改键。手动固定优先，目标死亡或身份失效后回到自动跟随。自动目标是装载工程师、C4、伊文、潜入或自爆等危险乘员的敌方载具，保持跟踪到目标失效或乘员危险状态消失。

读取中心周围 ±5 格，将已加载 TMP 地表、格高、悬崖额外图像、SHP 树木和覆盖物，与 VXL/HVA 或 SHP 单位模型合成到外部叠加层。不切换、不移动主视角；采样 20 Hz，模型、贴图缓存和每次解码数量有上限。读取不到的资源临时显示地块色/阵营标记，并明确显示未加载数量。

这比纯地块示意更接近第二相机，但尚不是完整的原生相机：单位固定朝向，SHP 单位使用首帧，VXL 简化光照；未复现原生逐像素跨物体深度、阴影、桥层、所有地形变体、动态光照、特效和完整迷雾遮蔽。因此复杂 MOD、桥梁、斜坡、高低地遮挡仍需实机校准。

## 队形与此前功能

拖动方向现在按原版 30:15 等距投影反算世界朝向，短拖停在同一格也更新方向。修复模型/朝向缓存满后预览持续缺失、读取到正在发布的快照时短暂消失等问题；缓存始终有界。Alt 拖动、双击 Alt 一次性模式、ZXCV 同键轮换及原版 Move 提交保持可用。

火控、磁电和队形完整操作见 [pre.1 说明](REVIEW-1.0.37-pre.1.md)。普通 Guard 待机会自动索敌；火控预留炮位临时 Sleep，释放时恢复 Guard。

## 验证与证据

最终二进制、逐项结果和样本哈希见 [摘要](verification/review-137-pre2/summary.json)；本次反汇编、ABI、格式、否定假设和未决问题见 [研究笔记](research/2026-10-23-refresh-watch-137-pre2.md)。

两份样本共 **296 项受控测试**、**12 项可移植/UI 测试**通过；4 项解析/缓存测试另通过 ASan 与 UBSan（环境无法运行 LeakSanitizer，未宣称泄漏检查通过）。

可移植算法、设置与 UI 桩覆盖 DDraw PE 导出识别/配置保留/重复应用、121 格采样、TMP 悬崖/色盘/SHP 帧解析、队形朝向与有界缓存、键盘和界面布局。两份私人游戏样本执行实际 DLL 和原生地表访问函数，世界、Win32、呈现表面和部分渲染函数使用桩。测试能证明请求与状态处理，不能证明实际屏幕 FPS、完整物理/寻路或联机兼容。Windows PE32 版本、导出、共享协议、manifest 与依赖检查通过。

pre.1 记录的两个旧源码契约检查基线失败仍保留，未放宽这些检查。本次没有把它们列为通过。

复现：`ZIG=/path/to/zig bash source/build-zig.sh`，再运行 `python3 source/tools/verify_windows_outputs.py`。可移植测试用 g++ C++17；原生受控测试需要 pefile、capstone、unicorn 和用户自己的对应样本：

```sh
python3 source/assist/tests/scroll_refresh_runtime.py GAME_EXE source/GameRuntime.dll --output scroll.json
python3 source/assist/tests/formation_runtime.py GAME_EXE source/GameRuntime.dll --output formation.json
python3 source/assist/tests/terrain_native_runtime.py GAME_EXE source/GameRuntime.dll --output terrain.json
python3 source/assist/tests/missile137_runtime.py GAME_EXE source/GameRuntime.dll --output missile.json
python3 source/assist/tests/magnetron137_runtime.py GAME_EXE source/GameRuntime.dll --output magnetron.json
```
