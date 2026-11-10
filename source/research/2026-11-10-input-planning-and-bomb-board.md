# 1.0.48-pre.1：原生路径节点、输入锁与绑弹装载

## 问题与范围

用户要求飞机直接提交原生路径点事件并保留操作反馈，修复 MO 手势拖框消失，增加绑弹步兵自动寻找最近载具，以及地图触发锁定操控后继续操作。

先检索 INDEX 和持久 SQLite；复用 2026-10-23-orders-display-136、2026-10-26-network-event-audit、2026-11-02-mo-pause-air-port 和 2026-11-01-control-house-flag-mode 的既有调查。本次没有新的游戏二进制反汇编或实机运行。

## 路径点提交链

既有研究支持的样本 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`（原版）及 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`（此前记录的支持样本）；不是本次重新验证其运行效果。

- MegaMission 构造入口 `0x4C6860`；事件 `+0x1D` 为 append 标记。
- PlanningAdd `0x637DD0`，事件按值传入、`ret 0x70`，设置 append 后调用 `0x6521C0` 入队。不是只修改本地路径图。
- 接收 `0x4C6CB0 -> 0x637E00 -> 0x638120` 追加 token 节点。
- 原生松键 `0x637A10/0x637A30` 经 NoOptions 构造 `0x4C66C0` 发送 `PlanCommit=43`；不会上传一份仅本地存在的路径图。
- `TryNextPlanningTokenNode=0x6385C0` 负责原生节点接续；飞机到达时是否会在所有原版/平台版本调用这条链，仍需实机验收。

实现：现有消息入口只接管纯己方飞机的地面点击；通过统一事件网关排入 append MegaMission。队列繁忙保留剩余节点。暂存真实原生路径点键的 KEYUP，等全部节点入队后原样交回游戏，由游戏发送一次 PlanCommit。跨对局/阵营不重放。自定义工具箱按住键继续使用兼容模式。未增加 CanUseWaypoint 或到达回调补丁。

拒绝“只加本地节点、松键自动广播全部节点”的假设；没有第二个工具箱 PlanCommit。记录、排队、松键提交和中断均有反馈；原生语音仅对应玩家添加节点，不用于自动续段。

## 地图触发输入锁：新的头文件交叉证据

固定主源：YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2`。

- [GeneralDefinitions.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GeneralDefinitions.h)：TriggerAction `LockInput=0x2E`、`UnlockInput=0x2F`。
- [Unsorted.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Unsorted.h)，blob `bf7058a1a54297ca6737fb319aa35831c38de3ce`：`bool UserInputLocked` 位于 `0xA8ED9C`。
- [TActionClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TActionClass.h)，blob `c243d16260d63196f02eec8b27ad5bfaaa737485`：Execute `0x6DD8B0` thiscall；动作通常内联，不能据此声称已反汇编 LockInput 的实际写入。

**证据级别：固定版本公开 SDK 声明 + 受控数据读写测试；没有本次支持 EXE SHA 对该锁字段的独立确认。** 对原版地址布局的推断在 UI 中以明确范围呈现；未知平台/Mod 自定义锁机制未验证。

实现 `Input.KeepUnlocked` 默认关。只在命令上下文就绪、游戏前台、非聊天且字段可读且为 0/1 时，将原值 1 清为 0；只写可写提交数据页的一字节，不改页面保护、代码、House 归属或战败状态。0 不重复写；写后复读。关闭后停止解除，不重新写回旧的 1（否则可能重锁已经被地图解锁的玩家）。有开关提示和解除次数/失败状态。

拒绝用 House `+0x1ED IsInPlayerControl` 普遍恢复操控的假设：既有调查已区分战役和非战役权限判定。该功能仅处理原版输入锁，不承诺解除所有夺权/战败/剧情对象权限。

## 绑弹步兵上车

沿用已记录的 `ObjectClass::AttachedBomb +0x38`，没有新增对象偏移。自动功能页独立开关与可配置 1–64 格范围，默认 8。每个逻辑帧共用一次已封存世界样本，寻找最近且原生允许进入、有空位的己方载具，正常 Enter 事件经统一网关提交。

与已有装车、合作进驻共用容量预留；最终入队前重新核对炸弹、对象身份、手动版本、目标范围和容量。每次最多轮询 32 个工作项、最多发送 8 条，仍服从总帧额度。手动操作取消对当前这枚炸弹的自动接管；暂停/失焦不清除此记录，炸弹变化才重新登记。无适用目标继续等待；不给自动操作重复播放玩家确认语音。

## 拖框与其他修复

MO 手势消失有两个可复现源码原因：配置 seqlock 正在发布被误当功能关闭；鼠标移动发生于新模拟帧、采样尚未更新时，预览借用了仅允许当前采样帧的下令检查。已捕获的拖动可短时使用同一对局/阵营的最后完整配置；释放/最终下令仍读取最新完整配置。只放宽预览帧检查，并在按下/释放时通过已有分发器更新样本。

另修复 1.0.47 载具气泡“己方/常驻”控件 ID 与防蛛/防空状态行冲突，INI 键保留。

## 验证与未决项

测试路径：`assist/tests/air_events_bomb_board_test.py`、`input_lock_test.cpp`、`input_bomb_settings_test.py`、`mo_orders_bridge_test.py`、`formation_quick_bridge_test.py`、`mo_patrol_planning.cpp`、`air_waypoint_host_test.py`、`air_path_command_admission.py`、`automatic_controls_test.py`、`runtime_feature_demand.cpp`、`hotkey_command_context.py`、`read_only_lifecycle_integration.py`、`passenger_settings_test.py`、`suite/tests/console_display_entry_test.py`。

Windows x86/x64 真实目标布局校验：Assist104、Shared23162608，910 个大小/对齐/字段布局条目一致。新增飞机回执、绑弹设置/计数和输入锁设置/状态均为协议尾部追加。

这些受控测试替换了 Win32、游戏内存、原生调用或事件接收边界，不能证明游戏实际移动、爆炸消除、平台联机同步或 Windows 渲染。完整编译、静态 PE 和包校验以 release-manifest/build-provenance 为准，不在源码笔记中提前宣称发布完成。
