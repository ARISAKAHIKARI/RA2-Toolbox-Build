# 首次同步门槛之后的直接写入与清理遗漏复核

## 问题、版本与证据边界

用户要求在合入 HF33 后，继续找不属于安装时机、但会直接制造不同步的旧做法。
源基线为合并提交 `fefd9e017060c7955811a9518c9bf98f7b144ffe`；其中第二父提交为用户指定的 `17ee48810d33d47203d52893d3b0472d992f3a11`。

已检索 research/INDEX、普通版不同步/兼容性、控制回归、Hook 冲突、HF31/HF32 与首次同步门槛笔记。Library 原持久 SQLite 本轮仍不可通过现有接口取得，没有另建替代数据库。
沿用已有原生研究样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的地址约定；本轮未取得/执行该私人样本，没有游戏、真实 socket 或双机执行。

本轮证据是生产源代码、生产模板在 C++17 static_assert 中的实际求值、Windows 目标 COFF 编译。下面的缺陷是可达的源码错误路径，不是已经取到用户故障局首个分歧的实测因果链。

## 新确认并修正的三类遗漏

### 1. 科技加强直接改规则，且失败的写入/还原会丢失所有权

位置：`assist/source/tech_enhancement.h`、`tech_enhancement_bridge.h` 的 apply/restore/restoreTechEnhancement/techEnhancementTick。
21 种 Type 的 `+634` 是完整 32-bit TechLevel。此路径没有 EventClass 广播，旧代码也没有联机模式门槛。即使晚于第一次远端同步，单端修改规则仍不能由时间门槛变成同步操作。

旧路径有三个具体问题：

* enhancementWrite 在 put 成功后还会做 ReadProcessMemory 验证；验证失败会回报 false。apply 却只在 writer 返回 true 后设置 owned，因而可能已经把 -1 改为 1，记录仍显示未拥有。
* restoreTechEnhancement 不论 restore 成败都 `r={}`，随后清掉 epoch/house/original。关闭后一次失败便永久丢失补还原所需的信息。
* identity 把读不到与确实替换混在同一个 false，restore 会丢掉 owned。重试还会把“前一次已写回原值但验证失败”误判为第三方冲突。

修正：只在模式 0/5 应用科技加强；模式 3/4 及未知模式不写规则，状态 2 明确显示“仅支持单机，联机未修改”。writer 调用前登记原值/所有权；读不到时保留；只有已确认的对象/目录替换才放弃旧对象；原值已经恢复时幂等完成。失败的记录、世界身份和剩余计数会保留重试，pending restoration 完成前不会重绑新应用。关闭开关也不能掩盖未还原错误。

没有尝试将 TechLevel 包装成非标准网络事件，也没有放宽 MOD 类型签名。换局/回退旧世界仍不写旧指针；不能保证旧版本已经遗弃的类型修改能由新 DLL 追回。

### 2. CRC 镜像清理把暂时读失败误当成世界已经退休

位置：`assist/source/crc_shadow_bridge.h::restoreCrcMapShadow`。
旧逻辑先做 `if(crcMapWorld() && frame>=last)`；世界任意一次 read 失败就跳过全部字段还原，继续卸掉八个站点并 clear model。逐 House 的 crcMapIdentity 失败也直接 continue。

条件性后果：如果同一活世界中工具已经把 House `+241 MapIsClear` 从 0 改成 1，且上述清理遇到暂时读失败，旧流程可留下 1、移除原值镜像并宣称完成。此前由镜像保留的原生摘要输入会改变；这条路径不需要两个 Hook 地址直接重叠。

修正：新增 `crc_shadow_restore.h`，区分 Current/Retired/Unavailable；先完成已拥有的字节还原，才允许卸 Hook/clear。读不到、写失败或未知外部值均保留原值与所有权，清理状态为 -2，等待后续普通 game-thread 入口重试；已成功写回但验证失败时可幂等完成。确认换 epoch、House ID/成员变化或回退时不写旧世界；自然同值写入仍保留。

**范围只有既有 MapIsClear 字节的清理，不增加 CRC 替换范围，不改原生接收/比较，也不重算 RNG。** unknownWrite 后不可信的记录仍返回真实原生值，不用旧镜像掩盖未知状态。本修正不还原 House+240、Cells、Visionary 或原生 Reveal 后继；HF32 的裁剪夹具依然不能证明完整 Reveal 联机安全。

### 3. stop=2 及 IPC 脱离没有等待所有功能恢复

位置：`assist/source/bridge.cpp::tick/disconnect/reset`。
旧 stop 分支只等待 CRC/sender 这类少数组；reset 中的其他恢复仍可失败，之后却写 stop=2、解除 Shared 映射。直接字节补丁没有一个可用的“disabled callback”来补救；若还原失败，游戏可继续带着补丁，而宿主已认为它清理完了。

修正：reset 尝试每一组恢复，不让第一组延期阻止其他组先退让；随后检查中央 registry 的剩余活动补丁、CRC/sender 安装状态、材质 controller 与科技还原记录。仅全部退休后才写 stop=2/unmap、允许新 controller 接手。
Runtime 独立的 `55DE81` 只读帧通知有自己的通道/清理；计数中特别排除它，避免 Assist 等待一个由自己断开后才能退役的通知而互锁。第三方冲突不会被强行覆盖；无法恢复时保留旧连接，持续等待可能需要重启游戏。

## 整条写入面的复核结果

| 路径 | 观察到的写入/原生调用 | 本轮判断 |
| --- | --- | --- |
| 普通自动命令、装载/卸载、修理、移动、攻击 | 统一构造器/高层原生 append；模式 0/5 DoList，3/4 OutList；计时后重查身份 | 已检查桥接层，未找到另一个普通联机功能直接调用 ForceMission 或自行消费事件的入口；不等于完整原生任务执行已验证 |
| Sticky | 本地 ForceMission/NextMission 仅模式 0/5；联机通过 mission 事件，维护只观察 | 当前代码有明确模式隔离；没有据此认证整个功能的实时网络表现 |
| 完整原生飞机续点 | arrival 回调只对本地 House 调 6385C0 TryNextPlanningTokenNode | **已知高风险仍在**：本地规划推进/DoList 与其他端并不自动等价；nativeAirNetwork 完整实验不是稳定性修复 |
| ControlAll / lowDamageNetwork | 共用控制判定；联机低伤害可改变所有 House 的原生索敌阈值 | **已知高风险仍在**：持续影响本地模拟判断，不属于仅 UI；延后安装不能保证它们不分叉 |
| NetworkHousePreserve | 原生命令 House 及原生发送适配 | HF33 是宿主连接生命周期修复；同 House 多来源排序及接收压缩字典碰撞的既有缺陷没有被这次合并修好 |
| 原生 Reveal / 直接全图 | 577D90、House+240/+241、Cells；直接模式可进入真实 Reveal | **明确状态写入**；CRC 单字段恢复与首个远端同步门槛均不能证明 Cell/Visionary/RNG 后继一致 |
| Sidebar、选中、音效、提示框、相机、缩放 | 本地 cameo/标志、原生 UI/显示函数；tooltip scratch object 调 TimeToBuild | 没有在桥接层发现这些功能直接写 Mission/RNG；不能把所有被调用的函数体和 MOD 覆盖当作无副作用认证 |
| 自动规划的原生查询 | GuardedEnterAction、SelectWeapon/GetWeapon/IsCloseEnough、CanEnterCell 等虚函数 | **证据缺口**：SEH 只处理异常，不回滚可能的状态/RNG 副作用。源码写 read-only 不能代替具体版本的写入足迹；本轮未执行完整函数体，未据猜测改它们的结果 |
| 外部读取与新渲染 | ReadSession 查询/读取权限；外部素材/像素/规划值运算 | 对已检查的生产读取/重绘调用链未发现游戏写入/远端执行 API；结论限于这些路径，不外推至整个工具箱 |
| 已禁用的 SwitchPlayerContext | 旧路径写 House+1EC 并替换 CurrentPlayer | **休眠危险路径**：固定 YRpp 中 +1EC 是 IsHumanPlayer、+1ED 是 IsInPlayerControl，不能称为仅改“当前玩家”。SwitchAvailable=false，使其不是当前可激活路径；未重新开放 |
| 浮点边界 | RuntimeFPScope 保存/恢复 FXSAVE，x87 control 保留，内部 MXCSR 归一化，链入其他 Hook 前退出 | 没有新增全局浮点修改。嵌套显示驱动/平台修复、MOD SSE 与真实线程时序仍需实测；旧的有限组合测试不能覆盖它们 |

因此“所有功能都因为 Hook 冲突掉线”没有证据。安装冲突/失败清理、直接规则写入、真实 Reveal、模拟判定改写和网络事件排序是独立机制，应按第一处分歧区分。

## 实际验证与尚未验证

* `tech_enhancement_recovery.cpp`：24 个 static_assert，执行生产模板，涵盖失败前/失败后写入、幂等恢复、原值/所有权保留、暂时读失败、退休/复用对象、第三方值及单机准入。
* `crc_shadow_recovery.cpp`：16 个 static_assert，执行生产 Model/restoreData，涵盖世界/House/字段读失败、写失败与已写未验证、自然写入、未知值、epoch/复用/回退。
* 合并版本原有 13 个首次同步策略 static_assert 已通过；总计 53 个编译期场景。它们没有执行游戏。
* 修改后的 x86 Assist bridge 和 x64 Host 使用 Windows 目标编译为 COFF 物件；共享 IPC 定义未改变，之前实际从 x86/x64 COFF 比对的 685 项 ABI 布局仍适用。
* 合并提交的 CI run 41221881846 仍为 runner_id=0、steps=[]，没有进入测试；原因未确立。本机普通链接受沙箱缓存/临时文件限制，未产出可宣称通过的完整 exe/DLL。

完整生产链接/PE、Windows HHOOK 执行、真实原生查询写入足迹、完整 Reveal、双端事件顺序及第一份成对 SYNC 差分仍未完成。
当前修改不能被描述为“全部不同步已修好”。新代码须由新游戏进程加载；只重启工具箱不能替换已 pin 的旧 Runtime 或回滚旧对局的探索/规则历史。

证据见 [本轮检查摘要](evidence/latent-desync-mutation-audit.json)。Hook 的 51 个允许站点、14 个旧禁止站点与 Windows hooks 分组仍见 [完整清单](2026-10-31-peer-sync-hook-activation-audit.md)。8 是并行游戏进程连接槽，32 是功能订阅槽，均不是可用 Hook 数。

公开布局核对：[固定 YRpp HouseClass](https://github.com/Phobos-developers/YRpp/blob/d979554/HouseClass.h)。原生网络/控制/Reveal 依据仍归属此前已链接的笔记，不冒认本轮重新执行过私人样本。
