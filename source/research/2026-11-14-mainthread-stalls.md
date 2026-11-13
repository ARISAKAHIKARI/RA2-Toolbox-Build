# 游戏线程采样与长回调修复（1.0.51-pre.3）

问题：用户报告 pre.2 刚进游戏逻辑帧下降，联机更明显；要求采样不拖住游戏并一并排查长阻塞。

## 证据与范围

用户提供 pre.2 日志，主要游戏进程 225 条有效记录：controlUs 中位数 3105、最大 6368；scanUs 中位数 3475、最大 7454；每次约 7520256 字节、997 次读取；sampleFailures 均为 0。session=3，renderReveal=2。日志不提交仓库。成功采样不等于耗时低；日志不含本轮修改后的实测结果。

这里只修改自有采集、显示与调度代码，没有新增引擎地址、调用 ABI 或反汇编结论。沿用支持样本与原生绘制研究见 [迷雾审计](2026-10-26-shroud-network-audit.md)。既有样本 EXE SHA-256 为 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；本轮未执行该 EXE。

## 修复

1. Runtime 的 `captureWorldFrame` 在游戏消息线程同步运行，旧的 Host Visual 需求和补页请求会放大 CaptureGraph 与 Server.publish 的读集和复制。新增 Host-local Resources/Visual 通道。Client 将这类订阅映射为 Runtime Base，显示补页租约只进入 Host VisualCache。实际控制功能仍可申请 Combat/Passengers/Ore/Bullets；每逻辑帧统一控制采集和失败恢复保持原有行为。
2. VisualCache 导入已封存控制页，保持这些字节不变；仅缺少的显示页通过外部 RPM 读取。读取前后检查场景、地图、House、会话、边界、活动状态和实时帧，实时帧须不早于控制帧且在本次读取间稳定。扩展快照有独立 visualFrame/visualTick/visualSerial，原控制帧、序号、时间戳不变。它是分来源的显示数据，不能解释为全体字段同一时刻的原子采样。
3. 控制 FrameScope 自动解开 controlSource；派生显示快照禁止通过 Runtime Server 发回控制通道。缺页不在读取中偷偷回退 live；只有下一次 Host 构建能补齐。派生缓存与异步完成使用独立观察身份，较早的显示修订不能覆盖较新的修订；显示子作用域的失败不污染控制父作用域。
4. Client 的显示采集锁与传输锁分开。无 SuspendThread、游戏等待 Host 结果或跨进程游戏锁。显示 worker 可以等待自己的任务，游戏发布和控制消费者不等待这把锁。
5. `halfShroud` 原像素循环每个有效像素的 put 都调用 GameEntryManager Validate；旧审计已量得 32×32 全有效遮罩 1027 次。本轮在每次绘制前校验权限，三个原生 helper 仍各自验证；像素混合用已验证范围的 halfSpan。调用数变为每次绘制 4 次。裁剪、环形 alpha 缓冲与 sentinel 混合保持逐字节一致，不写可见性/探索字段。
6. 私有队列原先一次可清除全部 4096 个失效项，改为每逻辑帧最多 128 次队首检查；有效队首和后续 FIFO 保留，不改变原生发包、flush、执行帧和重试提交边界。脚下攻击的无效成员循环增加每帧 64 次检查上限；发送额度和原截止时间保持。
7. 活动 tick 的计时起点移到入口，覆盖原先漏掉的维护及准入工作。它仍不代表所有原生绘图 hook 的耗时。

## 其余路径审查

- network sender 的原生 OutList 为 128 槽；未将其误认为 16384 槽 DoList，也未改动线上包编码或 flush 时机。
- 参数断点维护已有 500ms 审计间隔，普通 service.poll 不等待；本轮没有把恢复/关闭请求延迟到低频检查。
- 单机 FPS 的定时等待和单机暂停循环是配置功能，均先检查 session。联机不应进入这两条等待路径；保留原有设置效果。
- Host 的 worker join、frame clock 与 broker 等待不在游戏消息线程；没有为了消灭所有 Wait 字样而移除正常后台等待。
- 其他发令/乘客/磁电链遍历已有明确数量或队列限额。本轮不改游戏原生寻路、平台网络等待等未证实的候选原因。

## 验证

- `world_frame_core_test.py`：实际 Runtime/Transport/ProcessReader，新增 Host 显示隔离、控制来源恢复、派生 wire 拒绝；将一个 Host 读者阻塞时，另一线程仍能取得新发布的控制帧。512 个显示页只在 Host 读取；合成小场景控制读集维持 27 次/114688 字节/28 页。该数不是用户实局的节约比例。
- `mainthread_stall_test.py`：执行实际 halfShroud，36 组黑幕/迷雾、裁剪与环形边界逐像素对照；每次仅 4 次入口校验；4090 个失效队列项分帧清除，最后 6 个有效项保持原序。
- `attack_ground_policy.py`：63 项，新增无效成员分帧进度与同帧不重复检查。
- `world_frame_retention.py`、`tactical_frame_sampling.py`：缺页请求保持、失败恢复、自动功能统一控制快照、暂停补采和新鲜度。
- `shared_scene_sampling.cpp`、`world_ui_sampling.cpp`：共享显示解码与作用域；新增同控制帧的显示修订恢复、旧修订拒绝与父子来源隔离。
- `fps_read_only_recovery.py` 的夹具补齐既有 trigger/private 断点组的模型声明，16 项实际抽取函数检查继续执行；未替换产品函数。

Linux 合成测试与配套 Windows 构建、PE/包体校验分别记录在发布流水线。它们不提供真实联机 FPS、平台兼容或网络延迟结论；后续应直接比较同场景日志中的 controlUs/scanUs/samplePages/sampleBytes。
