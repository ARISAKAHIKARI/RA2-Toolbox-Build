# Hotfix31：按连续House分包、协议0帧头一致与CRC边界

用户要求将外方House事件与本方原指令分开发送，并确认协议0的FrameInfo与CRC应怎样处理。后续明确更正为**按House分组，不是每条外方事件一包**。沿用Hotfix30渲染及1.0.39-pre.7代码，在独立 `codex/house-packet-isolation-20261031` 实现；正式latest保持v1.0.38。

调查前查阅INDEX、network-event-audit、pre.5/pre.6发送器笔记、control-138回归记录与同一SQLite v110。已存在的原生CRC、接收计数、同House排序和压缩字典结论不扩张为本次新发现。

## 原有行为与本次修正

原压缩协议1/2已按连续相同House限制编码数量，FrameInfo.House使用该组House，普通事件的House/Frame由接收器恢复。因此此前不能说改一条外方Sell就必然把邻接本方Move一起改成外方。协议0逐事件携带House，但以前同一个包仍可混合多个House、包首FrameInfo保持本方。

本次将分组选择提前至原版FrameInfo全部字段生成后、两种编码器分流前。以队首连续相同House为上限，容量不足仍由原版继续切包；不同House不共用一包，同House多条可同包。A,A,B,B,B,A,A,A在足够容量下为A两条、B三条、A三条，共三包。没有将隔着其他House的事件抽出来归并，没有全局重排或重复投递，也没有临时缩写真实OutList的count/tail。

协议0新增内循环上限，只在进入原版CMP前限制ESI；不改真实队列或外层剩余数量。协议1/2继续限制其原打包器的max-events参数。两条路径均由原版完成事件Frame、时间戳、DoList复制、缓冲容量、计数、Send与Flush。外方事件House的原有保留、关闭时未发外方取消、缓存清理与Runtime延后停止机制继续保留。

“独立包”指此处游戏层原生Send/Flush的边界，外层平台是否聚合、分片或额外封装不在受控证明内。真实传输来源仍是本机连接节点，没有变成对手客户端。

## 新确认入口及支持样本

本轮实际使用已有私有 `gamemd-spawn(3).exe`，SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，与旧研究原样本 `7b8a0685...` /重建夹具 `2fcb478c...` 区分，不混用样本哈希。检查了完整原生发送控制流；不提交私有EXE、原始映像或原始反汇编转储。

| 地址/ABI | 新确认用途 |
| --- | --- |
| `649D61`，5-byte displaced `mov eax,[A8B24C]` | FrameInfo的Frame/CRC/count/Delay已完成；原ESP+10为本轮剩余事件上限，EBX为输出头。受保护cdecl回调选择连续House前缀，只写header+2；继续649D66。失败跳649FD4，保留之前已完成的发送进度 |
| `649DBC`，8-byte displaced两次MOV | 原ESP+14为本包协议0已消费数量，ESP+10为总剩余上限；限制ESI后继续649DC4原CMP，到组边界进入649F5B发送当前包 |
| `649D4C`、`649D57` | 原版从AC51FC读取全局CRC，复制到FrameInfo+7；添加跨站点精确签名检查，保留该代码 |
| `649DC4` | 原CMP EDX,ESI与退出跳转保持原版；新增上下文签名 |

网络适配由5点扩为7点，新增两点同时加入统一GameEntryManager实验地址许可与Assist精确签名注册。整组成功才发布Ready；任一不兼容/冲突不覆盖他人代码。卸载逆序恢复全部7点。Assist82、Shared6580472、Runtime4、entry3不变。

## CRC为什么没有按House替换

沿用既有研究：AC51FC是整场游戏状态摘要；接收端按(Frame-Delay)&255与B04474全局历史比较，不存在一份属于本方、另一份属于对手的CRC供切换。这里的CRC也不是对本包字节重新算CRC32。修改House不改变对应帧的全局战局摘要，因此保留原版CRC、Frame、Delay与累计计数才是正确边界。

不能为了这次分包再次调用ComputeGameCRC=64DAB0：它会推进原版Scenario随机数状态。本轮发送不新增该调用，也不生成“对手CRC”、伪造摘要、修改接收比较或隐藏状态分歧。协议0改FrameInfo.House后，同包事件与头部House一致，CRC等其他字段仍用原版值。

## 验证及夹具维护

`network_house_runtime.py`直接加载本轮生产GameRuntime.dll，运行实际安装的7点thunk及样本原生649CA0/64B660/64BDD0；Win32、House/对象和传输回调为合成替身，数据只捕获，不发真实网络。

最终36项受控案例通过：

- Session3/4 × protocol0/1/2混合序列跨128槽回绕；3包House为0/1/0，外方3条一起发送。协议0包长333/444/444，每条原生命令的本机DoList和接收解码记录相符。
- FrameInfo CRC、CommandCount=100、Delay=5及原版Frame逐包保留；x87/SSE、ABI栈和不应修改的寄存器核对；协议0原生返回值仍为0，不改造成压缩协议的事件数语义。
- 三种协议纯本方命令的完整wire SHA256与Hotfix30逐一相同；发送未再次调用64DAB0。
- 两个新增站点冲突拒绝、旧站点/上下文冲突、非法队列、发送回调使目标失效、AI/观战/无效House过滤、关闭取消未发外方并保留本方和时间戳。
- 缓存House不可读、未完成清理时推迟stop ACK并保留映射/7点入口、可安全清理后恢复，以及真正Abort函数体仅写DLL自身状态。

纯策略另覆盖16512个合法环形队列、4088种连续House分组。

旧测试夹具停在此前Shared布局，本轮补上computeFrame/FrameInput、result-seqlock和tooltip字段，当前254个声明字段偏移及总尺寸与真实C++ ABI报告一致；RuntimeControl移出新的Shared范围。Zig全局构造内联由匹配GUID/age的PDB解析真实初始化函数后执行，仅析构登记作替身。修正协议0回调案例原先不适用的return=1预期，以及“任何OpenFileMapping尝试都是重连”的旧假设：可选RA2Frame通知映射在夹具中返回0，允许重试；仍严查主Assist/Runtime映射不被提前撤销或重连。最终证据逐项采用修正后重检结果，未改产品来迎合旧夹具。

生产x64 host/x86 Runtime/broker交叉编译、680项ABI、PE/import/version/manifest静态核验随包记录。未执行Windows loader、真实传输/平台、完整游戏模拟、联机或出售退款。

## 没有解决的边界

按House独立分包不修复多发节点共用事件House的调度顺序：本机先插本方产生的事件、再插收到的事件，仍可能让同House在不同端顺序相反。接收MegaMission字典仍按House索引，另一真实发送者的压缩引用也可能撞到本机外方组留下的缓存。之前受控复现仍有效，不能据此宣称联机稳定或保证对手建筑成功变卖。

拒绝的假说：邻接本方事件必然被一起改House；更改House必须重算某份对手CRC；分包可自动解决跨发节点排序/字典；捕获到正确字节就等于平台转发及完整出售完成。
