# 手动快捷键的冷启动循环：版本 A/B 证据

## 结论与范围

确认一个 **1.0.43 之后新引入、当前 1.0.47-pre.4 仍存在** 的回归。首次引入提交为 `944586abf9cd331d2aaf9fe24af744a593bd31d3`（1.0.44-pre.2，`perf: share sealed world samples across Runtime and Host`）。共享世界采样改造使显示采样必须先拿到 Runtime 世界样本，但 Host 的自动连接判定仍会等待成功显示样本。特定配置下两者互相等待，普通手动快捷键无法完成注册，因而根本不能进入 `send()`。

这不是开箱默认配置：复现必须关闭所有持续 Runtime 需求，包括高级输入、自动修理、建造耗时提示等，并取消“攻击脚下”绑定；“自动装车”仍保留 `Ctrl+Alt+L`。默认的“攻击脚下”`Ctrl+Alt+F` 本身就计入 `runtimeFeatureDemand()`，会打破循环。

**这项回归不能直接解释上传日志里的已连接时段。** 该时段 `commandStatus=1`、`fresh=1`、`featureFlags=0x6028CC5C`，已经越过冷启动连接门槛。本复现不把这一新缺陷等同于用户现场的唯一原因，也不把它扩展为所有 UI 按钮失效：直接 UI `send()` 路径可以主动调用 `ensureBridge()`，没有“快捷键未注册”的前提。

## 实际生产代码中的循环

1. 1.0.43 `main.cpp:1252`：`sampleExternal()` 直接调用外部 sampler 并设置 `externalData`。只读内存健康时，无需 Runtime 就能获得 `externalData->status==1`。
2. 所有对照版本的 `needsBridge` 表达式保留 `runtimeFeatureDemand(...) || (externalData && externalData->status==1) || queue/selection/flash`。1.0.43 在第一个成功外读后即可调用 `ensureBridge()`。
3. `944586a` 的 `main.cpp:1264` 新增 `WorldFrameScope`；没有 Runtime 世界样本则在 `1265` 的分支直接返回。1.0.46.4 对应 `1292`，pre.4 对应 `1338`。
4. 冷启动且 `runtimeFeatureDemand()==false` 时，没有 Runtime → 没有世界样本 → `externalData` 为空 → `needsBridge==false` → 不调用 `ensureBridge()`。该状态每个 tick 重复。
5. 旧版快捷键模式需要显示就绪；pre.4 新模式需要命令连接就绪。冷循环中两者均不成立，因此 pre.4 虽修复了显示门控和按键消息恢复，仍不能从这个状态启动。

对应自动连接语句在 1.0.43 `main.cpp:2239`、1.0.44-pre.2 `2291`、1.0.46.4 `2319`、pre.4 `2379`。本次没有修改任何生产源码。

## 可执行 A/B 结果

完整提取并编译各版本的 `sampleExternal()`、`mergeExternal()`、命令身份、快捷键模式、`registerKey()`、`registerKeys()` 等生产函数，保留实际 tick 中“先更新注册，再采样，再判断连接”的顺序和原始 `needsBridge` 表达式。每个条件执行 12 个 tick；不是根据字符串存在与否宣布通过。

| 生产来源 | 手动专用配置：连接调用 | sampler 调用 | 最终热键模式 | AutoBoard 1011 注册 |
| --- | ---: | ---: | ---: | --- |
| 1.0.43 `bf3fbe8` | 1 | 12 | 2 | 成功 |
| 1.0.44-pre.2 `944586a` | 0 | 0 | 0 | 未注册 |
| 1.0.46.4 `f452d1c` | 0 | 0 | 0 | 未注册 |
| 1.0.47-pre.3 `93f0278` | 0 | 0 | 0 | 未注册 |
| 1.0.47-pre.4 `3cd2f47` | 0 | 0 | 0 | 未注册 |

后四版均有 12 次世界样本获取失败和 **零次连接尝试**，证明不是模拟连接器返回失败造成的结果。恢复默认 OwnGround 绑定、开启 AutoRepair、预先已有健康连接这三个阳性对照，在五个版本中都连接并注册成功。

总计五个版本、20 个场景、164 个检查。`passed=true` 表示该回归对照按预期重现；不表示产品缺陷已修复。

## 受控边界与独立复核

- Win32 前景/计时/注册、外部内存 sampler、IPC 世界样本可用性及成功连接是控制边界。设置为前台、启用、无聊天、只读游戏数据健康、注册一定成功。IPC 仅在 `ensureBridge()` 被调用后可用，并刻意让连接与首份发布立即成功，以排除模拟的连接延迟或错误。
- `flags()==0` 是 UI/设置边界输入：实际 `flags()` / `hookpolicy::resolve()` / `additionalFlags()` 的全部 opt-in 关闭可以达到；这不是默认设置。没有为输入枚举、单位合法性或游戏下令编造执行结果。
- 五个版本使用各自完整历史入口函数，但共用当前的支持头。`runtime_feature_demand.h`、`additional_settings.h` 与 1.0.43 原字节相同，脚本每次重跑均校验。Registry 版本不影响循环证明：失败发生在任何 `add()` 之前；阳性路径的 Win32 注册统一成功。未声称每版整产品重新编译。
- 历史主文件通过原始 Git blob SHA-1 与 SHA-256 双重校验。报告还绑定全部实际编译支持头、抽取函数、生成 C++、编译命令和运行日志的哈希。
- 没有执行真实 Windows 或游戏，没有证明这份日志的现场显示失败原因。只读独立复核由 `/root/hotkey_common_path` 完成，未发现阻断；其确认默认 OwnGround 例外、tick 顺序、直接 `send()` 的范围边界，以及 `flags()==0` 的可达性。

## 源码指纹

| 来源 | `main.cpp` 原始 SHA-256 |
| --- | --- |
| 1.0.43 | `60a194d16c3f6e124aa678f57dc33049b1b974b8c2212988546ee7d523538b16` |
| 1.0.44-pre.2 | `0000654ac95a49d364977931fa7b6edb995d9c7af069e2cd14f86627f73958d0` |
| 1.0.46.4 | `cb446fd28028f9b73379d4963dbd491e6aa2a5bc8d83166c45672fef99adc3df` |
| 1.0.47-pre.3 | `c824b03b81a7ee2cef824db5b04498dd4ca7c7935c956960ccc10ee0280b1b1f` |
| 1.0.47-pre.4 | `53c0dae3ece505449535dc481e9a37af7fa4cf7b057abd04a22b580210367f30` |

两端 `runtime_feature_demand.h` 均为 Git blob `38b5583aa66c9f34d4855e31adf3342a32e14f75`，原始 SHA-256 `f07e599773d3e33bad81e7ba68ab405aef7725e604a1763c3783a459ece08bf9`。默认 OwnGround 定义在冻结的 `sources/v1.0.43/additional_settings.h:18`。

## 重跑与下一步

```bash
python3 /workspace/scratch/c1b87d8550cd/hotkey-root-cause/command-history/cold_bootstrap.py --repo /workspace/scratch/c1b87d8550cd/ra2-hotkey-fix
```

原始结果为 `results/result.json`；每个版本目录保留完整 `production.cpp`、原始 `callsite.cpp`、编译日志和 JSON 行运行日志。`historical_sources.json` 与 `versions.json` 保存来源和冻结指纹。

修复方向应使连接需求包含当前实际存在的世界样本消费者或手动命令需求，不能继续拿“世界样本已经成功显示”作为启动该样本生产者的先决条件。该修复尚未实施；也不应以它替代对用户已连接时段的调查。
