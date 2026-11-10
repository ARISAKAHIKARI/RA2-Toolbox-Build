# 科技加强预定准入修复（1.0.49-pre.2）

## 问题与范围

用户要求修复 JJ 联机中科技全开／加强的点击失效，补充旧式非硬件实现也失效。前轮日志不能区分生产事件未生成、发送被拒、执行拒绝或随后取消；本轮不把其中任何一种推测写成平台根因。

本次修复已由源码确认的预定链路缺口，基于远端 `2c066d5f41bb0fb8142a763ee46f8e898cb077ea`（本地等价 `2e7fada`）。原生侧栏直接点击的 JJ 故障尚未证实修复。现有原生／私有 Hook／私有断点三个入队模式保留，不自动切换，不新增断点槽，不屏蔽所有 Abandon，也不自动重发已受理的 Produce。

调查前检索研究 INDEX、`2026-11-10-jj-production-regression-audit.md`，以及持久笔记 v178 中 `2026-11-10-jj-tech-production-and-cadence`、`tech-131-1`。

## 确认的缺口及改动

1. Runtime 的预定生产普通调用 CanBuild，没有 Exact probe token；而加强补图标使用 `GuardedTechEnhancementProbe`。Exact 不写 Type::TechLevel，所以图标已放行的类型仍可能在预定提交前得到 -9。现在先保留普通／全开 buildLimitOnly 原生判断；普通判断拒绝且不属于全开时，再使用现有带 token 的加强 probe。全开上限判断失败不会退到其他许可。probe 本身的同局、House、类型身份、签名、断点准备及 SEH 规则没有扩大。
2. Host 预测读取 Type+0x634，将原始 -1 当作不可建造。现在显式区分全开与 Exact：后者只针对 21 项规则的正确类型数组、索引、虚表和 ID，模拟 -1 分支及可选等级 1 比较；国家、Required/Forbidden、偷科技、前置建筑、工厂类别／海陆条件保留。原规则实现仍读实际规则值。
3. Host 的预测不是最终许可，不能取消仍有原生侧栏图标的预定。现在只在没有原生图标且预测持续失败时执行原有取消逻辑。Runtime 仍复核当前图标、CanBuild、工厂及事件准入；图标消失、用户取消、换局与其他既有取消路径保留。

## 原生依据与 ABI

本轮对私有运行期映像 `00-clean-game.mem`（SHA-256 `94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd`）只读反汇编；没有将其标为当前 JJ 映像，未公开该文件。

- CanBuild `0x4F7870`：thiscall，ECX=House，参数 Type、buildLimitOnly、includeProduction；预定和 probe 都使用 includeProduction=true。
- `0x4F794A` 比较 Type+0x634 与 -1，`0x4F7951` 为 JNE。
- `0x4F7B60` 比较 Type+0x634 与 EBX 玩家科技等级，`0x4F7B66` 为 **有符号 JG**。仅跳过 -1 检查时，-1 仍参加原始有符号比较；LUNR 的正等级不自动变成 1。勾选额外比较才按 1 比较。Host 预测保持这个差别。
- 既有侧栏 prune `0x6AA75F..0x6AA788` 仍查真实工厂及 CanBuild；本轮未改此原生路径或游戏指令。

此部分为反汇编证据，非当前版本的整段原生执行或实机证明。

## 验证

- `source/assist/tests/technology_reservation_test.py` 提取并编译最终生产函数，74 个组合验证原生成功、全开上限拒绝、Exact probe、关闭模式、缺类型／依赖；原生调用与 probe 返回由显式替身提供。
- `source/assist/tests/construction_host_test.py` 执行实际 Host 队列与 Catalogue，新增 21 类型 × 19 条件 = 399 项：等级、可选比较、关闭／规则模式、国家／前置／偷科技／工厂、错误虚表／ID／数组成员和读取失败；另测原生图标保留预定与图标移除后的取消。原有 FIFO、ACK、暂停、手动取消和布局回归同时执行。
- 对固定旧源码运行相同新增回归：普通预定未进入 Exact probe、预测原始 -1 拒绝，分别出现预期断言失败；不以编译错误当作回归复现。
- 测试使用 AddressSanitizer/UBSan；当前环境不支持 LeakSanitizer 的 ptrace 组合，关闭泄漏检测。Win32 展示与游戏世界为受控替身，不是实机或联机测试。
- 最终配套 Windows 目标构建、PE/ABI、输入哈希和打包结果单独绑定在交付凭据；本源码说明不提前声称构建已完成。

仍未知：JJ 原生直接点击的具体事件序列、平台当前完整修改及双机实际结果。本次没有提高维护频率，也没有修改联机接收／执行检查。
