# pre.4 首次命令执行的大栈临时与导弹输入原位重置

## 新日志定位

用户回传 `Runtime(7).log`，SHA-256 `c39ad8c007195b47d3224f4d60f5fde133e13a1768a818702cdf29ded1441f43`。本次只分析 v1.0.41-pre.4 新进程段，不混用前段 pre.3 的签名拒绝。

PID 37152 与 28568 均已安装 Hook、连接回调和 IPC，分别在 dispatch 6 与 5 后记录 `0xC00000FD`（栈溢出），fault boundary=1、fault command=0。两者故障地址同为 `0x5320A672`，Runtime 加载基址 `0x53150000`，RVA=`0xBA672`；随后功能桥进入 fault 状态。最后已发布的 match=0／commandStatus=5 是此前世界过渡快照，不能把它当作故障瞬间仍停在世界检查或继续被签名阻断的证明。

本轮调查前查询研究索引和持久 SQLite v125；没有既有 C00000FD/栈溢出记录。pre.4 产物 `GameRuntime.dll` SHA-256 为 `349e61129d23130bfe0e7fef9551b335c9896cb2e6e290a36ebe08dff8334a9e`，源码为 `4baa78ba34032f1b69096d37db8a0a6d2714d54c`。

## 源码、真实产物与编译器证据

该 Runtime 的故障 RVA 位于栈页触探测函数。独立产物审计发现唯一超过 1 MiB 的调用方：函数 RVA=`0x418D0`，序言在 `0x418DE` 调用栈探测，申请 `0x1044A0`（1,066,144）字节；函数体清理 `0x104418` 字节的局部对象，先调用一个维护函数，再读取 Shared 中的导弹启用字段。

源码 `missileControlTick` 对应流程是先 `pruneMissileSends(now)`，再判断 `sh->missileEnabled`，随后才在采样路径执行 `auto& i=missileInput; i={};`。`missileInput` 本身是静态对象，但赋值右侧的完整 aggregate 会由 Zig 0.13、Windows x86、`-O2` 生成为一个栈临时，然后复制至静态对象。`sizeof(missilecompute::Input)=0x104418`（1,066,008）与产物清理长度精确一致。

此大栈帧在函数序言申请，早于关闭功能时的 return。因此即使导弹功能没有开启，`tick` 正常调用该维护函数也可能先撑爆游戏线程栈。没有证据表明这是 Hook 递归；不能把修复替换成单纯调整早退分支或增大工具箱自己的线程栈。

历史扩容提交 `97d4f476f99f8a056978a83f31af8e33b8282064` 将导弹输入的 MaxSources/MaxTargets 从 64/64 提高到 256/256，边矩阵扩大 16 倍。Input 从约 `0x11118`（69,912）增到 `0x104418`，原赋值形态保持不变。旧平台启动门槛先前会阻断到达后续功能维护；pre.4 的依赖隔离使这个独立的大栈缺陷得到实际触发机会。

用户新采集的主 PE（SHA-256 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`）中，OptionalHeader 默认 StackReserve=`0x100000`、StackCommit=`0x1000`，这八字节 valid 与 stable 掩码均有效。它是 PE 默认值，采集没有线程 TEB 或完整调用栈，不能冒充对故障线程实际 reserve／剩余栈的直接测量；但单个 1,066,144 字节栈帧已超过该默认保留量，与栈探测时的 C00000FD 机制一致。

用户随后明确补充“别的平台都 OK”。本轮没有其他平台同版本 Runtime 的实际线程栈、剩余空间或完整调用路径对照，不能先将平台差异归因于栈大小。确认的大临时缺陷应修复；修复后的该平台功能恢复仍需用户实机验证。

## 最窄修复

`missile_compute.h` 增加 `reset(Input&)`，通过 placement new 在原对象存储直接值初始化；`missile_bridge.h` 唯一一处整体 `i={}` 替换为该 helper。它不进行堆分配，不构造完整 Input 栈临时，不缩小容量，不修改协议或调度入口。

不能直接 `memset` 整个 Input 当作等价替换：Input.frame 默认为 -1；每个 Source 的 pendingMission、advanceFrames 为 -1；256×256 个 Edge 的 close 默认为 1。原位值初始化保留所有成员默认值，包括未来向这些类型增加的默认成员。

同类型调用点检查没有发现第二处游戏线程 Input aggregate 赋值。FrameHistory 的固定输入副本通过 unique_ptr 分配；World、cooperative FrameInput 使用静态／共享存储。指定 reset 链的 Planner、装载与协同 helper 没有 MiB 级临时，FrameArbiter/LeaseArbiter 的 clear 只更新计数。较小的其他类型临时本轮不扩大修改。

## 验证及边界

`assist/tests/missile_input_reset.py` 对实际 helper 运行语义验证，并用 Zig 0.13 真实 Windows x86 `-O2` 生成新旧赋值形态的汇编。语义检查先污染全部成员，再逐字段比较 fresh 默认对象，覆盖全部 source/target/edge 槽、末尾索引、重复 reset 和全部非零默认值，不比较 padding。

本机语义测试 1,993,806 项通过，含 1,599,012 次逐字段比较、六轮重置验证。Zig 0.13 Windows x86 `-O2` 旧 wrapper 明确申请 1,066,008 字节并调用栈探测和 memcpy；生产 helper wrapper 的显式栈申请为 0，仅有两次对目标存储的 memset，没有栈探测、memcpy 或隐藏的 constructor/reset 调用。两种初始化都保留后续非零默认值写入。Linux g++ 会把旧赋值优化成很小的栈帧，因而仅跑 Linux 算法测试不足以发现本缺陷。脱敏报告为 `evidence/missile-input-stack-reset-20261104.json`。最终真实 Runtime 产物仍需重新检查对应函数的栈序言。

pre.5 编译产物 `GameRuntime.dll` SHA-256 为 `9d4233bbb3586a11e839c3e65faa32b236cee9cb75faf126f75aaad8021e82ae`，源码提交 `ff657f291dbb60f48f357364d4b3fe887de1b231`。成品函数仍位于 RVA `0x418D0`，实际序言已改为仅申请 `0xC98`（3,224）字节，不再调用入口的大栈探测，原关闭检查及返回分支保留。全部两类栈探测的 29 个直接调用方已审计，含 28 个固定申请与一个 CRT 可变申请；最大固定申请为 71,772 字节，旧 `0x1044A0` 帧已消失。报告见 `evidence/missile-input-product-stack-audit-pre5.json`。

这些验证证明重置语义、特定编译器代码生成与该成品的固定栈帧变化，不等于新版本已在用户平台、完整游戏或联机中验收。故障记录本身没有完整调用栈，因此也不能据此声称已排除所有其他潜在大栈调用。
