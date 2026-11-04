# 指定所属方自毁按钮（1.0.39-pre.5）

## 问题与证据边界

用户要求把前轮研究的 Destruct 加为可手动触发的按钮，并确认联机如何发送。本轮先检索 INDEX、2026-10-26-enemy-sell-pre4.md 及持久 SQLite 笔记的 2026-10-26-enemy-sell-pre4 条目。支持的原始 EXE SHA-256 为 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。

本版增加工具箱提交功能，并发现原版联网打包会覆盖事件House；因此联机仅开放本方自毁，拒绝其他所属方。没有真实双机或具体联机平台“观战”按钮的动态证据。原版接收器、受控发送分支和最终 DLL 入队测试必须与真实网络传播、完整 House AI / DestroyAll 销毁流程分开理解。

## 原版 Destruct 确实进入 OutList

事件类型 `Destruct=0x08`，不是 MegaMission，也不是 Exit。无额外负载的原版构造器 `4C66C0`，x86 thiscall，ECX=Event，栈参数 `(player,eventKind)`；构造器只填事件，不自行入队。事件头 `+2` 为有符号 House 索引。

`destruct_outlist_probe.py` 受控执行三个原版基本块：`48CA34..48CAA1`、`48CAB2..48CB22`、`48CBAE..48CC1E`。这些代码从当前 House 取 `+30` 索引，调用原版构造器，再把111字节事件槽写入 OutList（不是固定线上报文尺寸）。OutList count 位于 `A802C8`，tail 位于 `A802D0`，数据起点 `A802D4`，容量128；时间戳使用原版计时回调 `[7E1530]`，写入 `A83A54+tail*4`。队列满不追加，成功后 tail 按127掩码回绕，count加1。

18项覆盖三个分支 × 两个合法House索引 × 空队列/127尾端回绕/满128。测试运行原版构造器与追加指令，验证类型、House、帧、时间戳、计数/尾指针，并验证提交阶段没有修改House销毁字段。它证明原版发送块使用 OutList，未执行按钮选择、整个对话框或平台观战UI。

## 接收语义

原版执行器 `4C6CB0` 读取事件House并查House数组；Destruct分支 `4C7BDF` 以该House为ECX调用 `4FC980`。后者对已结束/胜利/失败状态不重复设置，否则写 `House+1F6=1`，`+298=当前帧`，`+2A0=0`。后续 House AI `4F87C2..4F87FA` 的静态路径到 `DestroyAll=4FC6D0`。前轮20项受控接收检查证明目标来自事件House，不取本机CurrentPlayer；完整对象销毁、结算及观战切换仍未动态验证。

按钮的成功反馈只能表示本端已入队。原版执行分支确实能理解其他所属方，但本轮进一步证实其前面的发送过程会覆盖这个标签；不能把手工直接调用接收器的foreignHouse结果推到真实发包。单机AI还可能因DoList原版House遍历条件而未执行。

## 新发现：原版联网发送覆盖所属方

原版发送主函数 `649CA0` 的协议字段位于 `A8B24C`。protocol=0 分支在 `649E0F..649E1E` 把 OutList 事件House强制写为 CurrentHouse `A83D4C -> +30`，随后复制本机DoList并序列化。

protocol=1/2 使用压缩打包器 `64B660`；`64B9B9` 取CurrentHouse，`64B9D1` 取House+30，`64B9D4` 写回当前OutList槽 `A802D6 + head*111`。同一阶段修改帧号，`64BA19..64BA3C` 将改写后的事件复制本机DoList。普通事件线上编码为type与payload，原来的每事件House头并不逐条原样发送。批次FrameInfo头在 `649D40..649D49` 填入本方House。

接收解包函数 `64BDD0` 在 `64BE32..64BE43` 从FrameInfo取得House和Frame，后续事件填payload，再由 `64C27F..64C2F0` 写入DoList。因此，这里的限制不仅是“是否有拒绝性权限检查”，还包括确定的字段改写/还原。

结论：支持样本的三个原版协议分支中，单靠 OutList 头指定其他House不能实现其他所属方Destruct；它会变成本方Destruct，并先进入本机DoList，存在误自毁。对手Sell同样会失去foreignHouse标签，之后因对象Owner不匹配而被已核实的Sell接收门槛拒绝。此结论修正前轮仅覆盖构造/入队/直接执行器的可行性假设。其他平台改版是否改变发送器，尚无证据，本版不尝试跳过或改写发送器。

`network_house_serialization_probe.py` 受控运行完整原版发送主函数649CA0、内部压缩打包64B660及压缩解包64BDD0，12项覆盖protocol0/1/2 × CurrentHouse0/1 × 初始事件House0/1，全部通过。协议0输出222字节（111字节FrameInfo和111字节Destruct），协议1/2输出15字节（14字节FrameInfo和1字节Destruct）。所有情况下原OutList事件与本机DoList均被改为本方；压缩流解包后House也来自本方FrameInfo。

ABI：649CA0为fastcall，ECX=受控transport对象，EDX=输出缓冲，栈参数(容量4096,帧延迟5,FrameInfo+0B输入1)，ret12；64BDD0为fastcall，ECX=缓冲，EDX=长度，无栈参数。日志4068E0、时钟回调7E1530、transport虚表+8(Send)/+4(Flush)为受控替身；没有发送真实网络数据或执行Destruct效果。测试依赖同目录destruct_receiver_probe.py的初始化和私有捕获夹具，不提交EXE或rawdump。

为避免选对手却毁己方，本版Runtime在命令入口及最终授权均拒绝联机目标非lastHouse，返回-42；Host同时禁用该选择的提交并给出原版发送覆盖所属方说明。联机本方仍需明确选择，单机保留已验证的指定House DoList路径。

## 本版实现与验证

UI位于“高级与实验性设置”，复用所属方列表862，新按钮9185“自毁所选所属方（实验性）”与回执区9186。列表显示实际House索引、名称、人类/AI；初始、换进程或换局不默认选择。列表读取采用稳定seq快照，点击需要500ms内的新鲜列表；同一进程/对局/指针/ID/索引才保留此前的明确选择。普通命令通道忙、待回执或Host尚未消费回执时禁用；工具箱处于前台可按既有commandFocus路径操作。

新增 `DestructPlayer=32`，arg=House指针，arg2=House UniqueID，requestToken=实际House索引，沿用requestMatch/requestTick/request/ack。协议升级77，Shared字段及大小不变（4165672字节）；需要成对替换EXE/DLL。联机选择其他所属方时禁用按钮并说明原因；按钮没有持久开启状态、快捷键、自动触发或自动重试。Host专门排除通用延后命令队列，断线、忙碌或对局变化时本次拒绝，不留待重新连接后执行。未开放切换操控方，`SwitchAvailable=false`。

Runtime在统一游戏线程dispatcher内处理单次请求，经独立DestructPlayerTest授权后调用 `submitConstructed`。仅放行NoOptions中的Destruct这一新增类型；Exit、Options等仍保持ProbeOnly，不开放整类事件。目标必须是现存、非被动/中立/观战/结束/胜利的可玩House；联机额外要求目标等于lastHouse，且HouseArray[index]、指针、UniqueID和House+30全部一致。请求需匹配当前对局且未过期，聊天、队列容量、统一预算、线程和原版计时回调检查继续生效；原版计时回调返回后、最终写队列前再次检查目标身份和请求。

调用原版构造器4C66C0后由统一enqueue选择传输：会话3/4写OutList，会话0/5写DoList并保留事件目标House。没有新建hook、直接执行4FC980、写House结束标志、修改对象Owner、替换CurrentPlayer或跳过接收器检查。标准pendingAckReady同request去重确保回执公布前重复进入command也不重复提交。按钮成功仅显示“自毁请求已入队”，失败需明确重新点击。

最终DLL SHA-256：`3917e6daf62f7af2d8d15adca7168f440056873a11eb1fed5e97ea2654792717`；EXE SHA-256：`85eee7e145eca16bb828a3371281fc79f81b6ca9abd0854d70df1b18f93dfc94`。完整初次构建后，依据原版发送器新证据更新Runtime和Host并分别重新编译链接；最终PE32、版本1.0.39.5、IPC77、Shared4165672、系统DLL依赖和实际CommonControls6 manifest检查通过。

最终DLL回归：敌方Sell准入/入队及接收门槛95项、己方变卖4项、Sleep/Guard/Harmless模式100项通过。它们验证工具箱提交与受控原版分支，不证明跨House经网络发送仍可保留原标签。纯C++测试constructor_no_options.cpp遍历0..255事件，确认只允许既有AllCheer/PlanCommit和新增Destruct，不放开Exit/Options等其他NoOptions事件。原版入OutList18项、完整发送/解包12项通过。

最终Destruct按钮Runtime测试180/180通过：112项队列/准入、36项pending回执去重、20项原版计时回调后的身份/请求变化、12项最终构造事件的原版接收器。179项首次通过；1项MP self的requestToken突变测试原先误写回相同0值，修正为翻转值后在同一DLL单独重跑通过，报告execution_note和reruns保留修正记录，未修改产品代码。报告目录：`verification/review-139-pre5-destruct/`。本版没有真实Windows视觉测试、实际网络发包/双机对战或完整DestroyAll销毁结果的验证。
