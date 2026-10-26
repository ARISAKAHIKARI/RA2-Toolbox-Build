# 联网所属方保留、单位Owner发令、原生出售与任务仲裁（1.0.39-pre.6）

## 问题与证据边界

用户要求修改原版打包器，保留 OutList 中已指定的所属方；同时放宽原生出售光标的所属方门槛，让原生 MegaMission 参与工具箱手动任务仲裁，并增加按发令单位实际Owner构造命令的独立实验开关。先检索 INDEX、pre.5 自毁研究及持久 SQLite 第63版，再复查发送、解包、接收计数及任务入口。

支持原始 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。从私有捕获重建、供原版指令执行测试使用的夹具 SHA-256：`2fcb478cb2e0de00b1af1a6c82e9b55e56e0b555f20f3c0a018e8aaddc58b0c8`。两者不是同一文件，不把夹具哈希写成原始 EXE 哈希；不提交游戏 EXE、捕获内存或原始转储。

本轮区分三类证据：原版静态控制流、原版指令加合成世界/传输替身的受控模型、最终 DLL 适配器测试。成功入队、编码与解码相符、接收计数相符，均不等于真实平台已经接受或完整游戏已经执行。没有真实网络发包、双机对战或完整出售退款／所属方销毁的验证。

## 为什么不能只删除两处 House 写入

pre.5 已执行原版发送主函数 `649CA0`，确认协议字段 `A8B24C` 为0时，在 `649E1E` 将事件House改为CurrentHouse；为1/2时，在压缩打包器 `64B9D4` 做同样改写。改写后才复制本机DoList。

协议0逐条编码111字节事件槽；协议1/2的普通事件只编码类型和负载，事件House、Frame由包首FrameInfo恢复。因此只跳过这两处写入，会得到“本机DoList保留外方House，对端解码仍取本方FrameInfo”的确定性差异。本轮原版模型已覆盖这个负例，不能把它作为压缩协议的实现。

向一个压缩包内插入额外FrameInfo也不能视为无副作用的字段切换。接收端把额外FrameInfo计入已收命令数；MegaMission字典索引还使用包首缓存的House，不跟随中途FrameInfo一起切换。本版采用**连续相同House分包，每包只有一个原版FrameInfo**。

## 原版接收端按真实发送节点统计

接收入口 `64A0C0` 先调用传输对象虚表 `+18`，把底层收到的来源连接ID映射为节点序号。支持样本的网络对象 `A8E9C0` 对应映射函数 `541040`：遍历对象 `+28` 的连接指针数组，数量在 `+44`，比较各连接 `+64` 与传入来源ID。正常收包 `5415F0` 也从连接 `+64` 输出来源ID。这与数据包内的FrameInfo.House是两个来源。

| 阶段 | 地址／字段 | 已确认含义 |
| --- | --- | --- |
| 构造帧头 | `649D40..649D49` | 原版填CurrentHouse；适配器只为当前同House事件组调整该帧头House |
| 发送计数 | `AFA400`、`647FBE`、`6483C1` | FrameInfo `+0B` 来自发送前累计事件数；调用者累加649CA0的返回值，协议1/2为本次消费数量 |
| 接收进度 | `64A3A7..64A3F4` | 按传输映射得到的sender节点，更新其记录 `+0` 的Frame−Delay |
| 预期数量 | `64A3F9..64A406` | 首FrameInfo `+0B` 更新同一sender节点记录 `+4` |
| 实收数量 | `64A563..64A590` | 解码事件总数只扣除一个包首FrameInfo，再累加同一sender节点记录 `+8` |
| 摘要比较 | `64C68F..64C6F2` | 比较全局历史摘要 `B04474[((Frame−Delay)&255)]` 与事件CRC，不是每House各自一份CRC |

`assist/tests/network_sender_accounting_probe.py` 运行完整原版 `64A0C0`、实际 `541040` 映射函数及相应解包分支。24项覆盖协议0/1/2、来源节点0/1、FrameInfo.House0/1，以及有无额外FrameInfo。

结果：进度、expected、received均归于**传输来源节点**；改变FrameInfo.House不会转而更新另一个节点的记录。DoList事件House仍按原版格式恢复。额外FrameInfo会使received再加1，证实它不是不计数的内嵌标记。这个测试使用合成连接记录和输入缓冲，没有运行底层socket接收、完整锁步等待、CRC执行分支或事件效果。

## 五点网络适配组

实现位于 `assist/source/network_sender_bridge.h`，纯策略和精确站点签名位于 `network_sender_policy.h`。所有可执行字节改动由共用GameEntryManager登记，五点整组可用后才公布Active并放行新的外方请求。

| 站点 | 长度 | 行为与返回位置 |
| --- | --- | --- |
| `649CA0` | 5 | 外层发送入口；在Capture/SEH与FP状态保护内运行原版完整发送循环，重放原始前缀后进入649CA5 |
| `649E1E` | 7 | 协议0发送期间保留事件House，其余帧、时间戳、计数、复制与原生发送照常运行 |
| `64B660` | 6 | 原压缩打包器入口；把最大事件数限为队首连续同House前缀，并将当前FrameInfo.House设为该组House，之后进入64B666 |
| `64B9D4` | 7 | 协议1/2发送期间保留事件House，随后仍由原版复制本机DoList |
| `64B4EA` | 10 | 启用期间跳至64B551，跳过发送端字典命中查询，保留原版完整对象组编码与缓存槽写入 |

外层 `649CA0` 是fastcall：ECX=传输对象，EDX=输出缓冲，三个栈参数为容量、帧延迟、FrameInfo累计计数输入，原版ret12。内部 `64B660` 的栈参数依次为帧延迟、初始长度、最大事件数、输出消费数指针，ret16。适配器保留原版外层循环，让它按实际消费数继续后续压缩分组，未另写socket、重排事件或重复投递。受控对照还确认：支持样本的协议0路径可以消费OutList而返回EAX=0；本版保留这个原版行为，不把三个协议的返回值统一解释成消费数量。

分组回调读取原调用栈的 `+12` 最大事件数，只调整当前组。当前组尚未消费事件时若身份或队列校验失败，返回原外层 `649FD4`，让原版返回此前已经消费的数量；不把当前未完成组当作已发送。正常协议0事件仍逐条自带House，协议1/2由每组单个FrameInfo恢复House和Frame。保留外方范围不限于Sell/Destruct；但必须是已验证的原生游戏事件和有效可调度House，FrameSync/FrameInfo等流控制记录不作为外方玩法事件放行。

## MegaMission压缩字典与关闭清理

发送 `64B4D0` 使用全局八槽字典；接收 `64BDD0` 在入口缓存包首House，后续以 `House×8` 选择接收字典。按House分包但继续跨House发送旧字典索引，可能引用该House从未建立过的对象组。本轮模型复现了这一负例，不能由Sell/Destruct解码正确推出MegaMission也正确。

启用期间令字典查询总是miss，原版仍发送完整对象组并选择槽位，对端据包首House建立对应字典。这样避免跨House复用只有索引的压缩引用。原版事件 `+0D` 在此路径是缓存槽元数据，原构造器未将它作为任务目标参数初始化；本机事件与解码结果不要求这一个内部字节恒为零。任务、来源对象、目标／目的地／最近格引用、House、Frame及顺序仍须一致。

关闭前清空发送端 `AFA338` 起八个DWORD有效数量，避免之后原版本方发送复用遗留的外方字典；不清接收端的House字典。模型也复现了未清理时“先外方完整组，再恢复原版发送同一对象组”可引用错误缓存，以及清理后的重复开关恢复。

开关关闭、Host心跳失效或适配条件失效时，先停止新外方准入，再在游戏线程读取当前OutList，按原顺序保留本方事件，同时搬移对应时间戳，并取消**尚未发送的外方事件**。不取消已发数据，不改DoList，也不回滚已执行效果。确认清理完成后才恢复原版字节，防止待发外方Destruct退化为本方Destruct。

队列要求count/head/tail一致且在128槽边界内。记录对局epoch、Scenario、House数组、当前House及身份；换局后不把旧身份重新绑定到一个尚有外方事件的新队列，再用旧状态清理。世界身份不明、队列不可读或无法完成安全恢复时保留外层阻断，报告Blocked。代码冲突不覆盖他人字节；整个适配组未就绪时，工具的外方自毁不得入队。

异常处理也遵守这个边界：`AssistNetworkSenderAbort` 只修改DLL自身的packing/enabled/faulted/state，不在SEH处理器里再次读取可能已经失效的House、队列或Shared，避免第一次异常后再次访问同一失效地址。世界检查、待发清理与无外方队列判断中的缓存House身份使用安全读取；状态发布和取消工作留给后续普通受保护入口。

Runtime停止时先撤去新的原生输入／Owner准入，网络组未安全恢复则不写 `stop=2`、不解除IPC及最后的消息唤醒入口，保留重试机会。通用disconnect同样以网络恢复成功为前提。Host正常关闭会停止新工作，保留待回收连接与定时唤醒，等Runtime停止应答或游戏进程结束后再销毁窗口；不会因一个固定等待时限而先卸掉仍在保护待发队列的入口。这不等于已经发送的命令被撤销。

## House有效不等于一定执行

原版DoList在 `64C4DC..64C550` 遍历House，仅处理 `+1EC` 或 `+1ED` 为真的参与方，再按事件House和仿真帧调度。未被这两个标志覆盖的AI House，即使能编码也可能根本不进入执行器。本版外方准入同时检查House数组实际槽位、索引、可玩状态和这道原版调度条件；没有修改AI控制标志、Observer或CurrentPlayer。

这一点也限制旧的跨玩家MegaMission：以前把事件头改回本方，某些普通Move/Attack仍可引用外方对象；如今要求保留对象所属方时，不能忽略目标House本身的DoList资格。不能把“保留所有有效House”描述为对任何AI、观战或负编号House都能下令。

## 原生出售光标

`assist/source/native_sell_cursor_bridge.h` 采用两个原生输入站点，独立默认关闭：

- `6929E5`：原来的Owner资格调用。仅对显式实验开关下、当前对局内有效敌方建筑放宽该判断；后续原生虚函数 `+98` 的CanSell仍保留。
- `4AC179`：构造Sell前原来的CurrentHouse／索引读取。核对刚才光标候选的对象ID、Owner身份、House索引、对局、手动版本和时效，再以实际Owner索引填写原生构造参数。

联机额外要求完整网络所属方适配组Ready与目标House具备原版DoList资格。原生CanSell调用返回后再复查身份与许可。后续仍走原版对象事件构造器、OutList、传输与接收器；未直接调用本机Building::Sell，未修改对象Owner，也未移除接收器的Sell所属方匹配条件。光标放行和Owner调整统计分别表示入口经过，不代表已经完成出售退款。

## 原生MegaMission成功后仲裁

`assist/source/native_mega_gateway_bridge.h` 的独立开关默认开启。在普通 `QueueMegaMission=646E90` 成功追加OutList后的尾部 `646F4E` 接入，原生构造、111字节复制、时间戳及队列指针更新已成功才登记。队列满失败路径不进入此回调。

回调比对仍在栈上的事件与刚追加的槽位，核对本方可跟踪对象后，更新工具的手动版本、任务仲裁、火控／队形等自动任务租约及只读执行观察。取消旧自动任务使用 `cancelNative(object,false)`，不调用ForceMission或解除Sticky，不再提交第二条事件，不修改已接受的原生事件。工具自己的原生发送调用通过深度标记排除重复登记；元数据空间不足只影响观察，不阻止已成功的原生命令。

范围仅普通646E90成功尾部，不涵盖 `QueueMegaMissionF=646F70`、规划系统直接写入的本机DoList、接收端QueueMission，或所有可能的原生发令入口。规划节点只有确实经过这个发送函数时才被此回调观察。不得将本功能表述为完全接管所有MegaMission构造与执行。

## 按发令单位实际Owner构造命令

新增独立控件9193“命令使用单位所属方（实验性）”，默认关闭，持久项为 `Experimental/ControlCommandOwner`。它配合已有跨玩家操控开关工作，与网络打包保留及成功后仲裁分别管理。实现位于 `control_command_owner_bridge.h`、`control_object_owner_bridge.h`；所属方从**命令来源对象**的 `+21C` Owner及Owner的 `+30` 索引取得，不从攻击目标、目的建筑或鼠标目标推断。

| 站点 | 路径 | 处理 |
| --- | --- | --- |
| `646EF5`，10字节 | 普通QueueMegaMission，事件04 | 原构造器返回后、容量判断与追加前，将有效外方来源对象的Owner索引写入事件House |
| `646FD2`，10字节 | QueueMegaMissionF，事件05 | 同样在原构造器之后、追加之前处理；不改变其余任务和引用字段 |
| `6FFC93`，8字节 | 规划追加入口 | 外方规划在首次追加前拒绝；本方继续原版路径 |
| `6FFE2B`，5字节 | 原生Techno::ClickedEvent | 解析来源NetID，核对事件白名单后，替换即将传入原构造器的House参数 |

ClickedEvent外方白名单为 `01/02` 建筑电力、`06/07` Idle／Scatter、`09/0A` Deploy／Detonate、`11/15/16` Primary／Repair／Sell。全局、生产、House-only和规划事件不能被当作单位事件套用。原生本方对象调用保留其原有类型及规划处理；不因外方白名单缩窄正常本方命令范围。

原生规划追加与PlanCommit是一对；原版提交事件没有来源单位，仍命名CurrentHouse。只改规划节点House无法证明提交也一致，因此本版拒绝外方规划，不临时切换CurrentPlayer或替造PlanCommit。接收端规划处理及随后直接写入本机DoList的路径保持原版。

四个准入点必须完整就绪，且满足跨玩家操控、前台输入、当前对局、心跳、无聊天、来源对象身份及House数组关系；联机外方还要求网络保留组Ready及目标House的DoList资格。启用9193时，原本放宽可控性的入口也依赖这套完整能力；不能只放宽选中／点击，却让后续命令退回本方House。失败沿原版失败返回路径结束，不写一个错误的本方替代事件。

工具Alt指令既有路径原本已经把来源单位的实际Owner传给 `nativeMission(...,PlayerTest)`。本版继续复用该路径，在实验选项开启时补上上述完整能力门槛，并在原版发送计时回调之后、最终入队之前再次 `authorizePlayerOrder`：重新核对来源ID、实际Owner、开关和传输能力，防止回调边界内Owner改变或功能关闭后仍提交旧身份事件。原生构造器、容量判断、OutList追加和接收器仍负责各自后续步骤；Accepted统计表示身份准备或入队，不表示远端执行完成。

关闭9193而游戏线程尚未维护卸载时，仍在运行的准入点继续拒绝外方命令，不会回退为CurrentHouse；本方旁路同时比较缓存lastHouse与当前CurrentHouse，避免切换瞬间误认。本版对外方ClickedEvent也校验count/head/tail完整环形队列关系，异常结构不追加。

原有controlAll仍包含 `50B6F0` 共享ControlledByPlayer判定的放宽，不只是光标补丁；本版证明的是命令身份与传输一致性，不能据此宣称旧controlAll全部仿真调用者已经通过联机同步验证。

这四点与 `646F4E` 成功后仲裁职责不同：Owner适配包含事件04/05及受限ClickedEvent，仲裁仍只覆盖普通646E90成功尾部。新增Owner路径并未扩大仲裁对646F70或规划DoList的覆盖。

## 设置、协议与验证

四个独立设置默认值为：网络所属方保留off、原生出售光标off、原生MegaMission仲裁on、按单位Owner发令off。设置均持久保存；Host和Runtime通过 `IPC79` 通信，Shared为 `4165736` 字节，必须成套替换。网络状态Active只表示整组适配已就绪；Dropped只统计被取消的未发事件，Packets只记录经过原生发送调用的包数，均不是远端执行成功统计。

已完成的原版／模型及纯策略证据：

- pre.5原版完整发送／解包12项，证明原版覆盖House。本轮 `network_house_batching_probe.py` 的41项覆盖两处NOP负例与按House分组模型，报告 `verification/review-139-pre6-network-house/network-house-batching-native.json`。
- `network_sender_accounting_probe.py`：24项原版接收、真实连接ID映射和计数检查，报告见 `verification/review-139-pre6-network-house/network-sender-accounting-native.json`。
- `network_house_cache_probe.py` 的34项泛化MegaMission模型包含完整组字典miss、House往返切换、非零目标引用、包容量、缓存遗留与开关恢复，报告 `verification/review-139-pre6-network-house/network-house-cache-native.json`。这些是原版指令加拦截模型，不能替代最终DLL适配器验证。
- `network_sender_policy.cpp`：16,512个合法环形队列、4,088个有序House分组及非法结构／元事件边界，通过独立packet限制验证不跨House、不重排、不丢事件。

最终DLL SHA-256：`24c405af124ec60c88b92c47b0cbaca9ab02ba1ed7530f5631f442520f43a08c`；最终EXE SHA-256：`4db463cbf9f3631b398fc4c93e5a48c2bcd8faecc3b67f2b063690c2b7b3c44d`。最终二进制在测试前复制到冻结目录，各项最终报告分别核对同一DLL哈希。

最终DLL受控验证全部通过，共737项：

| 验证范围 | 通过 | 报告（verification/review-139-pre6-network-house/） |
| --- | ---: | --- |
| 五点网络适配、关闭清理、失效指针、延迟停止ACK、真实Abort | 30 | network-house-runtime.json |
| 普通Mega/F实际发包与解包、混合Owner、规划拒绝、开关/CurrentHouse过渡、Alt计时边界 | 72 | control-command-owner-runtime.json |
| ClickedEvent所有权、256种类型白名单穷举、异常队列、ABI | 78 | control-object-owner-runtime.json |
| 原生普通MegaMission统一仲裁 | 82 | native-mega-gateway-runtime.json |
| 原生变卖光标及构造入口 | 96 | native-sell-cursor-runtime.json |
| 自毁按钮默认行为与接收器回归 | 180 | destruct-runtime.json |
| 既有敌方Sell实验 | 95 | enemy-sell-runtime.json |
| Sleep/Guard/Harmless任务模式 | 100 | selected-mode-runtime.json |
| 己方原有变卖快捷入口 | 4 | own-sell-runtime.json |

另有30项原版出售入口基线（native-sell-entry-baseline.json），使用私有原始内存捕获；其capture哈希与上文重建PE夹具哈希不同，报告分别记录。源游戏EXE哈希一致。前述原版模型41/34/24项、纯策略16,512/4,088项及既有补丁允许表回归通过。

完整Zig 0.13.0 Win32构建通过；最终EXE/DLL的PE32、版本1.0.39.6、IPC79、Shared4165736、导出、系统依赖与Common Controls manifest静态检查通过，见windows-build.json。发布ZIP仅包含RA2Toolbox.exe与GameRuntime.dll，必须一起替换。正常关闭等待清理的Host对象已编译，但未在Windows桌面实测交互。Abort测试执行匹配PDB定位的真实函数，不能替代Windows SEH分派测试。

所有上述测试均为受控执行；未验证真实网络、完整平台校验、双机同步、出售退款/拆除或DestroyAll完整后果。实验开关默认值和不支持的外方规划边界仍然适用。
