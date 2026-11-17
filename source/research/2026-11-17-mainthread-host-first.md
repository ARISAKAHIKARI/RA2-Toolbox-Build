# 游戏主线程拆分：Host 默认计算与按需游戏帧通知

## 问题与源码边界
基线 main `b8f4371380a226e467c7aac856dbf020c0d8d861`。该轮只处理计算分工与无消费者游戏帧通知，**没有将 CaptureGraph 整份采样搬离游戏线程**。最近独立修复 PR #43 已合入异步发布者的停机恢复。

- `compute_policy.h::Default` 原来选择部分 Repair/Load/Unload/Escape/Missile/Burst/Dodge 游戏内纯算法，尽管对应 Host worker 已存在。新建、未保存 Compute 配置优先选 Host 计算，原有 INI 明确设置仍遵守，Runtime 在新 match 通过 `computeApplied` 锁存，避免同局双计算者。
- `main.cpp::tick` 原 `computeApplied != compute::All` 即使所有 Host 计算功能均关闭，也要求 native frame notices。这触发无需要的游戏线程入口维护；本轮把布尔判断换成以活动 Host planner flags、repair/unload/dodge/missile/magnetronGlobal/Drop 和待完成任务/ACK 为依据的明确需求。
- `gameinput::setFrameSampling` 依旧让真实显示订阅与超武授予保持唤醒。无活动 Host 任务的配置变动不等价于世界进度事件；独立 `wakeGame()`、命令处理、Runtime 调度及外部绘制时钟保持原路径。

## 迁移清单

| 范围 | 当前执行位置 | 本轮变化 |
| --- | --- | --- |
| 装车分配/维修筛选/部分卸载/防空/原版散开躲弹等已实现的纯任务 | 可选 Host/Runtime | 新配置默认 Host，保留已有 INI 选择 |
| Host planner 的 Advanced/Magnetron/Ore/Defense | Host 或 Runtime | 按活动 flags 请求帧，空配置不请求 |
| 外部渲染、场景和视觉补页 | Host 独立读取/worker | 不改 |
| 已封口样本的映射与页复制 | Runtime 发布工作线程 | 不改；PR #43 修复异常退出 |
| `CaptureGraph` 真实游戏内存读取、封口一致性复核 | 游戏消息线程 | **未迁移**；仍是高优先级剩余阻塞 |
| `indexWorld` 实时 UID 保护，最终事件合法性/归属验证、原生函数和命令下发 | 游戏线程 | 保留，跨线程不能直接调用 |
| `catalogue::capture` 类型元数据轮询/扫描、维修候选读取等 | 部分游戏线程 | 需独立读域/一份一致性证据后再迁移 |

不得仅因为 frame/serial 相同便将可能混合多个模拟时刻的后台 RPM 作为联机执行输入。未来采样拆分须做生产者身份、序号、世界/House/对象世代双重验证、异常退役、不能同帧补页的状态机，以及实时下令前轻量复核，保证未知读取不被当作对象消失。

## 验证与范围
`source/assist/tests/host_frame_demand.cpp` 使用生产头执行：无活跃功能的三种配置、4 类 planner、6 类自动任务、已有任务即使配置关掉仍唤醒，以及无关 UI flags 不应唤醒。CI `.github/workflows/host-first-offload.yml` 同时运行现有 Runtime 热路径和共享采样核心测试，配套 Windows x64 Host / x86 Runtime 成套编译和 ABI 核验。是否通过以对应 Actions 的真实结论为准；当前没有游戏/联机 FPS 证明。

此轮没有私有 EXE、反汇编、游戏函数地址、ABI 或字段偏移的新结论；沿用 INDEX 中既有支持样本范围。未将纯计算迁移的回归测试冒充 FPS 性能测试。若实际工作负载中 `shared->externalDisplay` 始终启用，保留原生帧通知是必要的，本轮不会宣称它已消失。
