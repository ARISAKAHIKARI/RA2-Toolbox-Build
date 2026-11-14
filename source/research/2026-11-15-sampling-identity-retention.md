# Ctrl+D 偶发失效：部分样本不能充当空单位索引

用户反馈：pre.8 的 Ctrl+D 仍有概率无效，怀疑采样；要求样本稳定准确作为各功能基础。本轮沿用原有字段/地址，没有新增游戏逆向或实机日志。已查 INDEX 与持久 SQLite v187 中共享采样、依赖续租研究。历史支持样本 SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，本轮没有读取/执行该 EXE。

地址仍为原有 Techno vector `0xA8EC78`、对象 UID `+0x10`、步兵 vector `0xA83DE8` 与载具链接 `+0x11C`，未改其 ABI。

## 已复现的源码路径

基线 `418347c5d753f5acb72026fcaf3c45935a65769d`（pre.8）：Snapshot 可携带逐页失败标志后 seal，seal 不是任意消费者全部字段已齐的证明。`SampleReadScope` 对单位 UID 页读取失败会标记 incomplete。Dispatcher 在此时清空 `worldObjects` 并设 `indexedFrame=-1`，但继续执行自动功能。`smartUnloadAdmissionTick` 把载具不在索引视为失效，`smartUnloadTick` 把乘员不在索引视为结束，`observeUnloadRoster` 也会删除近期乘员历史。这把“未知”误当成“单位消失”，可能丢掉已受理的任务。

这是一条已复现的可能失效路径，不是对用户每次现场失败的唯一诊断。按键没有送达、原生卸载先于工具箱处理、最终事件拒绝等仍应分别依据现场回执/日志判定。

## 修正

- `refreshWorldIndex` 优先从同帧封口样本建立索引，检查完整性后退出读取作用域。若有用户命令或现存执行任务，样本缺失才在作用域外重新建立实时身份验证索引；不再把缺页造成的空索引交给任务清理。这里重读的是执行身份，不重做渲染/素材/全场规划采样。
- 实时身份核验自身读取失败也保持未知：完整读到 vector header、整个 pointer span 与全部 UID 后才发布索引；失败则分发器保留请求/任务/历史并返回，下次唤醒重试，同帧失败至少间隔16ms。有效完整索引是版本清理的依据，不在清理时再作可能失败的 UID 回读。
- 纯观察不追加实时扫描，等待下一份完整样本。`observeUnloadRoster` 在索引不属于当前帧时保留历史，恢复后仍检查原有 UID、手动令牌和 1500ms/90 帧有效期；没有延长样本有效期。
- `indexWorld` 在同帧样本不完整时不执行手动版本清理，防止先删掉有效 token 再补身份；丢弃不完整索引时同时清除全局磁电候选，禁止用部分候选启动新任务。
- SmartUnload 开启时订阅 Passengers；卸载待准入/已有乘员任务时订阅 Combat，减少冷依赖逐帧发现造成的缺页。没有通过提高采样唤醒频率解决。
- 已有 `SampleReadScope`、原始帧/比赛身份、消费者缺页标志、请求续租、失败补采和下令前实时核验继续有效；不在同一封口样本混入新读到的字节，不把旧样本时间戳刷新为当前时间。

## 验证

`source/assist/tests/tactical_frame_sampling.py --negative-control` 提取实际生产 `indexWorld`/`refreshWorldIndex` 和 SampleReadScope，在真实 Snapshot 上故意缺失一个 UID 页：pre.8 实际 sampled 分支失败，当前执行身份恢复通过；纯观察不回读，下一完整帧恢复。另以 pre.8 实际 indexWorld 函数配合当前恢复入口，复现部分 UID 页导致提前清理手动版本；当前版本保留相同 revision。负对照提取分支 SHA-256 `b36f15b0a5f63d833d87c2718badef4a2937c42fc1f0baf654244ba410522a7c`。

附加注入实时 header、pointer span、UID 失败，验证索引不发布、手动 revision 保留、同帧有界重试和恢复；单次 pointer span 批读替代逐项实时取指针。

同一测试继续检查卸载原始事实与原生合法性查询拆分、缺目标页不发布半份输入、源 UID/手动命令验证，以及装车/躲弹/规划/维修/倒矿/防蜘蛛采样。`unload_loaded_admission.py` 保留 pre.7 失败对照。`world_frame_retention.py` 覆盖失败补采、传输繁忙、完整发布保留、暂停需求和订阅续租；`world_frame_core_test.py` 检查共享读集与 Host 传输隔离。这些是合成内存/受控 I/O 检查，不是实机或联网验收。
