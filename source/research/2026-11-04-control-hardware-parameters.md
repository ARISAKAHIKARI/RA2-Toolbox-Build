# 控制全部：一个硬件入口与共用四槽

基线为 main `5253aef9c15659e3ed2df3412cc18311338b9fe7`。用户要求参照新版科技全开，控制全部不改游戏代码；随后询问断点数量及精确科技等级为何占两槽。已查研究 INDEX、Master 原生 ABI 核对、科技参数实现和持久 SQLite 笔记。现有原生依据限定于既有样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`；本轮没有重新运行该私有 EXE，没有启动游戏或联机，不增加 MOD/平台支持声明。

## 控制路由

控制入口 `0x50B6F0` 的一个 hardware execution breakpoint，在原指令执行前接收异常。`ECX` 仍为 House、`[ESP]` 仍为原返回地址。返回地址为 `0x6339BF` 或 `0x637E45` 时，只把 CONTEXT.Eip 指向原生严格 House 比较叶 `0x50B716`；其他已通过上下文和 House 身份检查的调用转向原生 true 叶 `0x50B710`。叶函数正常执行自己的返回，保留原 CALL、返回地址及栈平衡。

因此三个旧补丁站点都不需要新写入。没有设置 INT3、JMP、修改 vtable、CurrentPlayer、House+1EC/+1ED 或单位 Owner。启用前要求三个站点都是精确原值，允许既有记录中的两个 forward 前缀并核对原生尾段；无所有权的 Master after 也拒绝。已有己方代码补丁会先按旧所有权恢复，再请求硬件组；不能把这个清理过程说成从未写过原指令。

回调检查模式、stop/fault、Scenario/Session/Frame、本机 House/UID/非观察者/未结束状态及目标 House 数组身份。热路径的固定字段读取在现有 AV/in-page SEH 边界内直接读取，避免每个字段一次 ReadProcessMemory；任何失败都在修改 Eip 之前退出。回调不分配、不呼游戏、不写规则、不入事件队列。它仍是共用权限拦截，不能声称只影响 UI，也不能由无代码写入推导联机同步安全。

## 四槽分配及科技全开合并

Windows x86 地址断点只有四个物理槽。Service 保持四组物理地址/控制位，逻辑站点容量增为五，逻辑编号不等于新增硬件寄存器。沿用同一 Service/VEH、线程归属和事务式申请；保留 foreign 槽，不通过轮换制造覆盖空窗。槽位不足不改变已有已确认组，不自动回退代码补丁。

| 功能 | 新逻辑 tag/mask | 物理槽 |
| --- | --- | --- |
| 科技全开参数 | tag0 / 1 | 1 |
| 精确科技加强：-1 检查 | tag2 / 4 | 1 |
| 额外玩家科技等级比较（默认关闭） | tag3 / 8 | +1 |
| 控制全部参数 | tag4 / 16 | 1 |

旧 Full tag1 不再请求。科技全开改为在 `CanBuild 0x4F7870` 入口按返回地址 `0x6A97D7`、`0x6AA786` 分类；入口栈为 return / Type / buildLimitOnly / includeProduction，只把已为 false 的 `[ESP+8]` 变为 true。draw 的 includeProduction=0、prune=1，其他 caller（包括正常生产和自己的精确等级 probe）拒绝，House/Type 身份、原 CALL/入口签名和允许列表检查仍保留。原生工厂、OwnerMask、BuildLimit 等接收与生产条件没有被另外改写。

精确等级涉及两个 Jcc：`0x4F7951` 处理 TypeLevel==-1；独立开关 9472 控制 `0x4F7B66` 的 TypeLevel>HouseLevel 比较，默认 false。两处各只更换对应算术 EFLAGS。关闭第二处时原生上限比较继续读取实际 TypeLevel，不可宣称完整虚拟等级始终为 1；开启后才按等级 1 比较玩家上限。三项默认共 3 槽，额外比较开启共 4 槽。已有依赖生产时等待安全切换，新增槽安装后再次由游戏线程检查生产再启用对应回调。

界面 9473 显示总计/工具/其他/空闲；预算来自主线程 Suspend/Get/readback/Resume 的实际寄存器，而非设置项数量。闲置每 500ms 只读检查，不安装 VEH、不写 DR；有效快照最多保留 1000ms。非零地址但未启用的其他槽也视为保留，GD 或读回失败视为未知。界面依据所有已请求组联合预留，检查未就绪或不足时禁用新的开关，阻止已开启功能切换到需要额外槽的实现；已选开关始终允许关闭。检查与安装间的外部竞争由运行端事务再校验，拒绝时保留原有已确认组，不承诺无法发生外部竞争。

## 生命周期与设置

`Experimental/ControlImplementation=0` 现在选择硬件模式（默认），1 保留旧原生 Hook，2 可手动选择旧 Master。ControlAll 默认仍关闭；旧 slot0 设置迁移到用户要求的新实现。未知配置值归零，不能被 clamp 到 Master。命令所属方和网络保留偏好保留，但硬件模式的实际请求与原 Master 一样被禁止；这不是完整外方联网命令处理。

切换到代码模式前必须确认控制 DR 组已退休。切屏保留组；stop、fault、世界清理与显式关闭先即时关闭回调权限，再等待 helper、DR 读回和 VEH 所有权清理。共同 handler 被其他已知组保留时允许本组正常 Off；不确定 lease 或撤回失败不提前确认退出。部分控制状态为原值/模式冲突、槽不足、系统、foreign、线程错误，UI 保持明确错误与收纳展开。

Assist 协议从 92 升 93，追加可选等级比较字段与一个原子打包的槽预算字段；Shared 为 6585000，主程序和 Runtime 需要配套更新。入口、broker 及游戏网络协议未增加新字段。`Experimental/TechEnhanceCompareLevel=0` 默认关闭，INI 读写和 Shared 同步均独立于科技加强主开关。

## 验证与限制

`control_parameters.py` 抽取正式控制 tag4 与 Full tag0 回调分支执行：严格 caller、只改 Eip/对应 stack bool、House/世界/即时停用/原始签名拒绝等回调检查及槽预算枚举共 2752 项。`host_control_modes.cpp` 检查 42 项模式、配置归一化、保存偏好和 UI 依赖。正式 control_sites bridge fixture 覆盖硬件零新补丁、切屏、槽不足不 fallback、未撤回 DR 阻止安装旧代码模式，及原有 Master/Native 安装恢复。跨位数 PE 对照核对 783 项、Assist93、Shared6585000，不执行 Windows 镜像。

`control_parameters_windows.py` 在独立 x86 Windows 测试程序使用正式 Service 和原生样式 leaf/caller，检查一个真实 DR 同时保留严格编号、原代码/House 字节不变、只读预算、外部占槽、Full+Enh+Control 三槽/可选四槽、第五逻辑站点拒绝保留原组、tag0→tag4 同槽读回失败的双候选所有权、混合 foreign exception 完整上下文传递及最终清理。每次 30000 控制调用，三次记录 QPC；这是简化自有程序的异常成本，不是完整 production classifier、游戏帧率或联机结论。旧四站科技与 handler 撤回诊断也在 Windows CI 回归。输出在编译流程的 control-hardware/technology-four/retirement/abi.json artifact，正式编译产物绑定对应 GitHub SHA。

本轮不发布私有 EXE、内存 dump；不隐藏 debug registers，不改平台检查，也不覆盖其他工具的异常与槽。完整游戏画面、移动/攻击/出售全流程、其他线程调用覆盖、高速游戏命中量和多人同步仍需实机验证。
