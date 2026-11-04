# 平台实机已连接但可选入口导致全部命令停用

## 问题、实机证据与历史

用户报告只读正常、依赖 Hook 的功能全不工作，并指出此前平台兼容已修好。调查前重读 `INDEX.md` 与持久 SQLite v124 的 `platform5-live-signature-rejection`、`platform5-135-mrga-beacon-startup`、地图揭示及后台渲染发现记录。

本轮 `Runtime(6).log` SHA-256 `7a2e48ce5bbe688a055f8740393739472d2bd38fa844ee77f9ea3e8238cbac15`。v1.0.41-pre.3 段已连接 PID 40388，窗口来源为 YRpp `0xB73550`，callback TID 与 expected TID 一致；末段累计 3966 次回调、3949 次 dispatch，`status=1 match=1 runtimeError=0 faultSerial=0`。`commandStatus=3`、`gate=signature-mismatch` 才是本轮全套命令功能的阻断来源，不能继续解释为没有发现进程或 Hook 未进入。

反复出现的原字节差异：

| 入口 | 预期前八字节 | 实读前八字节 | E9 直接目标 |
| --- | --- | --- | --- |
| `0x431410` 清除信标 | `558BE98B456085C0` | `E9BB2849556085C0` | `0x558C3CD0` |
| `0x577D90` Map::Reveal | `8B44240453555733` | `E96B827E03555733` | `0x03D60000` |

目标按 x86 rel32 从日志指令计算；日志没有给出目标代码、模块/映射归属和 ABI，不能据此认定具体平台模块或授权新替换函数。Host 的 `startup-probe-difference` 只是原字节比较，不等于 Runtime 兼容分支拒绝；尤其不能仅凭此行判定旧 MRGA 信标兼容当前是否通过。`0x577D90` 没有替代兼容分支，已足以确定令全局校验失败。

21 项启动表没有近期新增：`signatures.h` 从最早公开源码 `50f59d03fda6c37ce2c35927565cabb372d8e94e`（v0.7.20）就包含两处入口，到本轮 main `51e62eec` 仍为 blob `02b874e952eba64304042837404543a6125303bd`。v1.0.35 修复 `ff5a76ada06efdd764d9c3ef13cd452b4d69ef14` 的真实采集是 20 项匹配，只有信标入口变化；当时 `0x577D90` 仍为原版。本轮观察超出那次采集证据。

## 依赖隔离

21 项表与 `startup_signature_compat.h` 原样保留。后者继续完整验证旧 MRGA 模块、PE32/大小、目标 RVA、可执行 MEM_IMAGE 归属、完整重定位归一化函数指纹及 ret 4 ABI。没有接受任意 E9，也没有删掉对两个原生入口的检查。

`startup_dependency_policy.h` 按实际消费者区分：其余 19 项作为核心 ABI 门槛，任一不匹配或不可读仍拒绝全局命令工作；两处可选入口改为在功能使用时强制检查。核心通过后全局 `good`/`commandStatus` 沿原有就绪路径发布，不再被这两个可选入口共同锁在 3。

- ClearBeacons 每次执行前继续使用原 `startupSignature(Signatures[8])`。原版和已有严格 MRGA 分支保留，未知入口拒绝，不调用清信标函数。失败码改为通用版本不兼容 `-12`，避免 Host 的 `-11` 科技生产文字误导。
- RevealMap 在 `crcMapToolBegin` 及 `0x577D90` 调用前逐次核对 `Signatures[9]`。失配返回 `-12`，既不进入 CRC 包装也不调用未知目标。
- 旧 `renderReveal==3` 直接全图组会放宽 SpySat 分支，并可由引擎到达 Map::Reveal。因此在安装前、普通维护时以及暂停/显示未知等挂起维护时检查同一依赖。失配只尝试回收该组仍由本工具拥有、字节仍匹配的 Map 站点；保留其他写入者的字节，发布该功能失败。挂起时若用户已切往其他模式，也立即尝试回收旧直接 Map 组；此路径不读取 Reveal 入口。挂起维护只回收，不安装。
- 单槽硬件地图参数、绘制遮罩、素材模式以及科技/控制/放置等没有因此增加对 Map::Reveal 的依赖，仍分别通过原有独立准入和恢复流程。

这是共享启动门槛的依赖修正；没有在本轮适配两个未知替换函数本身。其目标所属平台更新、其他工具改写或跳板语义仍未决。

## 映像、ABI 与验证范围

本轮日志没有附完整 EXE/模块身份，不能认定当前映像等同历史样本。历史随机名称样本 SHA-256 为 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`；本轮不增加新的支持映像指纹。

原清信标调用仍为 thiscall，ECX=`0x89C3B0`，栈参数为 House+`0x30` 所属索引。原地图揭示调用仍为 thiscall，ECX=`0x87F7E8`、一个 House 指针参数；未知替换的 ABI 未验证，故不执行。没有新增游戏字段偏移、游戏内存写入方式或注入入口。

`assist/tests/startup_dependencies.py` 的 portable fixture 抽取生产核心校验、命令分支与直接 Map 维护/恢复函数，配明确的 Win32/映像和补丁注册表替身。覆盖双未知入口、19 项逐项错误/不可读、逐次命令拒绝、CRC 零进入、直接 Map 零安装、启用后依赖变化与 owned/foreign 恢复边界；结果由同提交 `result.json` 和 CI 保存。旧完整 MRGA 指纹实现保持逐字不变，既有 `platform_startup_runtime.py` 是带私有旧采集的完整函数验证入口。

实机日志证明本轮已连接但启动拒绝；portable 语义测试和编译不等于新版本在用户平台的实际功能或联机验收。新两个替换目标仍需只读采集其内存归属、代码及 ABI 后才能添加精确兼容。
