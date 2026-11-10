# 1.0.48-pre.2：完整解锁、触发视角保护、面板布局与 Sell 直接组包

## 问题、来源与证据范围

用户报告触发锁定后仍无法框选/拖屏、超武面板高度固定且可能遮挡预定面板，并要求阻止触发切屏。进一步要求快捷键变卖直接组联机协议包，而不是写 OutList；保留原生变卖光标，实现单机电脑建筑出售，联机电脑建筑不生效。

调查前已查 INDEX、2026-11-10-input-planning-and-bomb-board、2026-10-26-enemy-sell-pre4、destruct-button-pre5 及 network-house-preserve-pre6、对应后续发送分组笔记和持久 SQLite。基线 main 为 00a4494708fe409c56246cdc793fe811c777acb7，Assist104。

本轮私有原生捕获映像 SHA256：94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd；既有捕获记录关联原始 EXE 7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6。原始 EXE 字节本轮不在场，不将两者混称。另有 gamemd-spawn(4) EXE c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a、gamemd-spawn(5) EXE ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707；三份对应锁/解锁及触发入口指令核对一致。公开字段参考 YRpp d9795546950fcc75a94325ffc83b44d16ec34ce2。

这里只报告静态指令与受控执行。没有 Windows loader、真实 HWND 绘制、整局游戏、平台传输、双机联机、完整拆除/退款验收。未发布私人 EXE、内存映像或原始反汇编转储。

## 输入锁遗漏

- Scenario 指针 A8B230，UserInputLocked 为 Scenario+35A2；A8ED9C 是另一全局输入锁，模态窗口/缩放也会使用。
- 原生 LockInput 684240（cdecl，无参数）先清选择集合，必要时增加 WWMouse 锁计数，然后设置两个标志，调用输入刷新 54F720，并清 B07784。
- 原生 UnlockInput 684290（cdecl，无参数）在 Scenario 锁为1时通过 WWMouse（887640）虚表+28查询计数、+10逐次解锁，清两个标志，刷新输入并清 B07784。旧实现仅清 A8ED9C，遗漏剧情标志与鼠标计数。
- 新维护逻辑只在剧情锁明确为1、有效可操作前台、非聊天上下文调用完整原生解锁；验证入口与收尾、鼠标方法可执行性和锁计数0..64。全局锁单独成立时不动，避免误解除菜单占用。关闭不重新套回旧锁。
- 仅在锁定后恢复仍可能被重复触发先清选择，因此新增共享硬件槽上的触发动作过滤；不是增加独立 VEH 或修改游戏指令。

## 触发动作过滤

TActionClass::Execute 6DD8B0，ECX=TAction，动作编号+2C，参数 House*, Object*, Trigger*, Cell&，thiscall bool，ret16。新增逻辑 tag64，和已有科技/地图/控制/FPS共用四个物理DR槽，SiteCount由6到7。容量不足不夺走已有槽，UI显示未就绪；原生解锁仍单独尝试。

- 保持操作权限：只跳过动作46 LockInput，阻止其清选择和压鼠标锁。
- 独立视角开关：跳过48 MoveCameraToWaypoint、49 ZoomIn、112 CenterCameraAtWaypoint、145 JumpCameraHome。
- 保留47 UnlockInput、50 ZoomOut，可结束开启前已有锁/缩放；保留40 ResizePlayerView（改变地图边界），保留其他触发和手动滚屏。
- 已核对动作入口签名、世界身份、原生返回地址后，仅设置 EAX=1，EIP=[ESP]，ESP+=20。卸载/只读/失联/换局撤销权限与槽位；不写游戏代码、单位归属或战败标志。
- 开关允许单机和联机请求，尚无双机证据；“本机输入状态”不等于对全部平台作无不同步保证。开启前已开始的持续镜头移动不在本轮撤回范围。

## 超武面板

旧 rows()只按配置上限和客户区高度，忽略超武数量；默认定位也不查询预定面板。改为 min(ceil(条目/2),配置上限,可容纳行数)，无超武隐藏。预定面板先服务，随后将实际显示矩形传给超武面板；相交时选择最近可容纳的左/上/下/右位置并夹到客户区，手动拖动期间不干扰，松开后避让。极小窗口若无法同时放下两个面板，几何函数不虚构可用空间。

## 单机电脑变卖

沿用已证实的 House+1EC/+1ED 调度限制：AI 无人类调度资格时，其事件即使进入普通队列也可能不执行。新增仅 session0/5、明确手动 Sell、可玩敌对建筑且非人类 House 的专用接收路径。构造与原版等价 Sell 事件，再调用完整 EventClass::Execute 4C6CB0（thiscall，无参数）。保留4C6F3B所属方检查、原生类型判断与建筑447110方法；不把AI临时改成人类，不直接绕过接收器调用建筑方法。

原生变卖光标仍保留 CanSell、原有资格与联机事件流程。仅单机AI分支在原有点击回调内处理一次，随后跳过已处理的构造/OutList追加；已核对4AC179处有12字节预压参数，消耗分支先ESP+=12，继续4AC20C原生收尾。联机快捷键和原生光标都拒绝AI House。原生光标的人类对手联机路径仍需既有所属方保留适配。

## 联机快捷键直接组包

不是“把原生事件直接写入OutList”。新增限16项的工具箱私有待发列表，记录对象/Owner/UID/对局/手动版本与3秒时限。发送前反复验证；从原生全局读取帧延迟、协议、全局CRC和命令计数，等待当前帧CRC历史已经发布，协议2还等待周期边界。没有再次调用 ComputeGameCRC（会推进随机数）。

确定的原生链与字段：

- 649CA0 原生打包器 fastcall(transport,buffer,capacity,delay,serial)，ret12；本实现不调用。
- 原版调用点647F93..647FBE取 A8B550 延迟、AFA400 低16位累计命令计数，调用后对压缩协议累加消费数。
- FrameInfo 类型1C，+2 House，+3 Frame，+7 CRC，+B serial16，+D Delay8；CRC来自AC51FC。B04474[(frame&255)]由原生当前帧调度发布。协议2周期A8B554。
- Sell为16，payload是网络对象ID32+Techno标签34。协议0是111字节FrameInfo+111字节Sell，共222；协议1/2为14字节FrameInfo+1字节类型+5字节引用，共20。Sell不读写MegaMission压缩字典。
- LAN/Internet的现有 transport 对象 A8E9C0，vtable7EC16C，+8 Send=5414C0，thiscall(buffer,length,reliable=1,destination=-1)，ret16；+4 Flush=541820，无栈参数。继续使用它下层的当前连接/平台封装，而不是新建UDP套接字猜目的地址。
- 成功接受后，将完全相同执行帧的Sell放入本机DoList；协议1/2累计命令序号加1（16位回绕）；保留协议0原生不增加该计数的行为；发送包统计B0487C加1。先退休私有请求，再Flush，避免已被传输接受的请求重试。
- Send拒绝时不追加本机执行、不更新计数；50ms后可重试且受3秒时限约束。满OutList不阻塞此路径。本机DoList满、CRC尚未就绪时等待；目标失效、改主、变AI、关闭资格、换局、版本变化时取消。
- 快捷键路径不调用ClickedEvent、对象事件构造器、649CA0/64B660，也不改原生OutList内容或索引。原生变卖光标保持上述独立方式。

边界：正确wire与同帧本机事件不证明真实平台转发成功；跨发送者共用对手House的调度顺序问题仍在，不能宣称跨方出售稳定。底层平台仍可能过滤传输。多节点同时发送同House冲突、完整退款与真实多人CRC尚未验收；默认关闭的对手出售实验性质不变。

## 验证

- input_lock_test.cpp：调用原生解锁回调、仅全局模态锁不动、空闲不反复调用、关闭不重锁、读取/调用/世界切换失败。
- input_panels_sell_policy.cpp：61,440行数组合；常用分辨率与75/100/150/200%布局避让；触发动作过滤、共享DR预算、Sell编码与非法参数。
- sell_direct_policy.cpp：直接编译生产service头，三协议、本机执行帧、序号回绕、满OutList不影响、传输拒绝重试、CRC等待、AI/换主/手动覆盖/超时/调用边界失效及私有容量。
- native_sell_wire_probe.py：12个protocol0/1/2×House0/1/7/31案例，生产C++编码与原生649CA0输出逐字节相同；原生64BDD0压缩解包还原相同Sell。仅捕获型transport，未发网络。
- native_input_sell_receiver_probe.py：原生684290对初始鼠标深度0/1/5/64完整解锁并刷新；原生4C6CB0→447110保留Owner拒绝/接受，接受时进入Selling任务19。完整拆除动画、退款和House AI后继未运行。
- 生产构建：x64 Host + x86 Runtime/broker，Assist105，预计Shared23162640。最终PE与包哈希以构建后收据为准，不在本文预先声称通过。


## 最终交付收据

代码提交 `9342c7ede9c71036e908d4bd54ed6ac7c7fd99e0`；846个冻结构建输入Git blob与该提交逐个一致。完整 x64 Host / x86 Runtime、broker 编译及静态 PE/ABI 校验均返回0；Assist105，Shared23162640。三个生产策略测试、12项原生wire对照、6项原生解锁/接收案例通过；无真实游戏或联机测试。

更新包 `RA2Toolbox-1.0.48-pre.2-Input-Panels-Sell-x64-Update.zip`，4,126,219字节，SHA256 `cc17e03e715a70b65892edc9b91e603935d6039b8155d799483dcb612ca460c6`。独立依赖包 `RA2Toolbox-1.0.48-x64-Dependencies.zip`，104,375,534字节，SHA256 `dd6b89dacda804b9e3d108082bbc4fdb1fcffe1732eab9b6dfea27702e7fd37b`。两包已交付；详细二进制与验证包哈希见 [交付收据](evidence/2026-11-10-input-panels-direct-sell.json)。
