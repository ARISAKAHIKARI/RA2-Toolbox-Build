# 原生 Place 跨帧点击与 Q/W 缓存收尾

问题：原生事件放置需要反复等候／点击，且放下后按 Q 仍会进入旧建筑的僵尸游标。对照原生 MouseUp 的哪些收尾顺序存在差异？

支持私有 PE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。先复查研究索引、持久 SQLite 及现有 Place 接收、预览连续性、完成观察笔记。本轮没有打开真实游戏、发送网络事件或公开私有二进制／原始反汇编。

## 跨帧点击拒绝

`GameRuntimeHook` 在 Tactical 的 Host-ready 位为零时，仍会转发真实鼠标／键盘消息到 `AssistRuntimeFilter`；但这种转发原先不运行 Tactical 采样。预览的只读检查允许原生帧向前，松开鼠标真正提交时却必须满足 `simulationReady(false)` 的原生帧等于 `snap.frame`。因而玩家正常跨帧的一次点击可能被消费而没有 Place 入队，直到下一次 Host 唤醒后碰巧同帧才成功。

修复在现有 Runtime filter 入口先取得短期 Assist lease，确认消息属于主游戏窗口且有本工具放置／待收尾状态；释放 lease 后，通过已有 `dispatchFromTrigger(Message)` 和合并的 `hostWorkPending` 采样，再取得 filter lease 处理输入。没有引入新的 Hook、模拟帧回调、事件生产者或断点槽；没有放宽 `simulationReady`、世界身份、心跳、请求身份或最后一次入队检查。调度器忙或采样失败时仍不能凭旧帧发送。

## 原生缓存与游标的不同阶段

原始 MouseUp 在 Place 入队后、接收方实际放置之前，于 `0x4ABBD7` 调用 `0x734270(product)`。它再把 active 游标搬到 copy 游标并退休 active；这不是“等建筑 OnMap 才清 Q/W”。该顺序由支持 PE 的原始控制流核对，本轮没有执行完整原生 MouseUp。

| 地址／字段 | 含义与使用边界 |
| --- | --- |
| `0xB0FE5C / 0xB0FE60` | Q/W 完工建筑缓存，独立于 Map 的 active/copy 三栏 |
| `0x734270` | 原生 thiscall、零栈参数的按产品清理；45-byte FNV `61003681`；一次最多清一个匹配槽 |
| `Map+11A4/+11A8/+11AC` | active 的 product/type/House array index |
| `Map+1190/+1194/+1198` | copy 的相同三栏 |
| `0x4A8BF0 / 0x4A8D50` | thiscall、RET 4 的 active/copy foundation setter；仅在确切产品 OnMap 后观察式清理 |
| Product `+10/+520/+21C` | UID、type、owner；Type `+10` 也须匹配 |
| Product `+74/+81/+90/+6C` | OnMap、Limbo、Alive、HP；工厂消费不等于产品落地成功 |

修复把匹配 Q/W 缓存的退休前移到 `submitConstructed` 成功后的 `nativePlacementQueued`，并在待确认期间及输入过滤前重查。支持两个缓存都指向同一产品的情况，最多两次非空原生调用；空参数会清掉别的产品，禁止使用。签名、世界或产品读取不确定时等待重试。只允许确切的活着且仍处 Limbo／已 OnMap 的原产品，不因地址重用清除新产品。

active/copy foundation 的收尾仍须确认原产品活着且 OnMap、非 Limbo。维修／出售／超武等其他原生工具正在使用时，不清其游标，也不再丢掉原产品的完成观察；空闲后继续匹配清理。观察保留原来的 8 项／10 秒界限。

## 验证与未决问题

- `native_placement_completion.cpp` 直接编译生产 header，新增入队即清、两个同产品槽、外来槽保留、未知签名／短读恢复、Limbo 期间重新出现缓存、切换工具后继续收尾。Linux 的 DWORD seam + ASan/UBSan：117 checks PASS；Win32/x64 在 CI 执行同一 fixture。
- `native_placement_bridge.cpp` 使用真实 Place helper、提取自生产的 submit/enqueue 及实际 `AssistRuntimeFilter` adapter。受控中央采样验证新帧 down/up、采样失败仍拒绝、lease 拒绝和外来窗口；没有把私有 seam 当作完整真实调度器。
- `native_placement_input_runtime.py` 另执行实际编译 DLL 的 manager/filter/dispatcher/tick/admission、原始 Place ctor 和单机队列，明确把 Host-ready 置零；原生选点查询与音频 worker 创建为明示 seam。8 项 PASS；同一 fixture 对旧版 DLL 的 4 项复现证实 stale release 被拒绝。结果见 `evidence/2026-11-04-placement-input-runtime.json`。该测试不执行 Place 接收、工厂消耗或效果。
- `runtime_gesture_dispatch.py` 的现有 31 项编译 DLL 消息路由检查通过。

原生接收端仍会拒绝占用地面、非法地形、消失的工厂等。补齐完整 active→copy 交接、从接收失败立即恢复自定义预览，以及真实对局输入手感／双机一致性尚未在本轮证明；保留待确认超时后允许玩家显式重试，不自动重发 Place。不能把这次跨帧和缓存修复宣称为“所有原生落地失败均已消除”。
