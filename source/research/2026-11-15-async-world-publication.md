# 封口样本后台发布与缓冲所有权

问题：把已经采集完成的整份样本发布到共享映射时，不应继续占用游戏线程；采样身份、完整性、任务和退出安全必须保持。基线为 `68281b25aad41f31defef154faf9124d05e8c10d`（pre.9 后的缺页需求优先提交）。本次为传输/生命周期重构，无新增游戏函数、地址或字段。历史支持的 EXE SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，本轮没有该文件，未执行游戏。

## 分工和成本

游戏线程仍在原有稳定边界执行 CaptureGraph、采集后世界/House/帧复核和 seal；它将页缓冲的所有权交给独立 Win32 线程，不复制页面。后台线程负责 DataView 映射创建/增长、全部 Page 复制和发布通知；只读取自己持有的封口页，不执行游戏函数、不调用 Live reader，也不持有 game-entry 锁。

PublicationPool 最多三份页缓冲：一份后台持有、一份待发布、一份用于下一次采集。pending 是最新状态槽，更新可以合并；没有把命令、Ctrl+D 工作任务或执行回执放进这个覆盖槽。缓冲几何增长后复用，内存上界增加；初次捕获/较大样本的缓冲扩容和实际捕获成本仍在游戏线程。本次没有调整同帧补采策略，也没有改变 Host 导入执行上下文。

## 退役、并发与卸载

控制线程的 Server 管观察、订阅和退役；后台 Server 使用 PublicationOnly 角色，只拥有自己打开的映射/事件句柄，不能代写观察或清空控制端状态。epoch 取消待发/进行中旧任务，producer generation 区分世界退役前后；接收端即使读到新鲜的 active 槽，也必须通过独立 observation 的身份和 tick 校验。测试强制在最后提交检查时退役，允许模拟出的旧 active 物理写入，但客户端拒绝其采纳。tick=0 是退役标识，快速重复读取也不能绕过。

不能仅缓存“serial 已成功发布”：通道心跳到期后 active 可能被清除，而暂停的游戏帧不变。前台改为只读核对通道中的 descriptor；若它已消失，重新排队原封口样本，保留 serial/tick/所有原始字节，不再 Live 读取。失败重试仍按16ms合并。

Worker 启动持有所属 DLL 的独立引用，在释放页租约和自己的映射后使用 FreeLibraryAndExitThread 退出；前台 stop 只以0ms轮询，退出未完成时不确认 Runtime 卸载。意外退出的线程会被非阻塞回收，下一次发布可重启。模块引用说明参见 Microsoft GetModuleHandleEx/FreeLibraryAndExitThread 文档。

## 协议和验证

TransportVersion=3（WorldFrame/WorldData v003，通知 v002）；Assist=109 强制成套 Host/Runtime。Shared 仍为23162720字节，Runtime=4，game-entry=3。目标版本 1.0.51-pre.10。

- `world_frame_publication.cpp`：后台固定持有旧样本时连续采集1万次，旧字节和时间戳不变，至多三份地址；并发生产/消费1万次，最新 pending、取消和租约复用。
- `world_frame_authority.cpp`：实际 Client/Server，受控 Win32 I/O；提交/退役交错、旧 generation 拒收、后台断开不清控制观察、tick零快速路径。
- `world_frame_async_dll.cpp` / `world_frame_async_host.cpp`：真实 Windows DLL/线程/命名映射，执行实际 world_frame_runtime.h 和原 CaptureGraph；原生 ReadProcessMemory 只读取测试自有的合成内存。阻塞后台首个映射时前台仍完成1000次采集；读回帧/单位字段核对，检查暂停同帧通道恢复、停止轮询、重启、Host卸载引用后的模块存活与最终自动卸载。Live 回调断言只能由测试前台调用。x86/x64各三次新进程。旧版同步 fixture 单独保留为 transport/采样回归，并未冒充异步测试。
- `world_frame_async_portable.py`：UBSan、原有8项保留/失败/需求回归、池和退役验证；`world_frame_core_test.py` 76122项；`tactical_frame_sampling.py --negative-control` 检查任务/身份保留。
- `.github/workflows/async-world-publication.yml`：Windows 成套编译、实际 PE、导出/ABI/版本/依赖检查和测试日志。各次运行以 Actions 的 source SHA 和结果为准；初轮975744b的六次Windows worker测试与完整编译通过，portable测试因错误地在最后一个映射句柄关闭后首次导入而失败，测试已改为先导入再验证持有令牌与断开语义。

受控测试不证明游戏内实际帧耗时或联机改善幅度；没有将整个采样图、最终原生调用或原生绘制搬到后台，也没有修改小地图接入路线。
