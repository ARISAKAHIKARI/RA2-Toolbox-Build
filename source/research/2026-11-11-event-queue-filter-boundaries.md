# OutList、本机 DoList 与接收端检查边界

## 问题与证据范围

用户问：OutList 到本机 DoList 是否过滤；远端收报与解码过滤什么。

本次是静态控制流复核，没有修改产品代码，没有运行游戏或实机联机。既有受控测试及平台分析分别引用下文旧笔记，不能把它们算作本轮新增执行结果。

| 样本 | SHA-256 | 本轮用途 |
| --- | --- | --- |
| spawn(5) | ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707 | PE 按 VA 取字节，所列四个区间与 clean capture 相同 |
| clean loaded image | 94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd | 原指令对照 |
| spawn(4) | c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a | 发送包装器、接收包装器和 decoder 区间与上述样本相同；压缩发送器有下述已知例外 |

四个半开区间及 SHA-256：
- [649CA0,64A000)：d7712b5a4f57a47cb4247a1d404e6cea80b82d57826445fe80391b62a472de21
- [64A0C0,64A622)：5759baa3acc3d060033048ec3bd855283017fc2e1c9c564c14fe97cb0a9639bf
- [64B660,64BDD0)：100dcb4c4b5da7b658c603b4c24e2e2e99b3f12b89b79fbf447ec1450929d209
- [64BDD0,64C37F)：a44999c0cec83c3d35b712ef83479fe7d15a38b3f3377afb5d694fc187447eda

spawn(4) 的 64B9F7 已跳转至磁盘未提供有效代码的 B7E637；本轮重遇此差异，并核对既有 2026-10-28-cncnet-ordinary-compatibility 记录。不能拿它声称该样本实际加载后的压缩发送路径完整无过滤。spawn(5) 的 objdump 报符号表截断，但 pefile 取得的所列代码区间完整且与 clean 映像相同；没有据此修补私有样本。

私有证据路径：工作区 private_engine/gamemd-spawn(4).exe、gamemd-spawn(5).exe、00-clean-game.mem；仅发布地址、摘要和结论，不发布文件或原始反汇编。复核工具：objdump -d -M intel 指定地址区间；pefile.get_data(VA-ImageBase,size) 与 clean[VA-400000:...] 比较。未新增测试夹具。

## 1. 发送端入本机 DoList

既有 ABI：649CA0 为 fastcall，ECX=transport、EDX=buffer，栈上传 capacity/delay/serial，ret 12；64B660 是协议 1/2 编码器。

- 649CB5..649CD2 用 8192-DoListCount 限定本轮待取数量；这是发送调度水位，DoList 实体容量仍为 16384。
- 协议 0 在 649DED..649DF5 检查输出包剩余空间；1F 变长记录计算额外长度。
- 649DFB..649E1E 改写 Event.Frame 和 House；House 来自 CurrentPlayer(A83D4C)+30。649E37 清 Event+1 的 bit0。
- 649E46 检查 DoListCount < 16384，649E7A 复制 111 字节，649EAE 更新数量。压缩原指令路径对应 64B9B9..64B9D4 的 House 归一化及 64B9FC/64BA38/64BA6A 的容量、复制、计数。
- 原发送器的本机复制在 649FA1 调用 transport Send 之前；不是收到远端 ACK 才入本机队列。
- 这不是统一的 CanBuild/目标控制权审查入口。不能由此推导此前 UI/入 OutList 没有条件，也不能推导后来 Respond 无检查。House 改写是字段归一化，可能改变命令含义，不是“合法性检查已通过”。

## 2. 收报、解码和队列准入

64A0C0 从实际 transport 连接映射源节点；64A3A7..64A406 维护该源节点的 Frame-Delay 进度及累计预期命令数。这是同步记账，不能等同于比较源连接身份与每条 Event.House。既有连接身份来源详见 CnCNet 事件记录。

- 协议 0：64A43B 至少需要 111 字节才进入记录循环；type19 走专用路径，不作为普通动作入队；清 Event+1 bit0；1F 有独立变长数据处理。64A49B 检查 16384 容量，再复制记录。余数不足一条时停止。该复制循环未见统一科技或目标控制者查询。
- 协议 1/2：64BDD0 为 fastcall(ECX=packet,EDX=length)，返回接受数或 -1。64BDF6/64BE14 设置 14 字节入循环门槛；64BE32..64BE43 从 FrameInfo 恢复 House/Frame；普通记录按类型长度表解码，MegaMission/变长事件另有分支。
- 64C325..64C32E 对后续记录剩余长度不足返回已接受数量；64C27F/64C191 的 DoList 满走 -1。64A53D 之后包装器区分 decoder 错误与成功记账；成功时去掉初始 FrameInfo 的一个计数再累计收到命令数。
- 64C256..64C27C 的零 Frame/type 诊断之后仍到普通入队检查；日志不等同于拒收。
- 这些只证明列出的格式门槛。没有证明对所有非法 type/index、变长负载、压缩缓存索引都有完整边界验证，也没有审计全部底层 socket/可靠包重传去重实现。不得概括成“完整验证全部报文”。
- 到达 DoList 后仍须等待对应模拟帧及 House 调度资格。FrameInfo 状态摘要比较发生在后续调度阶段，是模拟一致性检查，不是建造许可，也不是报文 payload CRC。

## 3. 执行和平台附加条件

历史已证而非本轮新增执行：
- 64C380 调度后进入 4C6CB0 Event::Respond；各事件分支仍有自身目标/状态/所有权条件。AI House 不符合原调度资格时，入队不等于执行。
- Produce 经 4C711B -> 4FA350 -> 5F7900 -> 4F7870；原版生产路径的 CanBuild limitOnly=true/includeProduction=true 不等于完整侧栏科技查询。
- 固定 CnCNet Hardened v0.0.0.17 DLL SHA 62466e9e534ceb15c5d71fb05c62ad98a2b2b4903c569ed0c82189ef86ea3b7d 另有：内容衍生 payload CRC 拒收；压缩 decoder 的连接 House 与事件 House 比较；部分 Respond 的目标控制权检查；生产 CanBuild(type,false,true) 和放置邻近性检查。身份失配停止本次解码且不回滚已接受前缀；执行权限失败可正常返回并消耗事件。不能把这些检查全部叫作“解码过滤”。
- 上述固定 DLL 不是 JJ 现用模块的证明；本轮未定位 JJ 现场拦截点。

当前私有操控队列替换原 OutList/编码发送路径，仍使用原可靠 transport，并在顶层 Send 接受后写入相同的本机 DoList 事件；Send 接受不是远端 ACK。没有替换对端 decoder、DoList 调度或 Respond。即使发送与本机入队成功，本机后续执行检查和对端接收/执行检查仍可能使操作无效。

参考：2026-11-11-destruct-delivery-and-icon、2026-11-10-private-control-outlist、2026-10-26-cncnet-anticheat-principles、2026-10-28-cncnet-ordinary-compatibility；完整历史内容保存在同一 SQLite notebook。

排除的推断：没有“入队即生效”“接管 OutList 就接管对端”“图标显示且点击取消必然是 OutList 被清空”的证据。未决：JJ 当前模块与命中现场、所有协议畸形输入分支、完整可靠传输层及真实多人一致性。

## 4. 用户实测补充（2026-11-11，Asia/Shanghai）

用户在本会话反馈：“懂了，局域网1.001确实能用，lbw用不了自毁敌方，JJ操控不了敌方。”

| 环境 | 用户报告的结果 | 记录边界 |
| --- | --- | --- |
| 局域网 1.001 | 确实能用 | 关联本段讨论的功能；用户未逐项给出控制、自毁及各模式的测试矩阵，不扩写为全部功能和所有配置均通过 |
| lbw（兰博玩） | 敌方自毁不可用 | 没有报告该平台敌方操控的结果，不能补成成功或失败 |
| JJ | 无法操控敌方 | 没有报告该平台敌方自毁的结果；未区分选择、提交、发送、对端解码或执行哪个环节失败 |

证据级别：用户真实使用反馈，助手未独立复现；本次没有新的游戏运行、抓包或接收端命中记录。用户未提供此次工具箱版本、具体模式/开关、平台模块版本、EXE/DLL SHA-256、协议号和目标是否人类。不得将前文静态样本的哈希自动归给这些现场。

结论更新：已有用户确认的局域网 1.001 可用现象及两个平台上的具体失败现象，支持按环境和功能分别记录兼容性；仍不能断定 lbw 与 JJ 使用相同的连接身份检查，或把失败唯一定位到对端。既有固定 Hardened 的检查只作为已知机制实例。发送端构造/传输、本端执行、对端解码/执行等分支的现场因果仍未确定。
