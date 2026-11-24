# CRC Shadow 八点缩减：单事务边界可行性研究

日期：2026-11-24。分支：work/latest-ra2-integration。

## 结论

本分支已将 CRC Shadow 运行时切换为单点实现：只在工具手动 Reveal 作用域内，于 Map::Reveal 的 0x577DB0 抑制目标 House 的 MapIsClear 写入；自然游戏调用、其他 House 和其余写入继续执行原生指令。运行时只安装一个代码补丁点，不再做 CRC 读取替换、字段镜像或事后数据恢复。该路径仍是默认关闭的高危实验功能；Windows 完整构建、完整 Map::Reveal 和双机联机验收尚未通过，不能发布或视为认证。

另有一个零补丁候选：调用前保存、原生 Reveal 返回后比较恢复。它已通过合成事务测试，但安全性不如单点写入抑制：恢复发生在函数返回后，若函数内部触发 CRC 会太晚；恢复写入失败时也没有 CRC 读取兜底。因此将其保留为备选，不作为首选。

研究阶段最初只新增候选、反汇编证据与合成测试；本次继续推进已把单点实现接入运行时代码，并移除生产八点镜像/恢复路径。用户已明确确认：只处理 MapIsClear；Cell 与 Visionary 继续完全使用当前正常工作的原生机制，不扩展状态管理。源码切换不等于游戏验收。

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

这个调用发生在 Runtime 的游戏线程命令路径。现在该边界通过 RuntimeScopedCall 的 finally 清理单点作用域；只有 0x577DB0 的写入 trampoline 会检查目标 House 身份。

### 3. 当前静态扫描中的读取者

既有固定 PE 线性扫描列出三个 +0x241 读取候选，进一步反汇编后分类为：

- `0x502FFF`：读取 House+0x241 后调用 `0x4A1CA0`；该 helper 把布尔字节加入 CRC 累积器并更新查表 CRC32 状态，是 CRC 序列化输入。
- `0x64DCCA`：全局 ComputeGameCRC 的 House 字节读取。
- `0x64E250`：读取 MapIsClear 与 Visionary 后传给 `0x7CA7D8` 诊断格式化路径。

因此当前三个直接读取候选均属于 CRC 输入或诊断，不是已知普通游戏玩法读取者。YRpp 的 HouseClass 将 Visionary 与 MapIsClear 相邻声明；MapClass 将 Reveal 声明为 0x577D90。Phobos 的 SyncLogging 也把每个 House 的 MapIsClear 加入 CurrentFrameCRC。这些是字段与摘要关系的交叉佐证，不等于证明整个可执行文件不存在间接别名、批量拷贝、动态模块或扩展读取者。

## 对比方案（不采用）：保存 → 原生 Reveal → 比较恢复

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
- `0x577DB6`：开始比较 Reveal 参数与当前本地 House；单点拦截必须从这里继续执行原生指令。
- 非本地 House 会跳到函数尾部；本地 House 继续执行原生地图揭示流程。`0x577E09` 先加载本地 House 指针，真正读取 `House+0x240`（Visionary）的是 `0x577E0F`，并在 `0x577E29` 按原版写回。这个字段只用于区分原生流程，工具箱不接管它。

完整函数在 `0x577F21` 执行 `ret 4`。在该函数体内，直接访问 House+0x241 的位置只有 `0x577DB0`；该函数没有直接调用 ComputeGameCRC，也没有在该 store 之后直接读取 House+0x241。对几个直接调用目标的局部代码窗口也没有发现直接的 +0x241 或 ComputeGameCRC 引用。

证据文件：`research/evidence/crc-shadow-single-store-disassembly.json`。

### 4. 第二轮控制流与虚调用核验

对 Map::Reveal 的后半段又做了一轮指令级核验：

- `0x577DD1` 调用的 `0x4068E0` 在本 EXE 中只有一条 `ret`，是空操作。
- 函数体唯一的间接虚调用位于 `0x577DF4`（`call dword ptr [edx+0x4]`），对象来自全局 `0xA8B23C`。默认单例构造路径把 vtable 设为 `0x7EED60`；该表的 `+0x4` 槽指向 `0x5C0E30`，其机器码为 `32 C0 C3`（`xor al,al; ret`），不读写内存，只返回 false。该返回值只影响栈上的局部标志，不会调用 CRC。
- 这只解析了固定 EXE 的默认单例路径；运行时第三方替换全局对象或 vtable 的情况仍未排除。
- 直接调用目标的完整函数体边界也已重新确定：`0x4ADEE0–0x4ADFEF`、`0x578290–0x578342`、`0x4ADCD0–0x4ADED8`、`0x657CE0–0x657D92`、`0x4F42F0–0x4F431E`。这些完整函数体没有直接的 House+0x241 或 ComputeGameCRC 引用；其中仍有 Techno 虚调用和间接回调，所以这不是穷尽的传递调用图证明。
- Visionary 的真实读取指令是 `0x577E0F`，`0x577E09` 只是加载本地 House 指针；写入仍是 `0x577E29`。
- 全局 `ComputeGameCRC` 的两个直接调用点 `0x64731C`、`0x647684` 位于另一个帧/网络更新函数 `0x647260`，不在 Map::Reveal 内。单点抑制让 `House+0x241` 在 Reveal 期间始终保持原值，因此无论 CRC 在 Reveal 内部重入，还是在之后的帧更新中执行，都会读取原值。**“Reveal 返回前不能发生 CRC”只对事后恢复候选是必要条件，不再是单点抑制方案的门槛。**

### 5. x86 单点 trampoline 代码生成探针

新增研究专用的 x86 naked-function 探针，并用 `clang++ -target i686-pc-windows-msvc -fms-extensions` 实际编译成 COFF 对象。当前本地代码生成契约通过：

- 回调前保存 GPR、EFLAGS、x87 和 SSE 状态，回调后完整恢复。
- one-shot 分支恰好跳过原始 6 字节 `MOV [ecx+0x241],bl`，不碰其他指令。
- 两条分支都清除同一个 skip 标志，再恢复原始 EFLAGS。
- 通过 `push 0x577DB6; ret` 精确续接原始下一条指令。
- COFF 重定位中恰好有一个受保护回调引用，以及同一 skip 标志的比较/清除两处引用。

探针和测试分别位于 `source/assist/tests/crc_shadow_single_store_trampoline_probe.cpp` 与 `source/assist/tests/crc_shadow_single_store_trampoline_contract.py`。CRC 专项 CI 已加入该编译检查。

**边界：** 这是独立 x86 代码生成探针；生产 trampoline 已按同一指令序列接入 `crc_shadow_bridge.h`，并有静态源码契约对照。它仍不等于完整 Runtime DLL 的执行，也不证明 Windows SEH、完整 Map::Reveal 或联机行为已经通过。

这比此前仅按静态候选区间推断更强，但仍不是完整调用图证明：Map::Reveal 后续会调用地图/渲染相关函数，其中有虚函数与间接调用；局部窗口扫描不能代替对所有间接目标的穷尽分析。现有 Unicorn fixture 在 `0x577DB6` 后提前返回，因此它没有验证完整原生后续执行。但本项目不需要在工具箱中模拟或维护 Cell/Visionary 状态；验收只需确认单点拦截后从原始下一条指令 `0x577DB6` 继续，游戏原生后半段不被跳过、不被改写，并在真实游戏中正常运行。

## 当前实现：单点抑制 Map::Reveal 的写入

现有 CRC 回调已经能在 0x577DB0 执行原始 MOV 前取得 ECX（House）和保存的 EBX（其中 BL 是将写入的值）。反汇编确认该点正是 Map::Reveal 函数内唯一的 House+0x241 写入。生产代码只在工具调用作用域且 ECX 与事先校验的目标 House 指针、ID、索引完全匹配时跳过这条 MOV；其他 House、自然 Reveal 和身份变化都走原生写入，不做全局拦截。身份异常时 fail-open 执行原生 MOV，并停用本局单点保护。因此不必改写 EBX，也不必重算 CRC：

1. crcMapToolBegin 在同步调用前校验当前世界、目标 House 身份并开启工具揭示作用域。
2. 仅在 0x577DB0 回调中，如果作用域有效，就设置一个一次性 skipStore 标志。
3. 裸函数 trampoline 在原 MOV 位置检查标志；为真则跳过 MOV，为假则执行原 MOV。
4. 检查分支必须用 pushfd／popfd 恢复 EFLAGS；只访问内存标志，不修改任何 GPR。回调原有 pushad／popad 继续保护寄存器。
5. 立刻清除一次性标志。工具调用外的自然 Reveal 不设置标志，原指令照常执行。
6. 如果受保护回调发生可处理异常，AssistMapCrcRevealAbort 只对匹配的目标 House 设置 skipStore，再由 trampoline 消费；未处理异常则由外层 RuntimeScopedCall finally 清理作用域。
7. 手动 Reveal 外层使用 RuntimeScopedCall finally 清除作用域，确保正常返回、SEH unwind 和提前退出都不会把 skipStore 泄漏到下一次自然 Reveal。

这比调用后恢复更强：MapIsClear 在 0x577DB0 之后就保持原值，CRC 即使在 Map::Reveal 内部被嵌套调用，也会直接读到原值；同时不需要对 House 内存做事后写入。

**仍需验证的假设：** 作用域内到达 0x577DB0 的写入确实属于这次工具 Reveal；不存在依赖该字段临时变为 1 的非 CRC 后续读取；其他六个写入点不在这次 Reveal 的调用链中。当前源代码已经采用单点实现，但这些假设未完成完整传递调用图和真实游戏验证前，不得打包发布。

## 为什么不选“只保留一个 CRC 读取 Hook”

单独保留 0x64DCCA 仍需要知道每个 House 的真实原值。没有七个写入观察点时，单纯轮询无法区分“工具把 0 写成 1”与“游戏自然地再次写入 1”。同值写入恰好是当前模型必须捕获的边界。因此仅删七个写入点、留下 CRC 读取点，不是可靠缩减。

事务边界方案不同：它不让工具写入在调用返回后持续存在，因而自然写入不需要镜像模型。

## 当前适用范围

只覆盖工具自己的 RevealMap 命令。当前代码已经在 CRC Shadow 启用条件中排除非零 renderReveal、直接地图模式及 Ares/Phobos；这些限制必须保留。没有把自然 SpySat Reveal、第三方模块、自动全图或其他直接调用 0x577D90 的路径扩张为受支持范围。

如果未来要支持那些入口，必须逐一确认调用者并建立同等事务边界；不能假设所有 Reveal 都经过工具命令。

## 关键风险与尚未证明的条件

1. **非 CRC 读取者是否依赖临时值**：单点抑制让 +0x241 从写入点开始就保持原值，因此 CRC 在函数内重入或函数返回后执行都安全；不再需要证明“Reveal 返回前没有 CRC”。仍需排除原生后续逻辑通过间接别名读取 MapIsClear 并依赖它临时变为 1。已知直接读取候选均是 CRC/诊断，Map::Reveal 和已检查的直接 helper 函数体也没有直接读取；间接读取仍未穷尽。
2. **函数内是否有其他 +0x241 写入**：当前静态扫描只看到 0x577DB0 位于函数范围内；仍需完整执行时对所有 House 的 +0x241 写入做 watch trace。
3. **其他读取者的访问形式**：现有线性扫描未发现非 CRC 直接读取，但它明确不覆盖间接别名、宽访问、批量初始化、载入/还原及运行时扩展。需要确认没有依赖 MapIsClear 在本次工具 Reveal 中短暂变为 1 的非 CRC 逻辑；若存在，单点抑制同样不适用。
4. **Reveal 的其他同步副作用**：Cell、Visionary 和特殊格条件 RNG 仍按原生路径执行；工具箱不复制、不镜像、不恢复这些状态。测试不需要重写或模拟它们，但真实游戏验收必须确认跳过单条 MapIsClear store 后，控制流仍从 0x577DB6 进入原生后续代码并正常返回。
5. **异常与作用域清理**：合成测试验证了状态模型，但仍需在实际 Windows Runtime 的 SEH 边界与真实游戏调用中验证。异常时必须确保一次性 skip 标志只影响目标 House 的该次写入，外层 finally 清理作用域，不能把跳过状态泄漏到后续自然 Reveal。



## Hook 选择原则与本轮决策

本轮不把“零 Hook”作为验收目标。若一个原生规则/事件入口或更高层的规则类 Hook 能以更少状态、更少分支、更清晰的所有权保持相同语义，就优先采用它；如果它做不到，保留一个精确、默认关闭、范围明确的 Hook 比引入事后内存回写、镜像状态机或额外线程更简单可靠。Direct Placement、跨 House Network Sender 等已审计功能也不因追求零补丁而强行删除其必要入口。

对 CRC Shadow 当前的候选排序：

1. **优先验证单点写入抑制**：一个 0x577DB0 指令点、一个同步工具作用域、精确 House 身份比较；自然游戏调用完全原生。若确认 trampoline 正确续接且实际游戏中原生 Reveal 正常，这是当前最小且最直观的方案。
2. **不优先零补丁事后恢复**：保存／调用／比较恢复虽能在合成模型中成立，但需要额外读写 House 内存、异常清理和失败重试，且恢复发生在 Map::Reveal 返回后；如果函数内部发生 CRC 或依赖该字段，时序更复杂。
3. **回退策略**：八点旧实现不与单点实现并存，保留在 Git 历史中；若单点验收失败，回退该实现提交或保持功能关闭，不增加运行时双轨。

## 本轮验证

当前生产单点策略与验证资产：

- **零补丁事务模型**：保留为历史比较，不参与生产实现。它覆盖原值 0/1、自然同值写入、身份复用、未知值冲突、恢复失败等；不再是当前方案的运行时依赖。
- **生产单点策略单元测试**：直接编译 `crc_shadow_single_store_policy.h`，覆盖工具作用域、精确 House 指针/ID、其他 House 原生放行、同一作用域只抑制第一次匹配写入、身份/epoch 变化、SEH abort 和作用域结束清理。
- **生产源码静态契约**：检查 `crc_shadow_bridge.h` 只选择 `Sites[6]`、不存在 `crcMapModel` / `restoreData()`、工具调用使用 `RuntimeScopedCall`，并检查 Guarded callback 的异常路径不写 Shared 内存。
- **x86 trampoline 代码生成契约**：clang++ 交叉编译 i686 Windows COFF 对象，检查 GPR/EFLAGS/x87/SSE 保存恢复、只跳过原始 MOV、一次性标志清理、`0x577DB6` 精确续接及重定位；生产源码另有静态契约对照。
- **Windows 配对构建门槛**：新增 workflow，编译 x64 Host + x86 Runtime/Broker，并运行 x86 `RuntimeScopedCall` finally 与单点策略测试。编译结果和步骤状态以 GitHub Actions 实际结果为准。
- **真实 PE Runtime 夹具**：`crc_shadow_runtime.py` 已改为只验证一个写入点、工具作用域抑制、自然 Reveal 放行、CRC 输入、身份变化 fail-open 和卸载不回写；它在 `0x577DB6` 处停止，不模拟 Cell/Visionary 后半段。需要配对构建产物与原始游戏 PE 才能执行，不能替代真实游戏验收。

Ubuntu CRC 专项 CI 已通过；Windows 完整配对构建、SEH 运行验证、完整原生 Map::Reveal 和双机联机验收仍待完成。

交叉参考：固定 YRpp HouseClass 将 Visionary 与 MapIsClear 相邻声明（[HouseClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/HouseClass.h#L869-L872)）；MapClass 将 Reveal 声明为 0x577D90（[MapClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/MapClass.h#L477-L479)）；Phobos SyncLogging 将每个 House 的 MapIsClear 加入 CurrentFrameCRC（[SyncLogging.cpp](https://github.com/Phobos-developers/Phobos/blob/develop/src/Misc/SyncLogging.cpp#L653)）。这些只佐证字段布局和摘要用途，不证明完整可执行文件不存在其他读取者。

## 下一步验收门槛

单点实现已接入当前源码，但在以下项目通过前不发布 Update/Release：

1. 在固定 PE/受控运行环境中验证单点 trampoline 跳过 0x577DB0 的 MOV 后，准确回到原始下一条指令 0x577DB6；确认原生后续代码继续执行并正常返回。无需采集、记录或模拟 Cell/Visionary 状态。
2. 验证回调只对工具命令指定的 House 设置一次性 skip；其他 House、自然 Reveal 和身份变化均执行原生写入。确认 Map::Reveal 后续没有依赖 MapIsClear 临时变为 1 的非 CRC 读取者；不再把“没有嵌套 CRC”作为门槛，因为单点抑制在 CRC 执行前就保持原值。
3. 补充 MapIsClear 读取者审计，特别是宽访问、间接地址和扩展模块；不扩大到 Cell/Visionary 状态审计。
4. 在实际 Runtime 的游戏线程 SEH/finally 边界中验证正常返回、异常退出、作用域清理、House 身份变化和嵌套 Reveal；不引入事后内存恢复流程。
5. 完成新旧方案 CRC 输入逐帧对照，并在真实游戏中确认完整原生 Map::Reveal 从 0x577DB6 继续执行、正常返回，随后完成双机联机验收。单点实现已彻底移除 `restoreData()` 和事后 House 数据回写；卸载只撤销唯一代码补丁并清理作用域。原八点实现只保留在 Git 历史中，不在新运行时并存。

如果任一门槛不通过，保持 CRC Shadow 关闭并回退单点实现，不强行发布。
