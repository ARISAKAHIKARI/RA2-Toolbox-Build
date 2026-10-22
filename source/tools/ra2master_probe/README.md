# RA2Master 实机采集工具 0.1.0

用于研究“控制全部”和“卖别人建筑”的实际修改位置。独立诊断工具，不改变 Toolbox 版本，不需要 Python。可执行文件面向 64 位 Windows 10/11，读取 32 位游戏/修改大师。

## 使用

1. 解压压缩包。在 **gamemd.exe** 中进入一局单机游戏，退出 Toolbox，先不要打开修改大师。不要只启动 gamemd-spawn.exe，也不用为了采集改游戏文件名。
2. 双击 `RA2MasterProbe-0.1.0.exe`，模式选择按回车。若提示读取权限不足且游戏以管理员运行，再以管理员运行采集工具。
3. 按窗口提示依次采集：修改大师未启动 → 修改大师已打开但控制全部关闭 → 控制全部开启 → 再关闭。每步回到工具窗口按回车。建议暂停游戏，其余开关保持关闭。
4. 如果有单独的“卖别人建筑”开关，可以选择继续做关闭/开启/关闭采集；没有则回车跳过。此轮不要求切换玩家。
5. 推荐采集修改大师主模块：输入工具列出的修改大师 PID，确认名称后输入 `y`。这一项不是选择游戏进程。若列表没有显示，可从任务管理器“详细信息”查询修改大师 PID。
6. 将 EXE 旁的整个 `RA2Master-Capture-日期-时间-PID` 文件夹压缩后回传。即使某一步报错，也可回传已有结果。工具不自动联网或上传。

只想获取修改大师内存映像时，启动后输入 `2`，不需要游戏正在运行。采集结果包含程序代码/内存数据，直接回传用于分析，不要公开发布。

## 采集内容

- 精确识别 `gamemd.exe`。多实例时要求选择；仅发现 spawn 会明确报错，不静默替代。
- 游戏文件 SHA-256、实际加载基址、PE 节表、各阶段模块清单和可读字节数。不记录完整安装路径。
- 启动修改大师、开启控制、关闭控制时，主模块可执行节的两两字节差分；最多输出 30,000 个差异段，超限计数明确标记。
- 各阶段间隔 150 ms 的两次读取：只有两个样本均可读且字节相同，才视为阶段内稳定。全节扫描稳定的关闭→开启→关闭恢复字节，列为候选相关变化。
- 原始状态和功能开启时，游戏私有可执行内存的补充采样：每块最多 1 MiB，每阶段最多 8 MiB，超限标记截断；不采普通堆、栈。
- 可选修改大师主模块内存映像及逐字节可读标记。`.mem` 是 RVA 布局，不是可直接运行的 EXE，不能据此声称 VMProtect 虚拟化已移除。

## 边界

只以 `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ` 打开进程。不暂停/调试目标、不注入、不修改内存、不调用目标内部函数。

读操作不是原子快照；稳定性过滤不能证明因果。数据恢复可能是正常游戏变化；没有差异也不能排除临时写入、堆对象写入、驱动路径或其他模块。可执行节比对包含不稳定字节，供人工复核；跨三阶段候选才应用稳定性过滤。工具不记录 API 写入者或网络数据，不能证明联机同步。

本轮验证：差分算法本机测试、Windows 目标编译、PE 导入和版本检查。尚未在真实 Windows 的游戏/修改大师上运行。

## 构建

Zig 0.13.0：`ZIG=/path/to/zig bash build.sh OUTPUT_DIRECTORY`。

差分算法测试：`g++ -std=c++17 -O2 test_diff.cpp -o test_diff && ./test_diff`。

Win32 API 依据：[ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory)、[VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex)、[CreateToolhelp32Snapshot](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot)。
