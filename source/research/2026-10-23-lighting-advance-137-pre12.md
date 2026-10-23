# 地图全局光照与防空提前接敌（1.0.37-pre.12）

## 问题与范围

用户反馈外部 SHP/VXL 偏暗，并要求火控在后续空中目标进射程之前分配，避免使用无法可靠退出的 Sticky。基准为 GitHub pre.11，`09cebbc3378ddfdfa5ee99b60d9b340acf2e51c1`。先查研究索引、pre.1/pre.4/pre.7/pre.8 笔记及持久 SQLite，再核对以下证据。

支持样本 SHA-256（样本本身不发布）：

- LBW：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- zhanwang：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`

## 光照证据

- `ScenarioClass*` 位于 `0xA8B230`。`+0x352C` 是当前 Ambient，以 100 为 1.0；`+0x3530` 是过渡目标，不能拿它替代当前亮度。普通 RGB 为 `+0x3534/+0x3538/+0x353C`，以 100 为 1.0。`0x484680` 开始的原版格光照更新把当前 Ambient 乘 10，进入以 1000 为单位的光照计算。
- `ColorScheme::Array`：`0xB054D0`，items `+4`、count `+16`；scheme `+0x310` 为 ShadeCount，`+0x30C` 为 LightConvert。选择有多个色阶的单位 scheme，不取仅一个色阶的 UI scheme。
- `LightConvert::UpdateColors` `0x556090`，thiscall，4 个参数 RGB/tinted。`+0x198/19C/1A0` 保存普通 RGB；`+0x1A4/1A8/1AC` 保存临时 RGB；`+0x1B0` 选择当前一组。以 1000 为 1.0，非 -1 的输入限于 0..2000；-1 恢复已保存的一组。
- `0x53C280` 切换普通、核爆、离子、心灵控制器光照，调用 `0x53AD00` 更新各 converter。外部读取 converter 当前 RGB 与实际 Ambient，能跟随临时色调及渐变，而不是只读地图初始 INI。
- YRpp 名为 `ScenarioLighting` 的 `0x555AC0` 实际对传入 RGB 限幅、量化，不是获取当前光照的函数；没有调用它来读取场景。
- 上述原版格光照更新、模式切换和 converter 字段选择代码区，在两份样本逐字节一致。`lighting_native.py` 在两份 EXE 上各执行 5 个原始 `UpdateColors` 字段选择案例，在生成色阶之前截停；验证普通/临时/恢复/限幅，未替换被验证的字段写指令。

实现 `external_reader/source/scene_lighting.h`：只读、有界，转换为 Ambient×RGB 增益；校验跨地图/跨采样变化，缺失时用中性光照。观察窗的单位、地形、覆盖物，以及队形预览，在 GDI+ 绘制图片时使用同一 RGB 矩阵，alpha 不变，UI 文本/边框不着色。缓存保留原始姿态图像，光照变化不重新打开 MIX 或解码模型；静止队形预览也会重绘。

VXL 固定明暗改用 FA2 的 `(200 + 300 × directional) / 255` 响应及 `(1,0,-1)` 入射方向，改善旧的低环境亮度。这里仍使用模型面几何法线，尚非 FA2 的逐 voxel normal-table 光照。磁盘 PAL 已在现有解码器中由 6 位扩展，未凭“看起来暗”再把所有 live RGB 强行乘 4。

未声称复现：逐格灯源、地形高度光照、投影阴影、原版全部色阶量化、各 MOD 自定义渲染。预览的半透明保持 124/255；观察窗单位图像保持不透明。

## 火控实现与证据

此前代码使用的是 Sleep(0) 短暂留枪，并非 Sticky(6)。原有 Sticky 就绪门和普通 Attack 追击的证据沿用 pre.4/pre.7/pre.8，不把前者当作可以靠普通 Guard 在联机强行解开的任务。

- 新的 Advance=32 仅是外部规划结果：机动单位提交原版 Attack(1)，建筑在射程外提交 Sleep(0) 并保留候选目标，进入射程切回 Attack。Release 提交 Guard(5)。机动单位不发送 Sleep，所有火控都不发送 Sticky，不直接写任务、目标、目的地或联机游戏字段。
- **原版建筑例外**：`Building::Mission_Attack 0x44ACF0` 普通分支调用 CanFire 后按 `0x44B728` 跳转表分派。RANGE=8 落入 `0x44B0DE`，清目标，再 QueueMission(Guard)/NextMission。因此“所有单位直接提前 Attack”不能保证建筑留枪，已放弃这一中间方案。
- 建筑 `Mission_Sleep 0x5B2E10` 不索敌/开火；`ReadyToNextMission 0x454250` 读取 `+0x6DD`，并无 Unit 的 Sticky 任务门。正常动画就绪片段 `0x451205..0x45121F` 在无活动动画时设置此字节。`building_advance_native.py` 在两份原 EXE 上执行范围拒绝分支、Sleep、QueueMission、正常动画就绪片段及 NextMission，确认能按正常路径切回 Attack/Guard；没有强制写就绪标志作为“修复”。这仍不是整个动画循环的实机验证。
- 建筑对射程外 4 格候选带内的目标预留火力，不受机动单位 60 帧期限限制。战车、步兵用普通 Attack 让原版接近；首次提前攻击记录起点/帧。离起点达到 4 格、接敌持续达到 60 帧、目标离开起点约束范围/候选带、明确远离、死亡或不再符合筛选时，重新分配近处目标或提交 Guard。实际停止时刻仍受扫描与原版事件送达延迟影响。
- 原版 `SelectWeapon/GetWeapon/IsCloseEnough` 决定武器及真实射程；额外距离计算只限制提前接敌候选，不替代开火判定，不进入武器最小射程。包括先在射程内攻击、随后目标离开的情况，也会建立追击期限。
- 智能模式沿用 120% HP 覆盖线；尚未送达/尚未解除的建筑 Sleep 不算立即可用覆盖。即将可用的近处火力不足或目标近身威胁时，先补近处火力。覆盖足够才提前攻击后续目标；没有后续目标则正常集中开火。平均分配、仅导弹/全部空军筛选继续有效。长 CD 单位不为无法及时兑现的火力自动前进。
- 仅已确认属于本火控的 Attack 可带移动目的地继续管理。原有手动移动、手动攻击接管、队形、其他高优先级任务、显式 Sticky 都受保护。离开候选范围后的 Guard 带 30 帧再接敌间隔，避免立刻循环追赶。
- 自动攻击记录轮询：有候选时最多先占 32 个输入名额，再给世界轮询剩余名额，避免持续追击的前 64 个单位垄断扫描。128 个防空单位的受控例验证后续单位均能获得分配。
- 关闭火控或退出工具时，已经排队但尚未观察到的 Attack 也会跟随 Guard；不因“还没开始移动”遗留自动追击。

Shared 新增来源受控标志，Assist ABI 从 66 升至 67；EXE/DLL 同时更新，通信名称统一为 `Local\RA2Assist_v067_`。Runtime 仍为 4。

## 验证与限制

见 `verification/review-137-pre12/summary.json`。`missile_advance_runtime.py` 使用实际最终 DLL、原版事件构造函数，两份样本，单机/联机模式和内外两处计算；覆盖提前分配、现有锁定后进入追击、建筑/步兵/车辆、128 单位轮询、范围/时限、手动接管、未送达即关闭、退出、近处覆盖不足、紧急威胁、最小射程。断言扫描不修改单位游戏字段，不产生代码写入，机动单位事件中没有 Sleep，所有事件中没有 Sticky。

另验证实际 EXE/DLL 通信配对、原版高度/最小射程/额外防空射程，以及原有筛选、断电、防盗、过期/损坏/失效异步结果。纯 C++ 检查透明度、RGB 动态变化、静止队形重绘、缓存不重载、300 类型观察调度、武器分配和任务仲裁；真实 630 帧 SHP 经过现有资源加载链路。

最初建筑测试异常源自测试夹具在同一虚函数地址安装两个返回桩，重复弹出调用栈；步兵夹具缺失合法类型字段。128 单位测试还需要模拟送达后消费 OutList，否则会正常触发 112 条安全上限；保留原版队列背压，未抬高限额。这些修正了测试夹具，未据此修改生产攻击入口。

这是受控计算、内存读写边界和原版排队验证，不等于完整游戏走位、炮塔预瞄、弹道命中、Windows GDI+ 画面或多人同步已实机通过。伤害估算仍基于武器基础伤害/装甲系数，不含完整在途弹丸账本和实际命中概率。

参考：

- [YRpp ScenarioClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ScenarioClass.h)
- [YRpp ColorScheme](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ColorScheme.h)
- [YRpp ConvertClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ConvertClass.h)
- [EA Mission Editor 源码](https://github.com/electronicarts/CnC_TS_and_RA2_Mission_Editor) 的 `IsoView.cpp`、`Loading.cpp`、`MissionEditorPackLib.cpp`（本地已核对的公开源码）。
