# 随机名称平台与后台子渲染窗口发现

## 问题与历史核对

用户报告之前会随机改变名称的平台再次无法捕获，询问旧发现逻辑是否删除。调查前已核对研究索引及持久 SQLite v123 中 `platform5-134`、`platform5-live-signature-rejection`、`platform5-135-mrga-beacon-startup` 和 Hotfix11 进程识别记录。

旧发现逻辑没有删除。`process_discovery.h` 在引入随机名称发现的 `f5a1c92cd06e70d034e298772cc7a59a3eb64089`、当前 main `48d283ae3f854d94ae2568136e8c6e5ef62909e1` 与修复前 pre.3 `dd5a26b96889208decf8ff30744079f90423b583` 的 blob 均为 `da4c12d6b36f262de8a99fe2b7cf39dbb4f4e54c`。Web 控制台重构 `c84ea74492590cabdb0c58f368460cc837b54fed` 没有替换 Assist 的 `attach()`、`discovery::scan()` 或窗口恢复入口。

历史 MRGA 启动兼容也仍保留：`startup_signature_compat.h` 从 `ff5a76ada06efdd764d9c3ef13cd452b4d69ef14` 到上述 main/pre.3 的 blob 均为 `5bb7fa384a63714e2ccb0aa960ad40b3b8395759`。历史 `status=-1` 属于启动签名校验拒绝，当时 Hook 已有回调，日志实际 EXE 为 `gamemd-spawn.exe`，随机字符属于窗口标题/类名；不能把那次原因直接当作本轮原因。当前失败诊断还需检查 `commandStatus=3`。

## 已复现的发现缺口与修复

旧流程枚举所有顶层 HWND，但只枚举**当前前台窗口**的子 HWND。任意非十六进制改名的游戏若作为另一个平台进程的子渲染窗口运行，在平台转到后台、工具箱取得焦点后，可以同时漏掉候选 PID 和子 HWND。十六进制名称即使由快照识别了 PID，仍可能只得到 windowless 候选，无法建立要求 HWND/TID 的连接。

`process_discovery.h` 改为一次 `EnumWindows`，对每个顶层窗口调用一次 `EnumChildWindows`。后者本身递归访问全部后代，生产回调不会逐个子 HWND 再递归枚举。每个窗口先执行原有身份准入，再执行原有窗口 PID/尺寸/样式与打分规则；未改变固定名称兼容、首选 PID、歧义处理或窗口排序规则。

未知进程的精确身份核验每 PID 每次 scan 至多一次；快照中已尝试的十六进制名称负例也加入已检查集合，避免在其多个子窗口重复核验。重复文件读取仍由原有三秒 PID/创建时间/路径缓存控制，文件 I/O 仍在缓存锁外。scan 仍是同步操作，首次遇到未知 PID 可能读取磁盘文件，不能声称完全无阻塞。没有新增每帧调用或提高发现频率。

## 支持映像、地址和边界

随机名称准入仍只继承此前用户样本的发现指纹：4,813,161 字节，SHA-256 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`；PE32 x86，基址 `0x400000`、入口 RVA `0x3CD80F`、映像大小 `0x794000`，磁盘片段偏移 `0x2339B0/16`、`0x237E37/20`、`0x3B8A00/16`、`0x42D028/28`。本轮没有取得或验证新的平台映像，未知更新版本失配仍是未决可能；未根据随机名称、窗口标题或模块品牌直接放行。

未更改游戏函数 ABI、字段偏移或补丁授权。权威游戏 HWND 仍从 `Game::hWnd=0xB73550` 读取固定 32 位值，再校验真实 HWND 所属 PID/TID；Runtime 启动与每项功能的独立兼容检查照旧。发现阶段仅查询进程身份和读取磁盘文件，不新增游戏 VM_READ、注入、权限提升或游戏内存写入。

## 验证与未决

- `source/tests/random_process_discovery.cpp`：旧生产头在后台非十六进制子渲染断言失败；修复后通过。新增前台已切到工具箱、后台任意名称、后台十六进制名称的子 HWND、快照失败、嵌套后代、同 PID 多 HWND 一次精确核验、十六进制负例不重复核验及无关子进程拒绝。原有损坏/截断、缓存/PID 复用、窗口缺失、首选 PID、歧义及权限拒绝保持通过。
- `source/tests/random_process_discovery.py` 生成仅含公开发现字段的稀疏非可执行 fixture；不读取、打包或发布私有游戏 EXE。Linux 命令：`python3 source/tests/random_process_discovery.py --cxx g++ --out diagnostics/random-process-discovery`。
- `source/tests/process_identity_lock.cpp`：本地重跑通过，缓存命中不被另一个 PID 的慢文件读取持锁阻塞。
- `source/tests/process_discovery_windows.cpp`：新增自有 spawn 测试进程，将其唯一渲染 HWND 放入另一个自有隐藏顶层宿主下的嵌套容器，验证后台子窗口发现及权威 HWND 的 PID 校验。真实 Windows API 执行结果由同提交 CI 记录，本地 Linux 不能执行此夹具。

结论限于旧逻辑保留事实和可重复的后台子窗口漏检缺口。没有该平台本轮的运行日志或实际映像，尚不能认定用户当前失败必定由此触发，也没有真实游戏画面、Hook 回调或联机实测。
