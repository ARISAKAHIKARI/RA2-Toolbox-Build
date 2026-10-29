# 渲染升级时停止原生重画：阶段分离与当前限制

## 问题与证据边界

用户询问拖屏是否完全解决，以及开启渲染升级时能否停止原生游戏绘制以节约性能。调查前检索 INDEX、[强制重绘/DDraw](2026-10-23-refresh-watch-137-pre2.md)、[原生反馈保留](2026-10-29-native-order-placement-feedback.md)、[滚屏/性能修复](2026-10-29-render-feedback-performance.md)，并查询持久 SQLite v80 中的 `4F4480`、`6D3D10` 与 render gate 条目。

本次为私人授权 PE 的静态分析及原指令受控分支执行。没有修改游戏代码、虚表或游戏数据，没有实现原生渲染抑制，没有 Windows 游戏画面、硬件性能或多人同步实测。不得把“识别到阶段”写成“停止原生绘制已经实现”。

| 样本 | SHA-256 |
| --- | --- |
| sample3 | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| sample4 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |
| sample5 | `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707` |

不提交私人 EXE、原始指令、转储或反汇编文本。精简 hash/受控调用记录见 [native-render-gate.json](evidence/native-render-gate.json)，可重跑工具为 `source/tools/native_render_gate_audit.py`（需要 pefile、Unicorn 和用户提供的私人样本）。

## 原生渲染不是一个可直接关闭的末尾 Present

`Map::Render @0x4F4480`，thiscall、无栈参数。静态初始化 `0x40D1B2` 将全局对象 `0x87F7E8` 的虚表设为 `0x7E1964`，该表的 MSVC RTTI 为 `MouseClass`；`+0x3C=0x4F4480`、`+0x40=0x6D0A20`。

Map 原生调用顺序为：

1. 读取并清除 Map `+0x0C` 重绘级别，保存当前绘制 surface。
2. 调 `Tactical::Render @0x6D3D10`，phase 0，再 phase 1；ECX 为 `[0x887324]`，栈为 surface、bool redraw、int phase，`ret 12`。
3. 调 Mouse 的 virtual `+0x40`（`0x6D0A20`）；该函数处理原生提示图像，并在 `0x6D0E38` 调 Sidebar `0x6A6C30`。这条链不能当作无用世界绘制一起关闭。
4. 再调 Tactical phase 2，然后执行其他绘制/鼠标恢复/提交路径并恢复之前的 surface。

`Tactical` 的 phase 2 在 `0x6D3DD0..0x6D3DD3` 直接跳到 `0x6D4758`，绕过 `0x6D3E9B..0x6D457B` 的原生世界缓冲、滚动/脏区处理，继续原生反馈阶段 `0x6D45F2`。后者包含：

- `0x6D4651/0x6D46D8 → 0x6D5030`，其中 `0x6D524F/0x6D56F3 → Cell::DrawPlacement @0x47EC90`，即红绿格/摆放预览。
- `0x6D4735..0x6D4750`，选中物件的原生 action lines；具体逻辑已见此前反馈笔记。
- 其他原生反馈、规划、屏幕效果和表面拷贝。

这说明“只省世界重画，保留 UI”有可研究的阶段边界，但不能仅跳过最后的 DirectDraw Present 就节省前面的 TMP/VXL CPU 光栅工作，也不能跳过整个 Map Render 后声称原生反馈仍完整。

## 为什么清除重绘位不是稳定的世界关闭开关

即使传入 redraw=false，phase 0/1 仍检查实际状态：

| 状态 | 被检查的位置/字段 | 结果 |
| --- | --- | --- |
| 当前可见动态对象查询 | `0x6D3D50 → 0x6D9B50` | 有需要绘制的动态内容时进入世界处理 |
| 原生脏区列表 | count `0xB0CE88`、items `0xB0CE7C` | 非空时进入世界处理 |
| Tactical 重绘标记 | `Tactical+0xD7D` | 为真时进入世界处理 |
| 相机及滚动缓冲位移 | `+0xD64/+0xD68` 与 `+0xD6C/+0xD70`，以及 `+0xB0/+0xB4/+0xB8/+0xBC` | 实际移动仍触发世界处理 |

纯静止、无脏区、无动态内容时，原生本身就可跳过世界重建而复用画面。工具强制清掉这些状态会伪造渲染 bookkeeping，并不能据此保证下一次恢复、Z、遮挡和原生反馈正确。本次未写这些字段。

`0x53BAE0` 返回 `[0xA9FAB0]!=0`，Map 在其为真时绕过三个 Tactical 调用及 virtual `+0x40`。但 `0xA9FAB0` 是现有 0..3 屏幕表面过渡状态机的一部分，`0x53B560` 的分支会倒计时、切换 `0x88731C`、调用 FlagRedraw、嵌套 Render 和释放过渡表面。它不是可借用的“只关地形/单位”开关；直接写非零还会停止所需的原生 UI 链并触碰其他过渡状态。

## 当前决定与未来所需验证

当前只读渲染升级不停止原生游戏绘制。没有找到已验证的独立数据开关，可以省掉世界重画同时保持所需反馈。选择性截取上述 direct call / 分支可以作为独立实验方向，但会引入新的原生渲染接管；常规修改调用点/跳转会写可执行代码，不符合当前不改代码段分支。仅替换对象虚表也不能被称为完整截取：已找到多个对 `0x4F4480` 的直接 call。

即使将来有其他不写代码段的调用接管方法，仍需先验证：世界色/Z 缓冲及脏区状态、phase2 对此前缓冲的依赖、原生红绿格和特殊 MOD UI、工具暂时未出图/失焦/菜单/设备故障时的即时恢复、不同平台原有 detour，以及逻辑/联机入口不受影响。原生 UI 区域当前有透明 cutout，不能在其下把原生画面停成永久旧帧。

拖屏连续性仍需宿主缓存/相机提交优化。原来的96原生像素 overscan 能覆盖有限位移；它不等于无限拖动或新场景未完成时都能保持新画面。不能以冻结旧场景或关闭原生后露黑作为“拖屏闪烁已解决”。

## 受控验证

- 三样本各18个 Tactical admission 案例，共 **54 PASS**：phase0/1 的静态复用、相机移动、脏区、显式重画、Tactical flag、可见动态查询；phase2 在同组条件下均到达反馈阶段。
- 未修改 Map body 的 sample3/sample5 各12个案例，共 **24 PASS**：redraw 0/1/2 × coarse gate 0/1/2/3，确认 0→1→UI→2 调用顺序、正确 redraw bool、粗 gate 对全部阶段的影响、重绘级别消耗及 surface/栈恢复。Tactical、surface 和辅助绘制调用为明确替身。
- sample4 的 Map body 有实际平台 detour（如 `0x4F4483→0xB7E4B1`、`0x4F44C2→0xB7E56A`），未移植其他样本 bytes 来伪造通过；该 Map 完整 trace 标为不执行。它的相同 Tactical admission 前缀仍执行了18案，停止在后续平台 detour 之前。

这些结果证明控制流与候选分离边界，不证明完整原生 draw、Windows 画面、游戏 FPS、原生渲染抑制或联机安全。
