# 自毁事件投递复核与应用图标（1.0.49-pre.5）

问题：选定敌方单位后提交 Destruct，能否保留目标所属方并由远端执行？本次核对当前私有队列分支与 notebook 的 `2026-11-10-destruct-delivery`、`2026-11-10-private-control-outlist` 历史结论，发现当前分支未包含旧投递修复。

## 结论与修复

- 原版发送器 0x649CA0 / 0x64B660 会将事件 House 改成本方。只改 OutList 中的 House 不足以跨方自毁，可能反作用于本方。原实现保留；联机跨方必须有完整所属方保留 Hook，或已激活的私有操控队列。
- 私有模式现在独立准入，不再强制依赖旧 sender 开关。仍拒绝非 current/human 的联机 AI House；其原版 DoList 调度条件未放开。
- 旧发送路径把 DisplayHidden 当成失效并删除待发外方事件。将已接受事件的投递资格与新输入资格分开：DisplayHidden 可以继续投递，不能接收新自毁输入。私有队列也在同一有效对局的隐藏分支继续发送。停止、只读、心跳超时、身份/世界变化等限制保留。
- 私有 Destruct 额外保存 House 指针、唯一 ID、索引，发送前复核；索引复用取消待发事件。显式自毁不使用自动操作的 3 秒排队过期。Send 拒绝时保留原事件，接受后一次性写入本机 DoList 并退队；此处接受不等于远端 ACK。
- 恢复单机 AI 路径：明确的当前手动请求通过最终复核后调用原版 Event::Respond；联机不走此即时调用。反馈区分单机已触发、请求入队与失败，不声称已远端执行。
- 应用加入自制 SVG / 九尺寸 ICO：16、20、24、32、40、48、64、128、256。EXE 图标组 101；原生及 WebView2 主窗口的大小图标均从应用资源加载。最终 PE 检查加入图标资源校验。

## 地址、ABI 与字段

- Destruct=0x08，111 字节 Event，signed House 位于 +2，无 payload。
- 构造器 0x4C66C0：thiscall(Event*, HouseIndex, kind)，ret 8。
- 压缩解码器 0x64BDD0 从 FrameInfo 恢复 House/Frame。协议 0 的单 Destruct 包为 222 字节；协议 1/2 为 15 字节。
- Event::Respond 0x4C6CB0：thiscall(Event*)，分支 0x4C7BDF 调用 House::Blowup 0x4FC980（thiscall，无额外参数）。设置 House+0x1F6，计时器 +0x298=当前帧、+0x2A0=0；已经结束、获胜或失败时不再改变。
- House 数组 0xA80228，House ID +0x10、索引 +0x30、current +0x1EC、human +0x1ED。后续 House AI 销毁链和完整结算未在本轮执行。
- 私有编码器通过原可靠传输对象 0xA8E9C0，Send 0x5414C0（thiscall，buffer/length/1/-1），Flush 0x541820；未改接收端规则，也没有远端回执。

## 证据与边界

`evidence/2026-11-11-destruct-delivery.json` 记录受控结果；复现脚本为 `assist/tests/destruct_delivery_regression.py`、`private_outlist_fixture.cpp`、`private_outlist_capture_probe.py`、`private_outlist_native_probe.py`、`destruct_receiver_probe.py`。

- 当前 gate 回归通过；4396cea 基线在“隐藏后继续投递”断言失败，确认该问题可复现。
- 84 组生产接管/原路回退 ABI，4 组按值预定，可靠传输拒绝/重试/队列回绕检查通过。
- 生产私有队列 → 编码 → 原版解码 → 原版接收器：协议 0/1/2 均保留目标 House=1、本方=0；只设置目标结束状态。拒绝发送后重试一次、显式请求保留、三种目标身份变化取消、关闭取消、联机 AI 拒绝及私有/旧路径准入通过。
- 327 组原版编码/解码对照通过。初始化内存映像 SHA-256：`94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd`。
- 两份 spawn PE 的构造/接收器各 20 组通过；SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`、`ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`。这不是平台实战兼容承诺。

排除：入队成功不能证明实际发送；本机 Send 接受不能证明敌方接收；图标显示也不能证明生产许可。当前证据不支持“JJ 一定清掉 OutList”这一单一根因。

未决：真实 JJ/其他平台是否拦截可靠传输、接收器或调度；全部参战端的同步执行与完整整方销毁/结算；Windows Explorer/任务栏实机视觉。未发布私有 EXE 或原始内存映像。

## 追加：原版 Send 成功值的边界

进一步核对上述两份 spawn PE 的 0x5414C0，并各执行 6 个受控场景（`assist/tests/native_send_boundary.py`，结果收录 verification 包）。此函数 thiscall，ret 16；对象 +4/+5 为启用条件、+0x44 为连接数、+0x28 起为连接指针。

普通广播分支先遍历所有连接，比较各连接 +4 指向的队列之 +0x28 计数和 +4 容量；任何一个满即返回 0，不调用任一连接的发送入口。随后逐连接调用虚表 +8，**忽略其返回值**，广播总函数返回 1。因此把一个下层调用受控替换成返回 0 时，顶层仍返回 1。session=4 且 0xA8DBA0 非零的中继分支不同：使用对象 +0x50 的连接、recipient=255，保留下层返回值。

这验证了“顶层 Send 接受”不等于“所有节点接收”，也不能据此断言平台实际执行过下层拦截。私有队列只绕过原 OutList/编码器，仍使用原可靠传输；没有新增远端 ACK。平台底层 Hook、真实网络、对手接收/执行均待实机确认。不能因 Send=1 未见销毁而自动再发，否则可能产生重复事件。
