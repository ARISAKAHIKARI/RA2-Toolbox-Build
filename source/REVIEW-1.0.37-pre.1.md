# 1.0.37-pre.1 实现与验证

基于 GitHub main `b6911ba562010ab3b5b07f766b34625e72f811fe`（1.0.36）。文件版本 1.0.37.0，Assist 62，Runtime 4，统一入口管理器 3。EXE/DLL 需成套更新。

## 火控与磁电

火控将即将进入射程的敌方空中目标也纳入分配，为后续目标预留炮位，避免所有单位自动抢打第一个目标。预留通过原版 Sleep 事件执行；真正攻击仍以原版射程、武器与空中判定为准。配置可独立选择“仅导弹类 / 所有敌方空中单位”和“智能分配（血量与伤害）/ 平均分配”。预判最多 30 帧，范围带最多额外 1024 leptons。手动命令优先，停用和正常退出会释放本功能的预留任务。

普通 Guard 待机会自动索敌。本功能临时使用的 Sleep 不自动索敌，以免预留炮位仍抢打前排；不用预留后恢复 Guard。已发出的炮弹不能追回，本版没有弹体飞行中的伤害账本，仍可能产生部分过量射击。

磁电自动摔落新增正下方的树、矿柱、地面坦克。低空悬浮、飞机、磁电举起对象、空中层、非同格对象和桥面被排除。树/矿柱属于释放机会，不保证必定摔死目标。全局无限吸独立启用后，从空闲己方磁电中建立原有规划器的续接任务；手动移动/攻击期间不抢控制，关闭撤销全局续接。

## 队形与外部预览

默认按住 Alt，右键拖动；拖动方向为面向方向，距离越长横向越宽，320 个游戏像素时变为一横排。小于 6 像素按点击处理，直接聚团。双击 Alt（350 毫秒内）进入一次性排队模式，一次提交后退出；Esc、失焦、聊天或关闭功能取消预览。按住键可另行配置，双击 Alt 入口保持可用。

| 默认键 | 队形 |
| --- | --- |
| Z | 接近 √2∶1 的矩形横队 |
| X | 同类矩形，中心间距约两格 |
| C | 圆形聚集 |
| V | 约 60° 的扇形，弧长与纵深约 3∶1 |

每种队形都可独立改键，也可把几种或全部绑定为同一键；重复敲击按表中顺序轮换，只在排队模式中屏蔽该键的原功能。系统长按重复不会连跳。最多处理 256 个选中的己方地面步兵/载具；建筑和空中单位不参与。

预览由外部 EXE 读取已载入的真实 VXL、HVA、车身/炮塔/炮管与玩家色盘，自行解码、生成面、深度绘制和半透明混合，合成到现有叠加层。没有调用游戏绘制器。非 VXL 单位可显示实际 SHP 首帧，读取失败时显示未载入数量。采样约 30 Hz，32 档朝向；模型、图像和每次绘制工作量均有上限。

最终命令走原版 Move 事件：先到目标后方一格，再到目标格，以最后一段移动引导朝向。未验证任意角度转向事件，因此实际朝向受格点、地形和原版寻路影响。预览避开建筑/地形物并避免重复占格；原版继续决定可达性与拥挤处理。实际到达位置不保证完全复现预览。

## 验证

详见 [机器可读摘要](verification/review-137/summary.json) 和 [逆向证据](research/2026-10-23-fire-formation-137.md)。

- 13 项算法/UI 桩测试通过；队形覆盖 4096 种数量、形状和拉伸组合；VXL 覆盖 span 解码、边界、坏指针、HVA 平移只应用一次、深度和预乘透明度。
- 两份游戏样本，每份 36 项火控、28 项磁电、26 项队形受控测试，共 180 项通过。运行实际 DLL/事件构造器，世界、Win32、武器查询和到达状态使用桩。包括后排进射程后预留炮位转为攻击、停用恢复、手动接管、同键轮换、输入取消、跨帧、自定义按住键与 16 单位分批移动。
- Windows PE32 构建、导出、协议/共享布局、manifest 和依赖检查通过。`git diff --check` 通过。
- `no_interception_source_contract.py` 和 `shared_entry_source_contract.py` 两项旧源码检查失败；在未修改基线也以同一断言失败。前者仍禁止现有入口管理器的 VirtualProtect，后者仍要求旧 input_patch 路由的 gameentry::patch。保留失败日志，未删除或放宽断言。

尚未进行 Windows 实机画面、实际弹体/摔落物理、完整寻路或双机联机验证。VXL 首帧 HVA、炮塔偏移换算、特殊 MOD 的变形/多炮塔资源、SHP 朝向动画和真实比例仍需实机校准；当前光照为简化面光照。此版本按测试版交付。

## 复现

`ZIG=/path/to/zig bash source/build-zig.sh`，然后运行 `python3 source/tools/verify_windows_outputs.py`。

可移植 C++ 用例使用 `g++ -std=c++17 -O2 source/assist/tests/<case>.cpp -o /tmp/<case>` 编译后执行。受控运行需 pefile、capstone、unicorn 和用户自行提供的对应游戏样本：

```sh
python3 source/assist/tests/missile137_runtime.py GAME_EXE source/GameRuntime.dll --output missile.json
python3 source/assist/tests/magnetron137_runtime.py GAME_EXE source/GameRuntime.dll --output magnetron.json
python3 source/assist/tests/formation_runtime.py GAME_EXE source/GameRuntime.dll --output formation.json
```

仓库和测试包不包含私人游戏 EXE 或原始内存转储。新发现已写入研究索引及同一份持久 SQLite 笔记。
