# 副相机建筑附属动画的静止显示准入

基线：GitHub main / `v1.0.41`，`513aeb4cf03238cce956bd35e0c9381c2a7cc484`。

用户回报副相机缺少建筑动画结束后应显示的静止部分，例如建造厂夹子。本轮先读取 INDEX、原生动画 RenderFix2/3、建筑独立部件与恢复/buildup 笔记，并按文件名查阅现有 `RA2-Reverse-Engineering-Notes.sqlite`。当前连接器没有 Library 文件入口；本机同名笔记只有 13 条较早记录，没有本问题的结论。本篇记录可证明的读取器准入修正，**没有把构造的静止状态说成已观察到的建造厂终态，也没有完成用户现场的游戏视觉验收**。

## 原生指令与读取器差异

支持的私有游戏 EXE SHA-256：
`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。

- `Anim::DrawIfVisible 0x422C70`，thiscall，3 个栈参数，`ret 12`：先检查 `LoopDelay +0x184`，然后调用 `Object::DrawIfVisible 0x5F4B10`。后者在本轮受控的有效窗口/重绘条件下拒绝 `InLimbo +0x81`，并调用虚表 `+0x114` 的 `Anim::Draw_It 0x422CA0`。这两段与 Draw_It 都不以 `IsAlive +0x90` 拒绝绘制。
- `Anim::Draw_It 0x422CA0`，thiscall，2 个栈参数，`ret 8`：`Invisible +0x19D` 在 `0x422FEC` 拒绝绘制；显示帧在 `0x42303F..0x423061` 仍为 `Animation.Value +0xAC + AnimType.Start +0x2B4`。没有为末帧改用 `PausedAnimFrame +0x11C`，也没有从 LoopEnd 反推一个未观察到的帧。
- `Anim::AI 0x423AC0` 在 `0x4243AE` 检查 `IsAlive +0x90` 后停止推进。此处是更新条件，不能直接用作读取器的附属动画绘制条件。构造器设置 `+0x90=1`；`Object::UnInit 0x5F65F0` 在 `0x5F6625` 清零。因此仍不能将任意 `IsAlive=0` 指针当成合法动画。
- `TimeToDie +0x179` 并不是上述 Draw_It 的直接拒绝条件。读取器继续保留这项既有生命周期保护；本轮没有放宽它。`+0x19D` 是 Invisible，不是“末帧完成”。

原先 `building_animations.h::capture` 对所有动画无条件要求 `IsAlive=1`，比原生绘制准入更严格。现在仅在当前有效父建筑的槽成员关系、父/动画 ID、动画 Type、OwnerObject 经前后核对一致时，允许 `IsAlive=0` 的精确已观察姿态。新增 InLimbo 拒绝；原有 TimeToDie、Invisible、坐标、深度和身份保护保留。三个拒绝标志在采集开始和返回前都必须实际读取成功且为零，不能把读失败默认成零；全局 IsAlive 亦要求前后读取成功且为真。没有父槽的全局动画仍要求 IsAlive。槽删除、身份变化或销毁标记出现就拒绝，不保留已删除生产动画的图像。

## 原素材核查及没有采用的修法

仅在本地读取现有 MIX/ART/SHP，公开记录只保留字段、计数与结果。该安装的 GACNST_B 有 80 帧，前 40 帧有颜色像素、后 40 帧为空；GACNST_C 有 4 帧，前 2 帧有颜色像素。ARTMD 的 GACNST_B 配置 LoopCount=1，受损范围从 Start=20 开始。主体 GACNST/GGCNST 的前 3 帧亦非空。**像素数量不能证明夹子出现在哪一层，也不能证明该安装配置等同用户当前对局。**

既有原生证据及本轮复核继续支持：主建筑 SHP 使用 `min(GetCurrentFrame, FrameCount/2)`；槽动画使用 Value+Start。LoopCount=1 的生产动画可以在原生更新路径结束并 UnInit；不能在槽消失后凭文件名 `_C` 或已有 `_B` 素材制造常驻层。原素材存在 `_C` 也不代表当前类型已配置 IdleAnim。本轮没有增加静态 `_C` 猜测、帧推进、末帧减一、Pause 字段替代或新的游戏 Hook/写入。

## 可复现验证

- `assist/tests/building_animation_draw_native.py --exe <私有受支持 EXE> --output <报告>`：SHA-256 门限、映像映射、38 组受控原生 x86 调用；IsAlive=0/1、Value=0/18/19、Start=0/20 的提交帧一致；20 次静止重绘不推进；InLimbo/Invisible 拒绝。报告保存执行区间哈希、计数及帧号，不保存原始指令或素材。投影、Image getter、地格/光照标量和最终 DrawSHP 是测试边界，原生准入与帧选择指令不替换。
- `assist/tests/building_animations.cpp`：生产 attached/capture 的静止 frame=39 重复采集；全局、InLimbo、TimeToDie、Invisible、无 ID、失效父 ID、槽删除负控，以及既有坐标/资源/深度回归。新测试配旧头文件必须在静止采集断言失败。
- `assist/tests/building_animation_hold.py`：同一新 C++ 测试分别编译当前头文件和冻结的 GitHub 513aeb4 原始头文件；旧 guard 负控以退出码 31 受控失败，不触发 abort/WER；聚合原生 38 组与编译输入 SHA-256，前后源码哈希必须稳定。
- Source.zip 已附 `assist/tests/fixtures/building_animation_baseline/building_animations.h` 及同目录 `manifest.json`；在解压后的 `source` 目录执行 runner 时，将该头文件传给 `--baseline-header`。文件按 513aeb4 Git blob 与 SHA-256 双重核验，保留原始字节；私有 EXE 仍须用 `--exe` 显式提供。
- `assist/tests/game_assets_host_test.py`：生产 captureAnimation 与 SHP 解码/像素输出在 24 帧合成 SHP 的同一已观察 frame=23 连续成功，且生命期负控拒绝；未以合成素材声称真实夹子的逐像素匹配。
- `assist/tests/building_animation_pixels.py`：独立抽取上述公开 fixture 的文件/时间/内存边界与辅助函数，只运行本轮最小设置；20 次捕获、240 个 RGBA/索引像素、共用不可变图像缓存、Value 不推进及销毁/槽移除负控。Windows 使用实际系统 ABI 声明；不依赖旧 fixture 的 Surface/overlay 断言。旧完整 host fixture 在本机 Windows 改编下先出现既有 Surface 明暗像素断言失败，未运行到新增段，不能据此声称完整 host fixture 通过；未扩大修改旧 Surface/overlay。

受控 x86 准入、构造内存、公开合成 SHP 和编译通过都不等于 Windows 副相机最终显示、用户所见夹子终态或多人验证。若用户现场仍缺失，需要对具体动画槽、IsAlive/InLimbo/Invisible/TimeToDie、Value/Start、SHP 来源和副相机完成画面共同取证，继续区分采集、素材和呈现路径。
