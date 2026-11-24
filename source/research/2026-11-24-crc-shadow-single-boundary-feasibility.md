# CRC Shadow 八点缩减：单事务边界可行性研究

日期：2026-11-24。分支：work/latest-ra2-integration。

## 结论

发现一个更值得优先验证的候选：只保留 Map::Reveal 内的 0x577DB0 写入点，在工具手动 Reveal 作用域内抑制这一次 MapIsClear 写入；自然游戏调用仍执行原指令。这样有机会从八个指令补丁缩减到 **一个代码补丁点**，不再需要 CRC 读取替换或另外六个自然写入观察点。

另有一个零补丁候选：调用前保存、原生 Reveal 返回后比较恢复。它已通过合成事务测试，但安全性不如单点写入抑制：恢复发生在函数返回后，若函数内部触发 CRC 会太晚；恢复写入失败时也没有 CRC 读取兜底。因此将其保留为备选，不作为首选。

本轮只新增研究候选、反汇编证据与合成测试，没有改生产 Runtime、没有删改八个站点，也没有改变现有功能行为。用户已明确确认：只处理 MapIsClear；Cell 与 Visionary 继续完全使用当前正常工作的原生机制，不扩展状态管理。

## 现有证据

### 1. 七个写入点中只有一个在 Map::Reveal 内

当前证据清单给出的原生 Map::Reveal 范围是 0x577D90 起、长度 405 字节，结束位置为 0x577F25（右端不含）。七个直接写 House+0x241 的站点为：

- 0x4F576B：House 构造期写入
- 0x4FC328：自然状态写入
- 0x577ACA：自然状态写入
- 0x577BC7：重置写入
- 0x577CA7：重置写入
- 0x577DB0：Map::Reveal 内写入
- 0x577F48：独立的原生 RevealAll 写入入口

按地址区间求交，只有 0x577DB0 落在 Map::Reveal 函数内。其余六处不是工具手动 Reveal 的直接写入点。

### 2. 工具已经有一个同步调用边界

bridge.cpp 的 RevealMap 分支当前顺序是：

1. crcMapToolBegin(house)
2. 同步调用 Map::Reveal（0x577D90）
3. crcMapToolEnd()

这个调用发生在 Runtime 的游戏线程命令路径。当前 CRC Shadow 正是依赖这个边界设置 crcMapToolScope，只是之后又通过八个 detour 去追踪字段读写。

### 3. 当前静态扫描中的读取者

既有固定 PE 线性扫描列出三个 +0x241 读取候选，进一步反汇编后分类为：

- `0x502FFF`：读取 House+0x241 后调用 `0x4A1CA0`；该 helper 把布尔字节加入 CRC 累积器并更新查表 CRC32 状态，是 CRC 序列化输入。
- `0x64DCCA`：全局 ComputeGameCRC 的 House 字节读取。
- `0x64E250`：读取 MapIsClear 与 Visionary 后传给 `0x7CA7D8` 诊断格式化路径。

因此当前三个直接读取候选均属于 CRC 输入或诊断，不是已知普通游戏玩法读取者。YRpp 的 HouseClass 将 Visionary 与 MapIsClear 相邻声明；MapClass 将 Reveal 声明为 0x577D90。Phobos 的 SyncLogging 也把每个 House 的 MapIsClear 加入 CurrentFrameCRC。这些是字段与摘要关系的交叉佐证，不等于证明整个可执行文件不存在间接别名、批量拷贝、动态模块或扩展读取者。

## 候选机制：保存 → 原生 Reveal → 比较恢复

对工具手动 Reveal 的单次调用：

1. 在调用前读取目标 House 身份（epoch、House 指针、ID、索引）及原始 +0x241，原值必须是 0 或 1。
2. 不改写 Map::Reveal，完整调用原生函数。函数内部仍可暂时把 +0x241 设为 1，原生函数执行期间的判断与副作用不变。
3. 在 RuntimeScopedCall 的 finally 清理中重新核对 House 身份和 +0x241。
4. 若字段已经等于原值，不写入；若字段为 1 且原值为 0，恢复 0 并读回验证。
5. 若身份变化、字段出现非预期值、读写失败，则不覆盖未知数据，保留事务状态并阻止下一次工具 Reveal，直到安全重试或确认旧对象已退休。

这样，工具 Reveal 返回后，原生 CRC 直接读取真实的原始 MapIsClear，不需要 0x64DCCA 输入替换。自然构造、重罩、重置、败北等写入不再需要被工具箱观察；它们直接改变真实字段，后续 CRC 也自然读取新值。自然同值写入也不会被旧影子误判。

明确边界：该候选只处理单个 House 的 +0x241 MapIsClear 写入。Cell 探索/重罩数据和 House+0x240 Visionary 完全由原版维护；工具箱不读取、不采样、不镜像、不写入、不恢复，也不为它们增加 Hook。

## SHA 匹配原版 EXE 的反汇编核验

已对资料库中的 `gamemd-spawn(2).exe` 直接反汇编，并确认 SHA-256 为：

`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`

该值与本研究的 `crc-shadow-research.json` 中固定 PE 哈希一致，不是只按地址猜测的另一份 EXE。

关键指令关系：

- `0x577D9D`：`mov ebx,1`
- `0x577DA4–0x577DAD`：从 Reveal 参数的 `+0x30` 取得 House 索引，再由 House 数组解析实际 House 指针到 ECX。
- `0x577DB0`：`mov byte ptr [ecx+0x241],bl`，写入 MapIsClear=1。
- `0x577DB6`：开始比较 Reveal 参数与当前本地 House。
- 非本地 House 会跳到函数尾部；本地 House 继续执行 Cell/地图揭示流程，并在 `0x577E29` 单独写 `House+0x240`（Visionary），它不是 MapIsClear。

完整函数在 `0x577F21` 执行 `ret 4`。在该函数体内，直接访问 House+0x241 的位置只有 `0x577DB0`；该函数没有直接调用 ComputeGameCRC，也没有在该 store 之后直接读取 House+0x241。对几个直接调用目标的局部代码窗口也没有发现直接的 +0x241 或 ComputeGameCRC 引用。

证据文件：`research/evidence/crc-shadow-single-store-disassembly.json`。

这比此前仅按静态候选区间推断更强，但仍不是完整调用图证明：Map::Reveal 后续会调用地图/渲染相关函数，其中有虚函数与间接调用；局部窗口扫描不能代替对所有间接目标的穷尽分析。现有 Unicorn fixture 在 `0x577DB6` 后提前返回，因此它没有验证完整原生后续执行。但本项目不需要在工具箱中模拟或维护 Cell/Visionary 状态；验收只需确认单点拦截后从原始下一条指令 `0x577DB6` 继续，游戏原生后半段不被跳过、不被改写，并在真实游戏中正常运行。

## 首选候选：单点抑制 Map::Reveal 的写入

现有 CRC 回调已经能在 0x577DB0 执行原始 MOV 前取得 ECX（House）和保存的 EBX（其中 BL 是将写入的值）。反汇编确认该点正是 Map::Reveal 函数内唯一的 House+0x241 写入。候选只在工具调用作用域且 ECX 与事先校验的目标 House 身份完全匹配时跳过这条 MOV；其他 House、自然 Reveal 和异常身份变化都走原生写入，不做全局拦截。因此不必改写 EBX，也不必重算 CRC：

1. crcMapToolBegin 在同步调用前校验当前世界、目标 House 身份并开启工具揭示作用域。
2. 仅在 0x577DB0 回调中，如果作用域有效，就设置一个一次性 skipStore 标志。
3. 裸函数 trampoline 在原 MOV 位置检查标志；为真则跳过 MOV，为假则执行原 MOV。
4. 检查分支必须用 pushfd／popfd 恢复 EFLAGS；只访问内存标志，不修改任何 GPR。回调原有 pushad／popad 继续保护寄存器。
5. 立刻清除一次性标志。工具调用外的自然 Reveal 不设置标志，原指令照常执行。
6. 如果受保护回调发生异常，AssistMapCrcAbort 必须先设置 skipStore，再由 trampoline 消费；不能让异常回退路径意外执行原始写入。
7. 手动 Reveal 外层使用 RuntimeScopedCall finally 清除作用域，确保正常返回、SEH unwind 和提前退出都不会把 skipStore 泄漏到下一次自然 Reveal。

这比调用后恢复更强：MapIsClear 在 0x577DB0 之后就保持原值，CRC 即使在 Map::Reveal 内部被嵌套调用，也会直接读到原值；同时不需要对 House 内存做事后写入。

**仍需验证的假设：** 作用域内到达 0x577DB0 的写入确实都属于这次工具 Reveal；不存在必须依赖该字段被置 1 的非 CRC 后续读取；其他六个写入点不在这次 Reveal 的调用链中。若任一假设不成立，不得采用该方案。

## 为什么不选“只保留一个 CRC 读取 Hook”

单独保留 0x64DCCA 仍需要知道每个 House 的真实原值。没有七个写入观察点时，单纯轮询无法区分“工具把 0 写成 1”与“游戏自然地再次写入 1”。同值写入恰好是当前模型必须捕获的边界。因此仅删七个写入点、留下 CRC 读取点，不是可靠缩减。

事务边界方案不同：它不让工具写入在调用返回后持续存在，因而自然写入不需要镜像模型。

## 当前适用范围

只覆盖工具自己的 RevealMap 命令。当前代码已经在 CRC Shadow 启用条件中排除非零 renderReveal、直接地图模式及 Ares/Phobos；这些限制必须保留。没有把自然 SpySat Reveal、第三方模块、自动全图或其他直接调用 0x577D90 的路径扩张为受支持范围。

如果未来要支持那些入口，必须逐一确认调用者并建立同等事务边界；不能假设所有 Reveal 都经过工具命令。

## 关键风险与尚未证明的条件

1. **函数内是否存在 CRC 重入**：如果 Map::Reveal 在写入 +0x241 后、返回前触发 CRC，事务恢复发生得太晚。需要在固定 PE 上追完整函数调用图，或在 Unicorn 中记录 0x64DAB0 / 0x64DCCA 的执行顺序。
2. **函数内是否有其他 +0x241 写入**：当前静态扫描只看到 0x577DB0 位于函数范围内；仍需完整执行时对所有 House 的 +0x241 写入做 watch trace。
3. **返回后的其他读取者**：现有线性扫描未发现非 CRC 读取，但它明确不覆盖间接别名、宽访问、批量初始化、载入/还原及运行时扩展。若存在依赖 MapIsClear=1 的非 CRC 逻辑，返回后恢复会改变其观察结果。
4. **Reveal 的其他同步副作用**：Cell、Visionary 和特殊格条件 RNG 仍按原生路径执行；工具箱不复制、不镜像、不恢复这些状态。测试不需要重写或模拟它们，但真实游戏验收必须确认跳过单条 MapIsClear store 后，控制流仍从 0x577DB6 进入原生后续代码并正常返回。
5. **异常恢复**：合成测试验证了 finally 模型，但仍需在实际 Windows Runtime 的 SEH 边界与真实游戏调用中验证。写后失败必须保留事务并可重试，不能清除记录或报告成功。



## Hook 选择原则与本轮决策

本轮不把“零 Hook”作为验收目标。若一个原生规则/事件入口或更高层的规则类 Hook 能以更少状态、更少分支、更清晰的所有权保持相同语义，就优先采用它；如果它做不到，保留一个精确、默认关闭、范围明确的 Hook 比引入事后内存回写、镜像状态机或额外线程更简单可靠。Direct Placement、跨 House Network Sender 等已审计功能也不因追求零补丁而强行删除其必要入口。

对 CRC Shadow 当前的候选排序：

1. **优先验证单点写入抑制**：一个 0x577DB0 指令点、一个同步工具作用域、精确 House 身份比较；自然游戏调用完全原生。若完整 Reveal 证明通过，这是当前最小且最直观的方案。
2. **不优先零补丁事后恢复**：保存／调用／比较恢复虽能在合成模型中成立，但需要额外读写 House 内存、异常清理和失败重试，且恢复发生在 Map::Reveal 返回后；如果函数内部发生 CRC 或依赖该字段，时序更复杂。
3. **保留现有八点实现作为安全回退**：在单点候选未完成完整原生调用图、Windows SEH 和双机验证前，不改生产代码、不删除旧路径。

## 本轮验证

新增研究候选头文件和三类测试：

- 零补丁事务模型：覆盖原值 0/1、原生不写、后续自然同值写入、House 身份复用、未知值冲突、写失败重试、读取/读回失败、嵌套阻止，以及原生调用在写入前后异常退出。当前本地使用 g++ -std=c++17 -O2 -Wall -Wextra -Werror 编译并运行，20 项断言全部通过。异常路径由可移植 finally 替身模拟，不代表 Windows SEH 或游戏实测。
- 单点写入抑制模型：覆盖工具作用域内跳过写入、作用域外自然 Reveal、其他自然写入、多个 House 写入、回调异常兜底、嵌套阻止和 finally 清理。当前本地以 g++ -std=c++17 -O2 -Wall -Wextra -Werror 编译并运行，29 项断言全部通过；覆盖精确目标 House、嵌套其他 House 原生放行、同地址身份变化、epoch 变化、SEH abort 仅对目标生效、finally 清理和防止 skip 标志泄漏。它测试的是状态机模型，不是生产裸函数汇编。
- 静态证据契约：核对 RevealMap 单一调用边界、0x577DB0 是函数范围内唯一的已知写入点、三个已知读取候选的 CRC 分类，并明确保留 xref 缺口。

以上均为研究／合成测试，不是实际游戏执行，不是完整 Map::Reveal 验证，也不是双机联机测试。CI 工作流已加入，实际 GitHub Actions 运行结果仍待回报。

交叉参考：固定 YRpp HouseClass 将 Visionary 与 MapIsClear 相邻声明（[HouseClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/HouseClass.h#L869-L872)）；MapClass 将 Reveal 声明为 0x577D90（[MapClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/MapClass.h#L477-L479)）；Phobos SyncLogging 将每个 House 的 MapIsClear 加入 CurrentFrameCRC（[SyncLogging.cpp](https://github.com/Phobos-developers/Phobos/blob/develop/src/Misc/SyncLogging.cpp#L653)）。这些只佐证字段布局和摘要用途，不证明完整可执行文件不存在其他读取者。

## 下一步验收门槛

只有以下项目通过，才改生产代码：

1. 用支持的固定 PE 执行完整 Map::Reveal，不跳过 0x577DB6 后的代码；记录全部 House+0x241 写入、Cell/Visionary 写入、同步 RNG 和 CRC 调用时点。
2. 证明 0x577D90 返回前没有嵌套的 ComputeGameCRC；确认被跳过的写入只针对工具命令指定的 House，其他 House 与自然 Reveal 保持原生行为。
3. 补充 MapIsClear 读取者审计，特别是宽访问、间接地址和扩展模块；不扩大到 Cell/Visionary 状态审计。
4. 在实际 Runtime 的游戏线程 finally 边界中验证正常返回、SEH 异常、写失败、对象退休和重试。
5. 最后才把八个 detour 改为单事务边界，并保留原八点实现作为可回退基线，完成新旧方案 CRC 输入逐帧对照。

如果任一门槛不通过，继续保留现有八点实现，不强行缩减。
