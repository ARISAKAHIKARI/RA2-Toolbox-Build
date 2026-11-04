# 普通 CnCNet Spawner 与 1.0.39-pre.6 的兼容性复核

日期：2026-10-28（UTC+8）。问题：普通版 Spawner 是否与当前发布版发生覆盖、启动拒绝、显示／浮点或网络语义冲突。

## 结论

**不能给整包“完全无冲突”的结论。已找到一处会正确退让的共享补丁入口，并在当前发布 DLL 中重新确认了跨玩家保留所属方模式的条件性错令缺陷。** 新颜色／xBRZ 外部重绘与标准己方命令暂未发现普通版特有的直接冲突；这不是实机或双机联机认证。

| 分项 | 本次证据 | 结论 |
| --- | --- | --- |
| 写入地址 | Toolbox 39 个许可写入点，对照普通版 196 个 Hook、49 个静态补丁及公开源码 75 条动态写入 | 仅 `0x6D8640/5` 相机入口共享；其余登记／公开动态范围无直接交集 |
| 启动与调用校验 | 155 条声明、151 个不同地址范围，含 21 项启动签名、函数哈希、上下文及已知虚表槽 | 与上述登记／动态写入无交集；未穷举动态生成对象的所有虚函数 |
| 小地图相机 | 实际普通 DLL 构造器、重定位后的相机函数与发布版 Runtime | 两种引擎样本共 4 例、144 次原生相机调用；共享入口零覆盖，停止后仍保留普通版补丁 |
| 浮点环境 | 两个实际普通版修复导出与发布版回调，在两种引擎样本中组合执行 | 48 例通过；不覆盖任意嵌套调用、显卡驱动或实机时序 |
| 标准己方网络指令 | 发布 DLL 的 117 项已有受控发送／仲裁回归，加 8 项实际普通版长度 Hook 往返测试 | 未发现事件格式／时序事件处理冲突；没有真实网络和完整任务执行 |
| 跨玩家保留事件所属方 | 实际发布 DLL 的五个发送适配 thunk，两个发送者和第三接收端 | protocol 1／2 各有一组错单位引用复现，另有正常对照；该实验功能默认关闭 |
| 战网引擎样本的网络补充 | 该离线样本的发送续行已改为跳到空白填充 | 两套新增网络测试记为 **BLOCKED**，没有替换指令以强行通过 |

## 样本、先前资料与范围

调查前检索了 `INDEX.md`、已有普通版／网络／Hook 笔记，以及同一份 `RA2-Reverse-Engineering-Notes.sqlite` 的 Library 版本 76。旧记录主要针对较早 pre.6 代码，本次重新核对刚发布的实际 Windows CI 二进制。

| 对象 | 固定身份 |
| --- | --- |
| Toolbox 产品源码 | `b372ace50101e0099a5e9533c50470b4cabb1536` |
| 发布标签源码提交 | `v1.0.39-pre.6`，`5508c80f7b81abad4fc51847eb7f3530acddc9af`；只增加发布文件，产品源码与上行一致 |
| 实际发布 GameRuntime.dll | SHA-256 `eba0ce9f13376aa193942a4765b07fc28e2f63d7a577bed9450325d8e951f4f7` |
| 实际发布 RA2Toolbox.exe | SHA-256 `9ef2d38d185886af70d2d22372f4b976e3cccc26d7e87e76a1f00845c3a835a5` |
| 官方普通 CnCNetYR v17 DLL | `CnCNet-Spawner-CnCNetYR-v0.0.0.17`，233472 字节，SHA-256 `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d` |
| 普通版官方 ZIP | SHA-256 `d0d329c8d4db03f297a44e50c726df1ec97e086ae20079a08ccf0e15351fb9ba`，与官方 release asset digest 一致 |
| 普通版公开源码 | `CnCNet/yrpp-spawner`，`fbe8d7d2620584d04ba14fa3580943bfa1448d40`；69 个公开源码／配置文件逐一核对 Git blob SHA |
| 授权引擎样本 3 | SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| 授权引擎样本 4 | SHA-256 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

普通样本明确是 **CnCNetYR、非 Hardened** 构建；不是另一个不带 CnCNetYR 后缀的通用模组构建，也不等同官方客户端包中的同名加固 DLL。用户当前对局实际加载的路径、哈希及配置尚未取得。本轮没有访问 Private 子模块，不发布任何用户引擎文件、内存捕获或官方 DLL。

## 1. 完整写入面与校验范围

从生产 `experimentalpatch::size()` 和直接补丁策略编译提取 39 个许可地址，共 218 字节。这个集合保守包含未启用和条件性功能，不能说 39 个站点默认全部安装。当前游戏代码写入统一经过 `shared/source/game_entry_manager.cpp`；Windows 消息 Hook 与 Syringe 的游戏指令 Hook 分开记录。

实际普通 DLL `.syhks00` 有 196 项、`.patch` 有 49 项。此前公开源扫描的 223 个文本宏并不是该编译 DLL 的 Hook 数量。另从完整固定公开源码解析 75 条显式 `Patch::Apply_*` 调用，写入长度累计 473 字节，包含配置及 RA2 分支，不代表同时生效或去重后的字节范围；零未解析项。75 条公开动态写入与 39 个许可入口没有交集。

其中两个 Blowfish 分配 Hook 的登记长度为 0，不能当作没有写入：`0x4381DA` 的公开回调返回 `0x4381F3`，`0x43830A` 返回 `0x438323`。工具保留原始 `declared_size=0`，另用整个 25 字节跳过区间作保守覆盖检查，并明确它不是已经测得的 loader 实际覆盖长度。写入点及 guard 的所有比较都使用该覆盖字段；补入后仍只有相机一处交集。

额外校验覆盖：启动签名 21 条、`patchcontext` 73 条、直接补丁尾部上下文 7 条、发送链上下文 8 条、手工枚举的函数／签名范围 30 条、已知虚表槽 16 条，共 155 条／151 个不同范围。静态登记项和 75 条动态调用均没有命中这些范围。CameraClamp 的函数体校验从入口后五字节开始，因而不与入口共享结论矛盾。

边界：没有把动态 CommandClass 对象的所有 GetName 地址、运行时生成的其他插件 trampoline 或未知 DLL 全部纳入。地址不相交只能排除这些已枚举范围的直接覆盖，不能证明模拟语义一致。

## 2. 相机共享入口：有同址，但已有保护确实生效

普通版 `src/Misc/Bugfixes.ClampTacticalPos.cpp` 与 Toolbox `assist/source/small_map_camera_bridge.h` 都接管游戏 `0x6D8640` 的五字节入口。`bridge.cpp` 独立调用 `smallMapCameraTick()`，因此 `ViewZoom=false` **不会**关闭这项小地图修复。

普通补丁的数据在 BSS 中动态生成，仅查看文件内 `.patch` 数据不足以得到跳转。已在 Unicorn 中执行普通 DLL 的构造器 RVA `0x1080`，确认它把数据写入 RVA `0x3B1D4`，目标为相机替换函数 RVA `0x2A50`。分别在模块基址 `0x10000000` 和 `0x18000000` 验证了重定位后结果。替换函数使用寄存器快速调用约定，位置指针在栈上，返回布尔值位于 AL，`ret 4`。

再将实际普通 DLL 映射到 `0x18000000`，安装该真实跳转，与发布 Runtime 同时运行。两种引擎、初始大／小地图共四例，每例 36 次真实相机函数调用。初始→小→大→小→停止的过程中：入口写入次数为零；普通边界计算及 ABI 保持；Runtime 保持连接；小地图尝试会记录一项 conflict，但没有安装自己的相机补丁。停止后普通函数仍正常。

所以这里的 conflict 计数可以表示**已安全跳过外部拥有的入口**，不能单凭计数判为崩溃。测试覆盖普通补丁已存在、工具箱随后尝试安装的顺序；没有认证反向晚加载、完整 Syringe 启动或实际图形设备。

## 3. 标准网络路径与一个已确认的条件性缺陷

固定 v17 的 `ResponseTime2` 是事件 `0x30`，新增载荷两字节，`EventClass` 仍为 111 字节。另一份旧源码快照使用 `0x31`，不能混用于本样本。`spawn.ini` 的 `Protocol=0` 选择自适应时序，但 Spawner 初始化把原生通信协议设置为 2；不会因此被 Toolbox 的原生协议准入拒绝。IPC80 是工具箱进程间协议，不是新增游戏网络格式。

标准己方飞机分配仍使用原生构造器、OutList 和原生组包。普通版 NetHack 的直连／隧道处理在原生组包之后；没有发现标准路径另造一种 UDP 格式。

实际发布 DLL 在样本 3 上：网络适配 29 例、N/O 关闭的 wire 对照 6 例、MegaMission 仲裁及 ABI／FP 82 例，合计 117 例通过。另映射实际普通 DLL 的三个事件长度导出，以建模 Syringe `REGISTERS` 桥接执行：LAN／Internet、适配关闭／开启、纯时序或混合 MegaMission／时序、环形队列跨界共八例，事件顺序、House 和时序载荷均保留。这不是完整 ProtocolZero 时序循环或 socket 测试。

**当前发布版 NetworkHousePreserve 的条件性问题仍存在。** 实际五个发送适配 thunk 在两个发送者共享同一事件 House、第三接收端保留按 House 索引的压缩字典时，会串用来源单位组。protocol 1／2 的受控案例中，原发送者预期 `[201,202]`，被另一保留外方 House 的发送者插入后，第三端解码为 `[101,102]`；没有插入的对照正确。四个断言通过表示“缺陷及对照均按预期复现”，不是该功能兼容。

该问题不依赖 Hardened，也不要求普通 Spawner 额外拒收。只有实际保留外方身份时成立；`NetworkHousePreserve`、`ControlCommandOwner` 默认关闭。不能拿它解释这两个选项关闭时的故障，也没有复现用户某场实机的完整任务效果或 CRC/OOS。`ControlAll` 共用判定改写、完整原生联机飞机续点仍有独立模拟一致性风险，本轮没有把它们认定为用户掉线根因。

### 战网离线样本的网络验证阻塞

样本 4 的发送续行 `0x64B9F7` 已经是跳转，目标 `0xB7E637` 位于 `.p_text` 尾部填充。磁盘及映射中的目标 32 字节均为零；直接计算与 pefile 的文件偏移同为 `0x482637`，不是映射器漏掉了非零代码。该节虚拟范围到 `0xB7E3CC`，原始数据覆盖文件 `0x47E000..0x483000`。

因此上述两套新增网络测试在样本 4 上记为 **BLOCKED**、无执行案例，并未把跳转恢复为其他样本的原生指令来通过测试。不能从这个离线文件现象断言实机也跳向空白；可能需要真实载入态才能继续。该样本的小地图和浮点路径另已完成，不代表其网络路径也完成。

## 4. 新重绘、旧地图模式、浮点和配置不能混为一谈

新 `render_upgrade_host.h` 工作线程只持有游戏查询／读取权限，在外部进程解码、计算色盘／光照并绘图；不提交 Runtime 请求，不调用原生 Draw，不写游戏对象或 RNG。xBRZ 与颜色缓冲的修改均在工具箱自身地址空间。这个结论只覆盖新重绘链，不覆盖整套工具箱。

旧地图策略仍在界面和 Runtime 中存在：1／2 为半透明／跳过遮罩绘制；3 自动调用原生 `RevealMap @ 0x577D90`；4 使用直接可见性／雷达补丁。策略 3 会改变包含在原版同步摘要中的 `House+0x241 MapIsClear` 和探索状态；策略 4 也不能套用只读重绘结论。`SceneRefresh=false`／`ViewZoom=false` 并未禁用这些地图模式。

普通版已经有飞机阴影／地块重算、Invisible DrawBehind 和浮点状态修复。本次执行其实际 `SetVideoMode_RepairFPState` RVA `0x35B0`、`LogicClass_Update_RepairFPState` RVA `0x3600`，与发布 Runtime 回调以两种串行顺序组合。两种引擎 × 两导出 × 六种 MXCSR × 两顺序共 48 例，普通修复、Runtime 内部归一化和返回前完整状态恢复均符合预期。普通日志计数器在夹具中置于其正常上限，未改修复函数代码。任意嵌套修复调用、显卡驱动和真实 Hook 调度仍未覆盖。

Windows 消息 Hook 与 Spawner 游戏指令 Hook 是不同层；当前 Runtime 不替换游戏 WNDPROC／IAT，也不吞 WM_CLOSE。光标隐藏仍调用原生鼠标引用计数接口，并非全部外部绘图；普通 DoBlit 检查的是捕获状态，未发现同址补丁。对应实机硬件光标和窗口时序没有测试。

配置还有条件性相互作用：普通版 `SingleProcAffinity=true` 默认可把游戏进程绑定 CPU0，外部重绘工作进程不随该调用直接改变；`DDrawTargetFPS` 默认 `-1` 不写上限，非默认值则可写 ddraw 的运行期 TargetFPS 导出，影响 INI 加载后的实际目标值。这些可能影响流畅度，未作为已观察到的不同步／超时原因。

## 复现、失败记录与未决问题

可移植脚本见 [审计工具](../tools/cncnet_ordinary_audit/README.md)，精简证据位于 [ordinary-spawner-v17](evidence/ordinary-spawner-v17/)。脚本要求显式提供授权样本，不附 DLL、游戏文件或内存捕获。固定上游 Git blob 清单随工具保存。结果区分静态范围、实际指令受控执行、条件性缺陷和阻塞。

另外运行的发布版平台夹具 13 例及当前 Host 输入检查通过；其中未映射完整 Spawner。当前 `validate_render_upgrade.py` 通过。一个历史 `no_interception_source_contract.py` 因全局禁止当前已存在的 Manager `VirtualProtect` 写入、并仍要求旧 `PatchDenied` 行为而失败；这是过时契约，本轮未修改它，也不把它算作运行时兼容性证据。

被排除或修正的假说：只查旧 13 个站点足以涵盖产品；关闭 ViewZoom 就没有相机同址；所有“普通”同名 DLL 都等于该样本；样本 4 的失败是未绑定 CRT 导入；地址不相交即可证明联机兼容；关闭保留所属方时也可用该缓存缺陷解释故障。

仍缺：用户实际载入模块／配置、完整初始化及其他插件时序、真实 socket 与 ProtocolZero 循环、两端完整任务和同步摘要、实机画面／帧率／硬件光标。**本轮没有修改已发布产品行为，没有绕过平台验证，没有声称用户掉线根因已定位。**

官方固定来源：[v17 release](https://github.com/CnCNet/yrpp-spawner/releases/tag/v0.0.0.17)、[相机修复](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.ClampTacticalPos.cpp)、[不同步修复](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.Desyncs.cpp)、[事件扩展](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Ext/Event/Body.cpp)、[网络适配](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Spawner/NetHack.cpp)。
