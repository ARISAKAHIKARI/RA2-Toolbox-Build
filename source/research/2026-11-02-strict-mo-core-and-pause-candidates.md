# MO 功能与严格不改游戏代码架构（2026-11-02）

## 当前实现及基线

严格策略逐项对照仓库既有 `experiment/no-inline-game-patches-20261027` 的
`87c8b05805619e4add9bee33bb2fda6704b7d23e`，没有整体换回旧版本。本轮保留当前
宿主 Hook 回收、跨位数 broker、事件仲裁、协议与 MO 命令功能，只迁移严格拒绝
策略及协议隔离。

- `gameentry::Version=4`，`CodeWritesAllowed=false`；Request 仍为 108 字节。
- `runtime::Version=5`，Control 仍为 176 字节；命名改为
  `Local\\RA2Runtime_v5_<pid>_<owner>_`，隔离旧进程中已固定加载的 DLL。
- DLL 导出 `GameCodeWritePolicyInfo={1,0,0}`，含策略版本、安装许可、恢复许可。
  x86 宿主和 x86 broker 均检查该精确三元组；不能仅凭文件名接受旧运行时。
- 管理器旧 `applyPatch` 写入器已删除；通过合法 ABI 的 Patch 请求立即返回
  PatchDenied，早于读取目标、修改 lease/registry 或调用任何 Windows API。
  无论代码、虚表、操作数、安装还是恢复，都没有例外。
- client `patch()` 也在编译期直接拒绝，不读取 before/after 指针；Registry 的
  `mayApplyPatch()` 永远为 false。历史 Registry 分类只作诊断元数据，不产生写入。
- Windows 消息回调、数据读取、原生事件队列及游戏原生函数调用继续共用现有
  Runtime 入口。这是“不改游戏指令/补丁”，不是“完全不写游戏内存”。

严格策略要求新游戏进程，不能替旧 Runtime 清理已写入的补丁；恢复本身也是写入。
本轮没有真实 Windows 游戏或联机执行。

## 新功能的边界

`0x7E2908` 是 AircraftType 虚表函数指针槽，语义上是虚表数据而非原生函数体，
但旧实现仍要改写游戏映像并改变调用目标。不能把它称为严格版已迁移的路径。
本轮 strict `airpathpolicy::native()` 恒 false，单机和联机均回退已有兼容路径点
事件队列；显式完整原生选项也不能绕过该策略。

暂停门 `0x55D360`、缩放/侧栏的原生入口改写不能在严格策略下安装。
暂停 Runtime 的入口、原生 mini-loop 调用及安装路径均有编译期拒绝，状态 `-5`
明确表示“严格架构等价实现未完成”；不会只把复选框关闭后继续调用旧安装器。
快捷键保存改用 Shared 的 `pauseViewKeys[2]` 和现有普通按键规则，单按/松开/双击
由共用 `hotkeyroute::Registry` 处理；strict 状态不注册或消费它们。

游戏帧完成 observer 的 `0x55DE81` 也不能安装，帧通知通道报告
CodeWritesUnavailable，而不是伪报“已启用原生通知”。已有读取帧计数回退可继续采样。

## 无代码暂停候选：尚未实现

核对上游 Phobos `5a8fbebf189642a08a4988adbac889084f49adb2` 的
`src/Commands/FrameByFrame.cpp` 及其 YRpp 子模块
`d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 `ScenarioClass.h`、`Unsorted.h`：

| 已有接口 | 原生地址/成员 | 证据及限制 |
| --- | --- | --- |
| ScenarioClass::PauseGame() | `0x683EB0`，fastcall、无参 | 原生暂停入口候选；未执行验证完整副作用 |
| ScenarioClass::ResumeGame() | `0x683FB0`，fastcall、无参 | 与前者配套；不能假定仅翻转一个 bool |
| ScenarioClass::IsGamePaused / PauseTimer | 场景数据成员 | 标头已命名；本轮没有新的私有 EXE 偏移/读写验证 |
| Game::ShowSpecialDialog() | `0x48C8B0`；SpecialDialog=`0xA8EDA0` | 原生菜单可暂停；模态菜单不能等同于暂停后战场下令 |
| Game::IsActive / IsFocused | `0xA8E9A0` / `0xA8ED80` | 运行/焦点状态；不当作用户暂停开关写入 |
| TacticalActive / UserInputLocked | `0xA8E378` / `0xA8ED9C` | 战术显示及模态输入状态，不能假称冻结完整模拟 |

上述权威标头未提供所谓 `Game::UserPaused` 地址，不能臆造。Phobos 的真正逐帧
仍 hook `0x55D360` 并在多个 MainLoop 尾部消耗步进计数，这不提供严格版的
无补丁等价实现。MO 使用同一主循环门，不能直接接入后称作不改代码。

候选原生 PauseGame/ResumeGame 若只需原生调用即可工作，有机会迁移到统一
Runtime 请求入口，但必须先验证：是否保持可观察和可下令的战术界面；世界计时、
生产、弹体、动画是否同时冻结；普通消息回调是否继续；读档/退出/失焦恢复；
单步是否只推进一个模拟帧。仅在高频采样中反复暂停/恢复会有漏过多帧的竞态，
不能作为已实现的精确逐帧。直接改单位任务或全局帧计数也不能代替完整模拟暂停。

本轮只有源码审计；没有为这些候选新增“已支持 EXE SHA-256”的断言，也没有写
用户的私有可执行文件或转储。

## 验证

- `strict_code_write_policy.cpp` 直接包含产品管理器与既有 test-only Win32 模型，
  实际执行 931 项检查：非法/合法目标及长度、虚表、合法执行 lease、历史 owned
  Restore、旧 ABI、拒绝前 OS 调用计数和对象/registry 不变、精确策略导出及负例。
- `mo_pause_air.cpp` 实际执行：严格模式所有原生飞机模式回退普通事件，未知会话
  仍拒绝；暂停纯状态保留按实际帧计数推进/异常跨帧检测。
- x86 manager 对象反汇编确认 Patch==2 分支在首次 GetCurrentThreadId 前直接返回
  `-5`；符号表不含 VirtualProtect、WriteProcessMemory、FlushInstructionCache。
- Runtime 与 manager x86 编译通过；最终整套 x86/x64 编译、导出/跨位数验证由
  本轮统一构建记录。以上不等于 Windows 游戏或真实双机测试。
