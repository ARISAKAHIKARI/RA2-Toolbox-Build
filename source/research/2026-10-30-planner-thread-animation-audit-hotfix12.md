# Hotfix12：持续规划线程、固定处理名额与动画路径审计

## 问题与证据边界

用户要求继续查类似原先每轮 8 个素材的瓶颈，特别是全局磁电等持续计算对主线程的占用；并检查倒矿、下令线、超级武器等动画类别。调查前已读取研究索引、建筑动画／原生反馈／磁电分端记录，以及同一持久笔记 v87。沿 Hotfix11 的源码继续本地构建，没有触发 GitHub Actions。

本轮区分工具箱 UI 线程、工具箱计算线程与游戏线程。受控 C++、实际编译 DLL 的模拟执行、指定原生代码段执行均不等于 Windows 游戏画面、实际 GPU 帧率或两台机器的联机验证。没有新增游戏调用入口、Hook、写入原函数或共享协议字段；Assist80 / Runtime4 / Shared4169840 保持。

用于原函数核对的既有授权 EXE：

| 样本 | SHA-256 |
| --- | --- |
| sample3 | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| sample4 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

## 持续计算原来确实占用 UI

`main.cpp::servicePlanner()` 在 UI 消息回调中同步调用 `Planner::step()`。选择“工具箱解算”只选择宿主进程，并未把纯规划放到后台。`serviceCalculationTasks()` 的 dodge、burst、missile、unload、load、repair 六类值计算同样同步执行；装载的 512 人／128 载具合成算法测量曾出现约 5–7 ms 的纯计算，不能称为没有 UI 阻塞。

现在 `planner_task.h` 的持久工作线程独占 Planner 状态；UI 只复制不可变 World、依次交付 accepted/rejected/expired 回执，并校验结果后写原有计划邮箱。纯计算线程不持有 Shared、HWND 之外的游戏对象访问能力，也不调用引擎、发令或写游戏内存。六类无状态计算各用按需启动的持久 `ValueTask`，一个执行项和一个可替换等待项限制队列内存。

桥接 generation、任务 serial、seqlock 和对局身份拒绝旧结果。规划输入和输出保留原始 match/tick/frame；完成后仍重新检查设置、聊天与 Tactical 的 150 ms／6 帧时效。超时订单以 Expired 回执交回规划器，不算目标拒绝／寻路失败。完成通知回到 UI 现有 DataMessage 路径，不额外等固定 8 ms 轮询才开始发布。`Latest` 的可选完成回调在锁外执行；其余使用者保持原行为。

游戏线程仍负责实际引擎查询、世界采集、原生准入和 OutList。用户选择“游戏内解算”时，Runtime 的 `Planner::step()` 也仍在游戏线程；没有擅自改变已保存的解算位置。协同点击的外部采集／分配也还在 UI 上，已减少其重复读取，但不能说所有工作都已离开主线程。

`indexWorld()` 原本每逻辑帧已遍历全场建立 ID 索引，全局磁电又遍历全场检查 live/owner/武器。现在在同一次索引遍历中记录己方载具候选，全局磁电只检查候选；保留原身份、手动 token、暂停、任务、目标和载具状态检查。切 House、功能开关或帧变化会更新候选。它减少额外全场扫描，不删除原生查询或强制其他功能在工作线程访问对象。

## 固定名额及重复工作

| 路径 | 确认的问题 | 本轮处理 |
| --- | --- | --- |
| 副相机 live VXL | 每轮最多 256 个新姿态；连续变姿态时排序靠后的对象可能饥饿 | 独立游标按耗时轮转，保留面数／内存保护；预算恰为 N−1 时也不跳过末项 |
| 副相机 TMP／覆盖物 | 固定每种缓存 4 项，重复／失败项影响吞吐 | 去重基础上用耗时预算和轮转游标；不把缓存命中算工作 |
| 副相机色盘维护 | 对非当前视图、已消失动画的旧素材也刷新 | 只刷新当前需要的源；保留身份与色盘失效保护 |
| 主视图 live VXL | 缓存命中仍计入 2M 面数，超过 2048 个姿态直接丢整帧 | 只有实际新光栅计入面数，移除独立 2048 项整帧拒绝 |
| 普通命令线／选中血条／原生弹药载员 UI | 三处选区 >512 直接失败 | 与已有 8192 个对象快照验证边界一致，不因第 513 个选中单位拒绝整批 |
| 电弧采集／效果层 | >128 条 EBolt 直接拒绝整批，另有 512 容量验证 | 允许至 8192 的有界快照，并在构造电弧折线前剔除完全离屏段 |
| 协同候选排序 | comparator 内重复读动态坐标／ID；对入选成员反复线性查找 | 先冻结距离／ID 键再排序；成员使用集合查询 |

保留的 24 项计划邮箱、协同 512×512、World 8192 等是现有 IPC／内存／发令预算。本轮没有把它们当成素材解码名额任意增大；实际引擎事件仍经过统一的时间和额度检查。

`cooperative_snapshot.cpp` 的 1000 个候选保持 clicked-first／距离／ID 次序和分配结果。对比原 HEAD 的同一采样函数，X 坐标读取请求由 **58,948 降至 3,024**，每个候选最多 **1010 降至 4**。这是构造内存 Reader 的逻辑字段访问次数，不是 ReadProcessMemory 系统调用数、实际游戏时间或 FPS。页面缓存还会进一步影响真实跨进程读取次数。

## 动画透明度和渐隐

旧动画状态没有携带原生透明度档位，素材随后被 `camera::opaque()` 一律变成不透明。现在把选档值放入动画 Pose，参与像素缓存键；源 SHP／色盘缓存不因透明档变化重复捕获。`Image::intrinsicAlpha` 保留真实效果透明度，CPU／GPU 场景记录共用此素材。

新核对的原生代码段为 `Anim::Draw_It 0x423061..0x4231F3`，验证执行到 `0x4230FE` 之前停止，或记录隐藏分支 `0x4238A3`；不执行后续图像虚调用或原生 blitter。

| 字段 | 原生使用 |
| --- | --- |
| Anim `+0x190` | 初始 AnimFlags，低位混合标志 2/4/6 |
| Anim `+0x119`、Type `+0x368` | UnderTemporal 及 DoubleThick 对混合标志的附加 |
| Anim `+0x178` | 实际选档代码按 0..15 级处理；不能照上游旧注释当 1..100 百分数 |
| Type `+0x2D8`、全局 `0xA8EB78` | 透明细节阈值与当前细节 |
| Type `+0x36A`、`+0x2C0` | Translucent 与 End；Value 分别跨 End 的 20%/40%/60% 时改变档位 |
| Type `+0x2EC` | 固定 Translucency，25/50/75 |
| Anim `+0xAC`、Type `+0x2B4` | 渐隐使用原 Value；SHP 仍按 Value+Start 显示 |

二进制 double 的 .6 略小于 3/5，.4、.2 略大于对应分数；x87 扩展精度乘法的整数等号边界不能简单用宿主 double 四舍五入替代。产品 helper 用整数条件表达已核对边界。两份 EXE 共 **11,552** 组不同标志、细节、Value、End、透明参数、level 的原代码执行，与编译 helper 完全一致。像素重绘采用预乘 alpha 192/128/64；此测试不认证原生 RGB565 blitter 的每像素舍入完全相同。

普通 Foot 命令线核对 `0x4DC060` 到 `0x7049C0` 的调用参数：移动、攻击、暂停和最后导航目标样例的 animated/shadow 均为 false。不能为了用户说“下令线动画”，给普通实线虚构滚动效果。规划模式的动态节点属于另一路径，当前仍缺。

## 动画类别覆盖与未决问题

完整表见 [ANIMATION_RENDER_COVERAGE.md](../ANIMATION_RENDER_COVERAGE.md)。矿场可用建筑主体、全部 21+8 附属槽及独立 AnimClass 通路；本轮补测所有 29 槽和透明输出，但没有真实倒矿录制，不能把工厂门测试代称矿场全流程已验收。核弹／天气／超时空等若发布为 SHP AnimClass 或普通弹体，会进入通用通路；这不包含其所有屏幕效果。

确认尚未完整处理独立 Particle／ParticleSystem、Wave、RadBeam、VoxelAnim、HVA 多帧、扭曲／冲击波／独立 AlphaShape。主视图只对既有 LaserDraw、Gather、单位 UI 做原生素材孔；未采集的专用效果可能被升级覆盖层遮住。副相机没有主视图下方的原生游戏表面，不能直接套用此类保留方式。

为分类读取固定 [YRpp d9795546](https://github.com/Phobos-developers/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2) 的 BuildingClass、AnimTypeClass、ParticleClass／Type、ParticleSystem、VoxelAnim／Type、RadBeam、WaveClass、DiskLaserClass。额外只作静态核对：Particle 构造 `0x62B5E0` 写 vtable `0x7EF954`，Type `+0xAC`，粒子数组 `0xA83DC8`；VoxelAnim 构造 `0x7493B0` 写 vtable `0x7F6318`，Type `+0x104`，独立数组 `0x887388`。原 Particle 绘制 `0x62CEC0` 和选帧 `0x62D830` 表明部分粒子走 SHP、部分走专门像素路径。这些是分类证据，本轮没有把未验证的粒子／碎片渲染加入产品。

被否定的假设：工具箱解算已自动离开 UI；所有 VXL 缓存命中不消耗面数预算；普通 SHP 爆炸支持等于粒子／所有超级武器支持；普通命令线天然应该滚动；把解码数简单增大就能解决所有缺动画。仍需真实倒矿、超级武器组合、MOD 和显卡实机逐项验收。

## 验证入口

- `planner_tasks.cpp`：240 帧有状态规划及回执与同步基准一致；阻塞工作线程时 UI 提交／取结果／换桥接仍可用，旧 generation 不能发布。
- `latest_worker.cpp`：单执行／单等待、失效、锁外完成通知及退出后不可变结果所有权。
- `watch_host.cpp`：300 个廉价 VXL 同轮刷新；300 个持续变姿态且每项耗满预算时仍全部轮到；旧色盘／几何缓存回归。
- `native_feedback.cpp`、`native_feedback_scene.cpp`、`native_order_feedback.cpp`、`native_unit_ui_stencil.cpp`：600 人／600 条电弧、原 UI、裁剪、失效和相机约束。
- `cooperative_snapshot.cpp`：1000 候选排序键读取；`robust_cooperative.cpp` 的 10000 个分配例与 512×512 边界；矿坑／维修坞和飞机伤害读取回归。
- `animation_blend_native.py`＋`animation_blend.cpp`：两份 EXE 11552 组选档及产品透明 alpha；`game_assets_host_test.py` 验证实际 Context 的 SHP 像素透明输出，`building_animations.cpp` 覆盖全部 29 槽。
- 本地完整渲染／前端工作流用例、受影响用例重跑、x86 与 x64 成对编译、632 项跨位数 ABI 和实际 PE 反例门禁。具体结果、编译输入哈希和范围写入编译包 Build-info.json。
- 最终 DLL 的磁电路由、Guard／关闭模拟执行；原生事件构造和出站列表执行，不模拟真实传输、落体物理或双机同步。
