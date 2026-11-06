# Ares 普通函数入口兼容与科技参数模式修正（1.0.43-pre.5）

## 问题与现场

Runtime(10).log（SHA-256 72b05fe7f6c436573171f5f025425d4cbb00ae25fee0157d85fcacc504c88b67）显示 v1.0.43-pre.3 对 gamemd-ares.exe 的 WH_GETMESSAGE 安装和回调正常，累计回调 44859 次，但始终 commandStatus=3 / signature-mismatch。读取成功的三个 Core 入口为：

| 原入口 | 用途 | 现场前8字节 | 直接目标 |
| --- | --- | --- | --- |
| 0x6AA600 | StripClass_RecheckCameos | E9FB59C60781EC44 | 0x08310000 |
| 0x5F7900 | ObjectTypeClass::FindFactory | E9FB866F0656578B | 0x06CF0000 |
| 0x4F7870 | HouseClass::CanBuild | E98B87DE04909053 | 0x052E0000 |

三个目标均是已提交的可执行 MEM_PRIVATE 页；进程加载 Ares.dll 和 MRGA2.dll。未提供本次 EXE/模块 SHA-256 或目标页本体，不把地址和模块名当作完整构建身份。原始用户日志、私人路径和游戏转储不提交。

调查前查询研究索引及持久 SQLite v145（上一轮定位记录）。pre.3→pre.4 未改启动校验；此前 ClearBeacons/Reveal 的可选依赖隔离仍有效，本次原因是额外三项仍要求原版入口字节。

## 新核对的 ABI 与判断

[Ares 自身 CanBuild Hook](https://github.com/Ares-Developers/Ares/blob/4f1d929920aca31924c6cd4d3dfa849daa65252a/src/Ext/House/Hooks.cpp) 在 0x4F7870 读取 ECX=House、栈 +4=Type、+8=buildLimitOnly、+C=includeProduction，调用 HouseExt::PrereqValidate，并返回 0x4F8361。普通调用仍使用既有 thiscall ABI，Ares 接管函数实现；原生函数体内部的精确比较断点则会被绕过。

[Phobos 维护的 Ares 3.0p1 Hook 表](https://github.com/Phobos-developers/Phobos/blob/ce864b03e56b15f4c59f78955aee87079e54ce76/.agents/skills/check-hooks/ares_3.0p1_hooks.cpp) 同时包含三处入口。本库既有科技记录已确认 FindFactory 参数 allowOccupied/requirePower/requireCanBuild/house（类型虚表 +0x94）。本轮没有新增对象字段偏移。

普通调用的可调用性与修改指令/内部断点的布局兼容性需要区分。保持函数原入口、按原 ABI 调用平台现有实现，不要求平台完整 handler 与原版逐字相同；在某条具体指令改参数/标志或设置内部断点时，仍需检查对应站点与调用条件。这修正了上一轮将完整 handler 采集作为任何普通入口调用前提的过宽约束。

## 代码变化

- native_entry_policy.h / native_entry_runtime.h：只有上述三处已知原生 API 使用平台调用策略。原版字节仍接受；改写路径要求当前已加载的 Ares.dll 是合法 PE32/x86 映像，入口起始为 E9，相关内存可读且可执行。有界跟踪 E9、EB、FF25 分派边，拒绝空/低地址、溢出、环、过深跳转、Guard/不可执行页及无 Ares 环境。允许正常私有可执行跳板；不写、跳过或替换平台代码。
- bridge.cpp：三项启动检查使用上述普通调用策略；其余检查与两项 optional 消费者检查保留。实际调用地址保持原生入口，Ares 自己的链仍执行。
- 可调用检查使用短期缓存以避免热路径反复查询映射，科技断点冷准入强制复核；实际热回调继续比较捕获的入口字节。
- tech_ui_breakpoint_bridge.h：全科技参数模式仍严格校验两个已知原生 UI 调用点；CanBuild 入口采用可调用检查及前后稳定的实际16字节快照。断点只调整已有调用的 buildLimitOnly 参数，不覆盖 Ares 的 E9。
- 精确科技增强依赖原生 CanBuild 内部比较位点，Ares 替换函数体时返回局部 -8，界面明确提示选择规则模式。保留用户选择，不自动改为规则写入，不影响全局激活或其他功能。
- 保留原有 Hook 所有权、清理、重连、在产队列保护和规则模式交接。生命周期测试夹具补上已有单机 FPS 槽的退休状态，生产清理逻辑未因此改变。

## 验证与交付边界

新增 native_entry_runtime.py 从生产源码提取真实启动检查/后续 gate，使用现场原入口字节和合成 Win32/跳板本体验证接受、拒绝与缓存恢复。既有 startup_dependencies.py 继续覆盖可选命令独立拒绝与 owned/foreign 清理。科技参数回归检查全科技模式实际回调的参数改变、入口变化后停止处理、Exact 局部状态及原有科技增强交接。

可复现测试入口及最终计数保存于同目录 evidence/ares-native-entry-20261107.json。产品以 Zig 0.13.0 重新成套编译 x64 Host、x86 Runtime/broker，PE、版本、ABI 与构建来源由发布 manifest 记录。合成目标页并非用户现场跳板；受控回归与交叉编译不等于 Windows 实机、Ares 全部科技行为或多人联机验收。

后续若某个平台实际修改了 API ABI、侧栏数据结构或具体断点调用站点，需要对应证据进一步适配。本轮不把所有 E9、所有平台或所有游戏版本列为通用支持。

