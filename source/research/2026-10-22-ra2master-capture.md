# 修改大师模拟探针与实机采集工具 0.1.0

## 问题与证据边界

继续研究“控制全部 / 卖别人建筑”的实际实现。用户补充：修改大师应该只识别 `gamemd.exe`，不识别 spawn；若模拟分析受阻，可交付实机分析工具。进程名限制目前是用户提供的线索，尚未从修改大师业务代码核实。不能把未识别到游戏解释为命令被游戏拒绝。

研究前读取 INDEX、1.0.32 / 直接执行笔记与持久 SQLite。此前样本信息见 [1.0.32 笔记](2026-10-22-player-132.md)。本轮没有修改产品逻辑、恢复切换玩家或发布新 Toolbox 版本。

## 本轮模拟执行

- 样本：旧分发页标为 2.10.3 的 EXE，SHA-256 `779b70da60e9e78e9ee18a60b8b3692b0b184faedd477a1943a72e0ea7d59aac`。资源版本存在前轮记录的不一致。
- 模拟入口 VA `0xD4B0C6`。运行环境 Unicorn 2.1.4、Capstone 5.0.6、`x86rc/vmp-lazy-unpack` main 树/提交标识 `66f64f6f83e3104735dbfffbd18bfa768ecd4346`。
- 先因导出目录缺少 `winspool.drv` 失败。仅为模拟装载，按样本导入表补该模块的合成导出编号；不把这些编号当作真实 Windows 导出序号。
- 随后进入壳执行，在 `CheckRemoteDebuggerPresent` 的未实现 API 处停止。补充“合成进程没有调试器”的标准返回模型（向输出 BOOL 写 0，函数返回 1），未执行任何主机 Windows 调用。
- 最后一次记录约 19,959,382 个工具计数的基本块，停在 `kernel32.dll!CreateFileW` 未实现。此前调用 `GetModuleFileNameW(0x400000, buffer, 0x104)`，随后的 CreateFileW 使用同一 buffer、GENERIC_READ、共享读写、OPEN_EXISTING、普通文件属性：支持“壳在尝试读取自身文件”这一推断。
- 此时 `written_destination_pages=[]`、`analysis_pe=null`。没有生成还原后的业务 PE，没有追到控制全部函数或对游戏的写入。
- 合成导入地址 `0x708409E0` / `0x70840ED0` 属于模拟器，不是修改大师或游戏的真实系统 DLL 地址；不能用于实机断点。

复现脚本与摘要位于 `verification/ra2master-probe/emulate_probe.py`、`emulation-stop.json`，环境依赖外部工具与私有下载样本。此脚本只向模拟器提供标准 API 模型；最后一步未成功脱壳。外部项目：https://github.com/x86rc/vmp-lazy-unpack 。

## 实机工具

源码：`tools/ra2master_probe/`；工具版本 **0.1.0.0**。编译为 Windows x64 控制台 EXE，目标进程要求 PE32。

1. 精确选 `gamemd.exe`，多实例要求选择；不自动改选 `gamemd-spawn.exe`。
2. 仅以 `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ` 打开选定目标。
3. 四阶段：修改大师未启动、打开但控制全部关闭、控制全部开启、再次关闭。可选出售功能独立三阶段。
4. 主模块按页读取，跳过 guard/noaccess，保存逐字节可读性；每阶段两次读取间隔 150 ms。两两比较所有可执行节；三阶段候选要求三次均稳定、off != on 且 off == off-again。
5. 报告实际基址、RVA、VA、前后字节、上下文及有效性，不硬编码已知游戏版本地址。可执行私有页每块最多 1 MiB、每阶段最多 8 MiB，明确截断。
6. 可选保存用户明确选择的修改大师主模块 `.mem` 与 `.valid`。这是 RVA 布局内存映像，不是完整脱壳/修复导入后的可执行文件。也可选择独立采集模式，不依赖游戏正在运行。

仅观察，不注入、不挂调试器、不暂停目标、不修改内存、不调用内部发令函数、不自动联网上传。原始采集不进公共仓库；未来只发布必要的符号化结论和有限字节证据。

## 验证与限制

- `test_diff.cpp` 本机执行通过：恢复变化筛选、不可读字节排除、不稳定字节排除、范围边界、64 字节分段。
- Zig 0.13 Windows x64 编译通过，PE 版本 0.1.0.0；导入检查包含 ReadProcessMemory / VirtualQueryEx / OpenProcess，未发现写入、远程分配、远程线程、调试附加等接口。结果见 `verification/ra2master-probe/validation.json`。
- 没有真实 Windows 的采集结果；未证明 UI 完整可用、修改大师功能有效或联机同步。
- 分阶段读取不是原子快照，稳定/可恢复仅表示相关性；不会自动认定某个字节就是权限判断。未比较普通堆对象，也可能漏瞬时写入或其他模块内补丁。若三阶段无差异，不能据此认定修改大师没有写游戏。
- 下一步用回传的游戏 SHA、实际补丁字节和 trainer 映像定位选择判定/事件构造，再比较 HouseIndex 是否保持本地玩家。此前“可能只放开选取、仍用本地 HouseIndex 发令”的猜测仍未证实。
