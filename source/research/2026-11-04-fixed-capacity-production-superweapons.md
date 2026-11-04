# 固定容量扩容、科技全开生产预定与禁用超武

## 范围与实现

基线：`70d9ba37b7340079435e6b48fa52a303c47f449f`。本轮直接提高固定容量，没有引入自适应容量。8 GiB 是用户给出的参考，不是所有缓存的总额限制；缓存仍按实际使用分配。

硬件断点（每线程 4 槽及各模式成本）、科技／类型目录、统计快照、跟踪窗口（3 个）保持原值。共享协议升级为 Assist 94，Shared 为 23,141,808 字节；Host 与 Runtime 必须配套。

| 工具容量 | 旧值 | 新值 |
| --- | ---: | ---: |
| Host 游戏目标／订阅 | 8 / 32 | 32 / 256 |
| 物理指针输入 FIFO／命令暂存 | 16 / 12 | 128 / 128 |
| 入口嵌套／软件 hook 注册表 | 32 / 128 | 128 / 512 |
| 建筑预定／单位预定 | 5 / 30 | 32 / 128 |
| 编队／协同／MO 单批参与者 | 256 / 512 / 256 | 1024 / 1024 / 1024 |
| 自由轨迹采样／巡逻路线点／预览线 | 64 / 16 / 128 | 256 / 32 / 1024 |
| 自动装载乘员／载具 | 512 / 128 | 1024 / 256 |
| 修理候选／待观察修理请求 | 2048 / 4096 | 8192 / 16384 |
| 原生命令跟踪／每帧与跨帧所有权 | 2048 / 2048 | 8192 / 8192 |
| 卸矿任务／手动修订身份 | 256 / 8192 | 1024 / 32768 |
| Planner 对象／意图／矿厂／战斗探针 | 8192 / 2048 / 128 / 8192 | 16384 / 8192 / 512 / 32768 |
| Planner 单批结果／候选等待格 | 24 / 40 | 32 / 128 |
| 导弹防御单批／跟踪任务 | 64 / 256 | 256 / 1024 |
| 连射单批／跟踪任务 | 64 / 256 | 256 / 1024 |
| 躲避单批与威胁／跟踪任务 | 32 / 128 | 128 / 512 |
| 卸载单批／候选目标 | 64 / 8192 | 256 / 16384 |
| 每帧工具事件预算 | 24 | 64 |
| 控制台消息／文本／外发字节／待投递条数 | 64 KiB / 16 KiB / 2 MiB / 128 | 256 KiB / 64 KiB / 8 MiB / 1024 |

64 位渲染缓存：共用 native page 池约 3→6 GiB；图像 768→1536 MiB；地形 256→1024 MiB；几何、packet、raster 与 area-prefix 各提高到 512 MiB；pose、live-face 与 lit-pixel 项提高 4 倍。字体、光标、原始文件等缓存同步增加。32 位兼容预算保留原来的地址空间约束。

渲染输出像素预算 16→32 Mi 像素；采样选择器单边上限 16384，实际 raster/GPU 路径仍有各自 8192 等维度边界，不承诺任意 16K 输出。GPU material／triangle／xBRZ 原图预算 96→120 MiB；共用 RGBA atlas 维持 128 MiB 的设备基线。分配 storage buffer 前检查设备真实 `maxStorageBufferRange`，超出走现有失败／回退路径，不把软件扩容当作设备能力提升。

原生 ABI 边界维持：多人 OutList 128 项及工具保留余量、单机 DoList 16384 项、侧栏每 tab 75 项、单位工厂 30 项。更长的单位预定留在 Host FIFO，待原生占用／ACK 影子预算释放后逐帧提交。预定面板继续显示末尾 5 项并显示总数，避免 32／128 项撑开窗口；原生建筑图标重复项较多时用数量标记。

## 生产预定与科技全开

问题：科技全开已让侧栏出现可生产类型，但 `construction_catalog.h::possible` 的原始科技等级、国家、偷科技及前置条件预测会取消无来源预定；`ProduceReserved` 与 `ProduceDeferred` 又以 `CanBuild(type,false,true)` 拒绝相同类型。

修复：Host 预测按每个类型的已请求科技全开白名单判断，安装期间保持预定。Runtime 的 `reservationCanBuild` 复用旧模式的 `techGate`，并为新硬件参数模式核实已安装槽位、当前世界、玩家与已捕获的类型身份白名单；有许可时调用原生 `House::CanBuild(type,true,true)` 的 build-limit-only 路径，无许可仍走原始检查。实际图标、类型身份、建筑放置、工厂、建造数量及事件入队约束继续生效；两个预定分支均检查真实工厂。没有新增断点、规则写入或游戏指令写入。

证据：`construction_host_test.py` 执行实际 Host 服务（含原条件失效与撤销许可、128 项逐批排空）；`construction_batch_runtime_test.py` 编译实际两个 Runtime 分支和许可 helper；`tech_ui_parameter_runtime.py` 增加编译 DLL 中两种预定提交的许可／数量／工厂用例。后者明确建模 CanBuild 结果，不能当作真实游戏生产完成的证明。

## 游戏禁用超级武器的原生行为

问题：关闭超武后，即使建成超武建筑也无图标／倒计时，是否还能强制使用？

支持的私有 executable SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。布局交叉参考 YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 SuperClass、SessionClass、GameModeOptionsClass；结论以该私有 PE 的原始指令控制执行为准。

| 入口／字段 | 已验证含义 |
| --- | --- |
| `0xA8B263` | GameModeOptions 的 SWAllowed 字节 |
| Super.Type `+0xE7` | Disableable |
| `0x50B298` | 新建筑授予分支；Disableable 且 SWAllowed=0 时跳过普通 Grant／侧栏授予 |
| `0x50B078` | 已存在的建筑来源检查，禁用超武时清除授予来源 |
| `0x50B169` → `0x6CB7B0` | 普通实例可能被维护逻辑 Lose 回收 |
| `0x50AF47` | 已 Present 的 OneTime 实例走不同维护路径 |
| `0x6CB560` | `Super::Grant(bool oneTime,bool announce,bool onHold)`，thiscall，RET 12 |
| Super `+0x6D/+0x6E/+0x6F/+0x70` | Present / OneTime / Ready / Suspended |
| Super `+0x30/+0x38`，Type `+0xF4` | 充能 timer 起点／时长、ShowTimer |
| `0x6CC360` | 原生 CanFire 就绪／暂停检查 |

`superweapon_disabled_native.py` 在 Unicorn 中执行原始授予门、来源门、OneTime 维护门、Grant 与 CanFire，共 17 例通过，无替代函数 seam。普通 `Grant(false,false,false)` 本身不检查 SWAllowed，可暂时设 Present 并注册倒计时，但普通维护仍会回收。因此仅补图标或反复普通 Grant 不是完整解决方案。

`Grant(true,false,false)` 在 SWAllowed=0 时成功设置 Present=1、OneTime=1、Ready=1，并注册 ShowTimers，SWAllowed 保持 0。一次性原生授予可行，不需要改游戏代码；它是立即可用的一次性实例，不等价于恢复普通超武的持续充能。现有工具没有为此新增强制授予开关。发射效果、一次发射后的完整生命周期及多人一致性尚未验证。

排除“只有 UI 隐藏”的解释：普通授予与保留路径本身受到禁用选项影响。工具的超武列表又要求 Present，因此缺图标符合未授予／已回收的实际状态。

## 验证边界

固定容量测试覆盖旧边界之外的命令 FIFO、1024 人协同及重试身份、8192 所有权、16384 修理 tombstone 复用、256 轨迹尾部更新；另运行相应现有算法、生产预定、控制台、783 项跨位数 ABI 与 Windows 配对构建。原生超武用例的摘要见 `evidence/2026-11-04-superweapon-disabled-native.json`。CI 与编译 DLL 的最终结果以对应提交构建为准。没有打开真实游戏，没有验证视觉、超武发射效果或联机。
