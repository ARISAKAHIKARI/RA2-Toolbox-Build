# v1.0.37-pre.1：防空预备火力、磁电地面障碍与外部 VXL

调查前检索了 INDEX、1.0.36 笔记及持久 SQLite。这里只记录本轮新增发现。

## 样本和证据边界

- 兰博玩样本：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- 战网样本：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`
- 新反汇编最初针对战网样本；`verification/review-137/native-layout-comparison.json` 核对列出的八段函数在两份样本中逐字节一致。
- `assist/tests/{missile137_runtime,magnetron137_runtime,formation_runtime}.py` 执行真实 DLL 和原始事件构造器，Win32、世界、武器查询和移动完成状态是受控桩。不能据此声称完成实机寻路、弹体物理、Windows 叠加画面或双机同步验证。

## 普通待机与火控预留

问题：分配目标后，尚未进入原生射程的目标不能发射，单位仍自动抢打最前目标。

YRpp 的 Mission::Sleep=0、Attack=1、Guard=5、Sticky=6。原版 rules 的 Sleep 配置为 Zombie=yes、Retaliate=no、Scatter=no、Recruitable=no。普通 Guard 并非不主动攻击。实现用原版 MegaMission 的 Sleep 为即将进入射程的目标留枪，关闭、目标消失和正常退出时用 Guard 释放。禁止把 Sleep 写进单位字段，也不调用本地任务执行器。

预判只决定预留；Attack 仍必须通过原版 SelectWeapon / GetWeapon / IsCloseEnough / IsInAir 查询。短范围带为原武器射程＋AA bonus＋1024 leptons，最远预测 30 帧。静止或远离目标在获得运动样本后不继续视作接近。平均分配按数量，智能分配按 HP 与单发护甲修正伤害；本轮没有弹体飞行中的伤害账本，不声称绝对消除所有过量射击。原生命令有发送/执行延迟，也不能追回已经发出的炮弹。

受控结果覆盖两种计算位置、两种本地/网络队列、单目标自动锁定重新分配、Sleep/Attack/Guard 原始报文、停用、正常退出及手动攻击优先。手动 token 改变后，到该命令回到空闲前不自动重新接管。

## 磁电下方的树、矿柱与地面载具

0x71BB90 的 TerrainClass 构造器写入 vtable `0x7F522C`，Type +0xC8，IsBurning +0xCC，IsCrumbling +0xCD。树与矿柱属于 Terrain，不能只查 TechnoClass 全局数组。按原版落地使用的 Cell +0xE4 / Object +0x30 链查询，限制链长度并拒绝环。

原版 `ObjectClass::IsInAir` 0x5F6B90（thiscall，无参）使用 Alive +0x74、GetHeight 虚函数 +0x1C8；阈值为 `2 * DWORD[0xAC13C8]`。**低于阈值的悬浮对象可能返回 false**，所以不能只凭此接口判定“地面坦克”。地面载具还要无 transporter、磁电联系、被操纵标志和空中层，X/Y 在同格，并且 Z 恰等于 `Map::GetCellFloorHeight` 0x578080（thiscall, map=0x87F7E8, CoordStruct*）。桥面继续保守排除。

延续 1.0.36 的结论：0x54CC91 遍历落地格对象；Terrain 分支破坏地形物后继续，其他对象分支 0x54CD80 附近经 ReceiveDamage +0x16C 处理碰撞。树/矿柱存在是释放机会，不能宣称必定摔死被吸单位。受控测试覆盖树、矿柱、普通坦克、飞机、1 lepton 低空悬浮、磁电举起、空中层、邻格对象、正在倒塌地形及桥面。

全局无限吸继续走已有磁电规划器，仅为空闲己方磁电创建固定驻守意图。单独开启全局模式也必须触发世界索引；测试发现遗漏这一调度条件会使首次全局启用无单位，已补上。手动移动/攻击期间不纳入，关闭撤销全局续接意图。

## 外部 VXL/HVA 读取与自行绘制

问题：外部叠加层如何绘制实际模型，又不调用游戏渲染或改游戏画面。

主要公开布局依据：

- [YRpp ObjectTypeClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ObjectTypeClass.h)
- [YRpp VXL](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/FileFormats/VXL.h)、[HVA](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/FileFormats/HVA.h)
- [YRpp ColorScheme](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ColorScheme.h)
- [OpenRA VXL 格式读取](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/FileFormats/VxlReader.cs)，只用于核对格式；实现独立编写。

ObjectType +0xB0/+0xB8/+0xC0 是车身/炮塔/炮管 VoxelStruct，每项 `{VoxLib*,MotLib*}`。VoxLib +4/+8 为头/尾数量，+0xC 为 body 长度，+0x10/+0x14/+0x18 为 header/tailer/body 指针。Tailer **0xA4 字节**，尺寸位于 +0xA0..A2，法线模式 +0xA3，八个边界点在 +0x40。

关键反证：**内存 Tailer 的前三项不是文件相对偏移。** 0x7563A8–0x7563E7 给文件 span_start/end/data offset 加上 BodyData，再写回 Tailer。外部解析器先验证它们落在 body 范围内，再转换成本地数组偏移。Span 是 skip/count/(color,normal)×count/重复 count；空列为 -1。完整边界、总字节数、体素数、面数和指针一致性均有限制。

MotLib +4 layers，+8 frames，+0xC 为连续 48 字节矩阵。0x5BD5C0 读取，0x5AE5E0 仅拷贝 12 个 float。0x5BD730 只缩放平移项 +0xC/+0x1C/+0x2C；0x5F7C83–0x5F7C9C 在加载后传入 Tailer.HVAMultiplier。所以读取已经载入的 HVA 后**不能再次乘该倍率**。当前预览使用首帧 HVA，而非完整动画或动态炮塔状态。

0x705D70 GetDrawer：Type +0xDF0 的自定义色盘向量，或全局 `0xB054D0`；House +0x16054 是 color scheme 索引。ColorScheme +4 是 256×RGB 字节色盘，+0x30C 是原版 converter。外部直接读取 RGB 与玩家 remap，不调用 GetDrawer。VoxelScaleX/Y 依据 YRpp 为 Type +0x360/+0x368，TurretOffset +0x720（换算尚需实机校准）。

EXE 独立解码、生成可见体素面、软件深度缓冲、简单面光照和预乘 alpha=124，再由已有 UpdateLayeredWindow 叠加。主程序缓存类型/玩家/朝向；模型与图像数量受限，预览采样约 30 Hz。没有调用游戏 DrawVoxel，也没有把圆点或图标伪装成 VXL。非 VXL 单位读取已载入 SHP 首帧；未能读取实际资源时显示模型未载入计数。

未决：真实 MOD 的炮塔偏移、变形/多炮塔替代资源、精确原版法线光照、SHP 朝向动画与实际屏幕比例仍待实机验证。构造体素模型的解析/深度/alpha 测试不是这些视觉细节的证明。

## 队形输入与朝向

复用已有 WH_GETMESSAGE；只扩大送入 AssistRuntimeFilter 的消息种类。输入预览可以比最近快照领先最多 6 帧，不能误用要求同帧的 simulationReady 来处理鼠标消息；真正的 Move 发送仍在同帧原生网关里完成。

没有找到已验证可用的“任意角度转向”网络事件。本轮采用目标格后方一格的接近点，再发最终 Move；两段都走原版事件。最终朝向受格点、地形与原版寻路影响，并不直接写 Facing。预览可精细旋转，最终停靠角度不能保证像素级一致。原生建筑/地形障碍格在预览中避开，剩余可达性与拥挤交给游戏处理。
