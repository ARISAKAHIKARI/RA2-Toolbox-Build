# 原版自毁未执行：AI DoList 调度与 OutList 清理

问题：用户反馈原版也无法自毁；不能以 Hardened 接收校验解释全部失败。

样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。私有 EXE 不随源码、验证记录或补丁分发。源码基线 `663064b2a5ac9543171adf24a42e5ef50f0e76ff`（1.0.47，Assist 协议 103）。

## 已确认的两处阻断

1. **单机 AI 入队成功但没有原生调度。** 原版 `EventClass` 无参数构造器 `0x4C66C0`，`__thiscall(event, HouseIndex, 8)`，产生 111 字节 Destruct。原版 DoList 调度器 `0x64C380`，受控调用使用三个零参数；House 遍历在 `0x64C4DC..0x64C550` 要求 `House+0x1EC`（本机）或 `+0x1ED`（人类）非零。AI 两字段均零时，DoList 事件仍在，却没有调用 `EventClass::Respond=0x4C6CB0`，executed 位也未置位。
   `destruct_dolist_scheduler_probe.py` 在 0/5/3/4 会话、目标 0/1、人类标记开/关的 16 个组合中执行原始调度器和原始 Destruct 接收分支。仅非本机 AI 被跳过。本机与人类目标能进入 `0x4FC980`，设置目标 `+0x1F6=1`、`+0x298=当前帧`、`+0x2A0=0`。这把 2026-10-26 笔记中未验证的 AI 调度疑点变成受控执行证据。
2. **界面状态错误地撤销已接受的联机事件。** `AssistNetworkSend` 原来使用 `networkHousePreserveReady()`，其中 `snap.commandStatus==Ready` 本是新输入准入条件。原生战术画面不可见（`0xA8E378=0`，也包括特殊对话框/窗口状态）后，Runtime 正常发布 `DisplayHidden=10`，仍保留发送适配组；发送器却把 Ready=false 当成关闭，调用 `networkSenderCancelForeign()`，删掉已入队的 foreign Destruct。旧 DLL 对 protocol 0/1/2 的新回归均得到零发送、零本机/远端 Destruct。

## 修正

- 单机 0/5、明确一次性 Destruct、目标确为非本机 AI 时，由统一事件网关在已有 Assist 游戏线程租约内将原始构造包交给原版 `Respond`。其入口校验字节为 `81 EC 70 03 00 00 33 C0 53 55 56 8B F1`；新增独立调用诊断项。保持构造前及原生时钟回调后的身份、选取、会话、只读状态校验。结果码 2 表示原生自毁标记已观察到；Host 明确显示“已触发单机 AI 所属方的原生自毁”，不计入 queuedEvents。
- 人类所属方仍走原有 SP DoList；联机仅走原有 OutList，AI 联机目标仍拒绝。未更改 House 人类标记、CurrentPlayer、对象 Owner 或原生调度器。
- `networkSenderDeliveryReady()` 将 Ready/DisplayHidden 视为允许继续发送已接受事件；`networkHousePreserveReady()` 仍要求 Ready 才接受新跨所属方输入。停止、心跳超时、只读、会话/世界身份变更、关闭适配、挂钩冲突、目标失效仍不获得放行。
- 修正旧 Unicorn fixture 的 FeatureConfig 20→21、协议 97–103 尾部字段，并支持实际 DLL 使用的 CRT malloc/free。保留 DLL Shared 大小断言，并对照本次 Windows x86/x64 编译布局核验 Snapshot/Shared 的全部字段偏移，不能靠去掉断言运行旧布局。

## 验证及边界

- `destruct_owner_runtime.py`：本次实际 DLL，32 项（含 0/5 × 选取/显式目标 AI 原生执行一次、签名失败、时钟回调期间身份/会话/只读变化、MP AI 不使用本地执行）。同一测试在旧 DLL 的 AI 用例返回 1（入队），没有 Respond，形成失败对照。
- `network_house_runtime.py`：39 项通过；实际 DLL 安装的七处适配、原版发送/压缩/解码；补充三个协议的入队后 DisplayHidden 回归。旧 DLL 三项失败，新 DLL 三项通过。伪造 Win32/world/传输边界，无真实网络对端。
- `destruct_receiver_probe.py`：20 项原版构造/接收执行；`destruct_dolist_scheduler_probe.py`：16 项原版调度及响应。
- `verify_crossbit_abi.py`：900 项 Windows x86/x64 布局一致。真实游戏资源销毁、House AI 后续遍历/结算、Windows 点击体验和真实多人同步仍需实机验证。原版接收分支可执行不等同于平台接受，也不证明本次用户现场只有这两个原因。
- 本次没有修改 Hardened、平台接收校验或 CRC。旧笔记中的跨发送端顺序风险与平台差异仍未解决。

## 1.0.48-pre.1 衔接

用户指出当前版本已为 1.0.48-pre.1。确认正式预发布标签指向 `00a4494708fe409c56246cdc793fe811c777acb7`，发布时间为北京时间 2026-11-10 02:04；其 Assist 协议已为 104，原独立 1.0.47 自毁修复尚未包含。

本次以该发布源码为基线衔接 PR #33：保留 enqueue/submitConstructed 中新增的 BombBoard 与 DirectAir 再校验，保留原生路径点、预览捕获、绑弹自动上车、输入锁解除及乘员控件修复。Unicorn Shared 显式补齐原有 Assist104 飞机报告、绑弹及输入锁字段；产品协议和版本仍为 104 / 1.0.48.1。交付标为 1.0.48-pre.1 的 Destruct-Fix 补丁，不替换原发布标签，也不能用此前 1.0.47 补丁覆盖新版功能。验证结果及具体构建身份见本次补丁 manifest；沿用的原生地址发现不扩大实机结论。
