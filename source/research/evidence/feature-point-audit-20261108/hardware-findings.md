# 硬件入口、科技、控制、地图、原生放置、超武与 Hook 诊断逐点审查

## 结论和范围

本子任务已完成实际 UI 开关、模式、快捷键和 dispatcher 对应的 **82 条逐点源码审查记录**。原始 79 行之外，补充了铁幕、超时空和按超武类型 ID 绑定的三条独立快捷键。每行都有入口、已看路径、结论和验证缺口；不再有 `inventoried` 或 `source_review_partial`。82 行中含两条内部不变量及一条已禁用入口，不能作为全产品功能总量，更不能当作实机通过数。

机器可读明细见 `hardware-matrix.json`。状态为：79 条 `source_reviewed`，1 条 `fixed_controlled_verified`，1 条 `disabled_verified_source`，1 条 `source_fixed`。源码审查与实机验证分开记录；本轮没有逐功能 Windows 游戏执行或多人同步实验。

审查基线为 `c3154fa343a5be42da3453c0b3d228dea653d3ac`。本子任务修改的生产文件仅 `source/game_runtime/source/frame_signal_runtime.h`，新增验证脚本仅 `source/assist/tests/frame_observer_maintenance.py`，由主代理纳入 `38c6a71f840615986c6d37e1038034183c466566`。主代理冻结生产文件后，本子任务没有再改 `source`。未修改 Assist 主机入口、共享 broker、版本、INDEX、Notebook 或远端。

## H-FRAME-001：未开帧诊断仍逐消息读取并探测——已修复

### 确认的问题

`frameobserver::maintain` 原先在判断是否存在诊断需求、是否已经安装、是否需要撤销之前执行 `multiplayerpatch::mayInstall` 和 session 读取。一条维护调用通常发生六次 ReadProcessMemory；只要提供 controller owner 而映射尚不存在，就每条消息调用 OpenFileMapping。普通消息消费者或已经安装的诊断通道也重复进行同样的就绪读取。

这是可确认的热路径额外工作。计数实验没有测游戏帧率，也不能推出每次都会卡顿或死锁。

### 最小修改

- 取消选择、控制器消失、owner 变化、已写入后恢复失败等所有权清理仍在函数前部立即执行。
- 只有已有有效通道、显式选中帧诊断、heartbeat 可用、并且需要新安装时才读取游戏就绪状态。
- 没有通道时，按 owner 每 100ms 探测一次。owner 改变即刻探测；owner 为零清除探测身份。
- 保留通道的普通消费者可以后来设置 `ExplicitDiagnostic`；下一次维护直接看到新选择，无需销毁重建 IPC。
- 已安装且仍被选择的诊断入口不会因短暂 heartbeat 过期被强制撤掉。这保持原实现的恢复和暂停语义。
- unsigned tick 差值维持回绕安全。无法恢复旧 hook 时先返回，保留旧 channel/owner，防止新 owner 越过旧所有权。

**100ms 是新诊断通道的发现间隔，不是关闭或撤销的延迟。** 发现还取决于后续维护被调用；本补丁没有承诺系统调度的绝对时间上界。

### 生产函数受控计数

测试提取生产 namespace 状态、`releaseChannel` 和 `maintain` 原文，省略不会执行的 x86 committed adapter；替换 Win32 IPC、时钟、读内存与 owned-patch 服务边界。基线及修改后使用同一驱动、同一输入轨迹。编译为 Linux 本地 C++17，`-O2 -UNDEBUG`，断言未关闭。

| 轨迹 | 基线内存读取 | 修改后内存读取 | 基线映射探测 | 修改后映射探测 |
| --- | ---: | ---: | ---: | ---: |
| 未创建诊断通道，虚拟 1 秒内 100000 条消息 | 600000 | 0 | 100000 | 10 |
| 没有 controller，10000 次维护 | 60000 | 0 | 0 | 0 |
| 已映射的普通非显式消费者，1000 次维护 | 6000 | 0 | 1 | 1 |
| 已安装诊断，1000 次维护 | 6000 | 0 | 0 | 0 |
| 已选中但 heartbeat 过期，一次维护 | 6 | 0 | 0 | 0 |
| 取消选择，一次维护 | 6 | 0 | 0 | 0 |
| 同一通道重新启用，一次维护 | 6 | 6 | 0 | 0 |
| 控制器死亡，一次维护 | 6 | 0 | 0 | 0 |
| tick 回绕，三次维护 | 18 | 0 | 3 | 2 |

取消选择即执行一次恢复；控制器死亡即一次恢复、一次 unmap、两次 handle close。同一通道重新开启即一次安装；仅真正新装时仍有六次就绪读取。100000 次作用域入口仍为 100000 次，未声称消除全部维护成本。

受控断言另覆盖：magic/version/PID/owner/enable 不匹配、映射失败、事件失败、stale heartbeat、不可用 world/read、允许与拒绝的 session、新 owner、不完整安装已写入后的清理、恢复失败保留旧 owner、策略 revision 后重新准入。基线和修改后 `semantic_assertions` 都为 `passed`。

证据：

- `frame-maintain-before/result.json`，基线生产 header SHA-256 `47987d631e8e1e1830bf4ffb72f7b428189b2e9432e38f18ba017ac113ccd92b`。
- `frame-maintain-after/result.json`，修改后生产 header SHA-256 `4f7882be0e519487ec65d21b40cfdc790189b7f90e2f469aeec0e224b28b9c91`。
- 两个目录内保存提取的 `maintain-production.cpp` 和本地执行文件。
- `source/assist/tests/frame_observer_maintenance.py` 保存提取方式、所有输入、断言与完整编译命令。

验证边界：没有执行真实 x86 detour/SEH ABI、实际 Windows IPC、实际游戏帧或多人联机。只报告调用数量，不报告耗时倍数、FPS 提升或 OOS 已解决。

## H-DIAG-002：Hook 诊断日志目录与协议文案过时——已源码修复

修复前，`source/hook_diagnostics/source/main.cpp` 的开始日志硬编码：`48 available unique sites + 3 product-disabled + 1 Cursor-only + 14 explicit experimental sites`，另一个标题写 `Assist89 / Runtime4`。

直接编译生产 `catalog.h` 并调用 `allSites/covered/dormant/cursorOnly/experimentalOnly`，当前结果为：

```
allowlisted=82 covered=48 dormant=3 cursor=1 experimental=30 missing=0 steps=21
```

当前 `source/assist/source/protocol.h` 为 `Version=101`。因此硬编码实验站点 14 应为 30，Assist 协议文案 89 也过时。这里的 82 是代码站点数，恰好与本报告的逐点审查行数相同，两者没有对应关系。

调度、准入和逐站点检查仍来自生产动态目录，因此已确认的影响是诊断报告准确性，不是漏掉 runtime 安装检查。当前目录每个站点要么被步骤覆盖，要么明确属于禁用、Cursor 或独立实验；受控计数的 `missing=0` 只证明目录映射，没有证明 21 个步骤都实机运行。

证据为 `catalog-count.cpp`、`catalog-count`、`catalog-count-result.json` 及上述生产源码。主代理现已将协议标题改用 `assist::Version/runtime::Version`，四类站点数改为 `std::count_if` 调用现有 `covered/dormant/cursorOnly/experimentalOnly` 分类函数，并删除硬编码 denied 数字。已静态复核当前源码；本子代理未修改源文件。尚未执行真实 Win32 日志生成，因此状态为 `source_fixed`，不称运行时日志验证通过。

## 逐点审查中确认的行为边界

下列是明细矩阵的导航，不能替代逐行证据。

### 科技和硬件入口

五种科技模式分别核对。受控和经典只替换两处侧栏资格调用，经典保留其独立 factory 参数；直接模式使用五点 Master 组；规则数据模式维护九字段身份及恢复日志；参数模式通过四物理槽预算内的硬件入口，只改变已知侧栏调用上下文。生产/取消在规则数据、参数模式由 runtime 拒绝。进入和退出模式还检查在途产品，避免恢复许可时让原生侧栏误取消生产。

许可搜索、逐项切换、全选、清空、Ra2Tool 预设、Assist.ini 自定义配置、生产、取消、只读进度分别登记。原库存把 control428 称为“查看许可列表”，核对实际 handler 后改为选择 Assist.ini 自定义许可。

硬件预算与异常路由核对了 foreign DR 槽、DR6/DR7/EIP 所有权、owner-thread rendezvous/witness、失败后的可能拥有日志、VEH 退出和控制器清理。无需求预算检查仍可能每500ms进入一次自己的 rendezvous，不宣称硬件服务零成本。

### 原生放置和控制

受控、直接和事件放置是三条独立路径。事件模式核对完成产品、当前栏目、factory/product/type/House UID、匹配代数、viewport transform、输入 down/up 对和右键/Esc取消。排队成功并不等于放置完成；有界回执在同一个产品确实上图后才清光标。

控制全部三模式、命令所属方、网络所属方保留、原生出售、自毁分别登记。网络七点组处理压缩和非压缩封包、同House前缀、native CRC/时间戳保留、pending foreign OutList 的关闭排空以及跨世界不沿用旧身份。出售保留原生 CanSell 和 receiver；自毁只允许新鲜的一次用户请求，并在提交前重取一致选区。`SwitchPlayer/RestorePlayer` 的产品开关为 false，runtime 即使收到旧请求也返回 -4；它只算拒绝路径审查，不算可用模式。

### 地图、CRC、超武、FPS、暂停

四种地图模式、手动揭示、硬件显示及八点 CRC 影子独立登记。CRC 只影子化 MapIsClear；关闭只恢复自己仍拥有的字节，不声称撤回已探索 cells 或修复所有 CRC。

超武逐帧授予、面板显示、拖动/折叠、点选/取消、铁幕、超时空、类型ID快捷键均已核对。面板只读快照不得单独授权操作；release 和 runtime 都重新核验本局身份。无目标型走原生事件，目标型先完整检查 cursor helpers 再修改本地目标工具。已授予的超武不会因停止逐帧授予被撤销。

自定义FPS只允许session0/5，0或10..240，有界等待和暂停/控制器身份退出；缺实机 cadence 验证。暂停以原生loop gate工作；失焦/菜单解除暂停，单帧步进出现跳帧会报告 overshot，不把多帧执行当作单帧成功。

原生任务成功通知只观察特定 QueueMegaMission 成功 append，更新工具元数据，不再次发事件；不覆盖 direct DoList、QueueMegaMissionF、receiver QueueMission。精确原生战斗查询仍是显式实验项，SEH 拦访问异常不证明 MOD 虚函数无副作用；该限制已逐行保留。

### Hook 诊断

21 个步骤分别核对 configure/签名/精确注册表数量/非目标站点基线/恢复状态机。另有排除共享依赖、跳过、停止、继续、焦点、OOS、持久化和日志八条流程。CRC 步骤只 arm，不自动 Reveal；基线可选且默认关闭。用户“正常”记录不能代替未发生网络流量的验证；跳过明确记为未测试而不是通过。恢复未完成不会进入下一组，用户关闭也先清理订阅。

## 编译和验证记录

本子任务的生产函数受控实验及动态目录计数均执行成功。没有为未改变的全部功能跑无目的全量回归。

额外尝试编译既有 `cursor_tooltip_windows.cpp` 独立 fixture，工具链完成前端但链接失败：

```
lld-link: error: undefined symbol: _RuntimeScopedCall
```

该独立命令没有链接 MSVC SEH 的 runtime-scope 实现。**这次 fixture 没有构建成功、没有执行，不能列作 Windows 测试通过。** 没有为此去改生产文件或添加假的 scope 实现。主代理正以真实集成构建验证 x64 Host 与 x86 Runtime/Broker，最终编译结论以主代理记录为准。

本轮未执行 Windows 游戏、多人联机、真实消息调度时延、画面/鼠标交互和全部 MOD 兼容组合。矩阵中 `validation_gaps` 均明确保留相应缺口；不存在以“查过模块”替代“所有功能测试完”的结论。
