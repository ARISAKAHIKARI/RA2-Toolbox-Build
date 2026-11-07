# pre.13：设置保存误报权限与队列恢复

## 问题与证据边界

用户报告统一控制台显示“设置保存失败，请检查文件夹写入权限”，目标为 Downloads 下既有安装目录中的 Assist.ini。目录名称含 pre.10 不能证明当前 EXE 版本，因为覆盖更新不会重命名目录。用户未提供实际 Windows 错误码、文件属性、ACL 或占用进程。

本轮基线为 `76b28dc4bb3ca2fb2e1a8769fefe6ba548554e17`（pre.12）。这是工具箱自身配置服务调查，没有新增游戏映像、引擎地址、ABI、字段偏移或游戏写入结论；持久逆向笔记维持 v154。本轮结果记录在本文件和同提交验证证据。

## 确定缺陷与最小修复

`modern_console_main.h::publishShell()` 原来只发送 pending/busy/failed 和固定权限文案，丢掉了 Store 已有的错误码。内部队列拒收、缓存满、服务停止和真实 Win32 错误全部呈现为文件夹权限问题。因此这条原始提示不能识别现场根因。

`saveAdditionalSettings()` 每轮都会删除旧的 `[Experimental] RevealMap`。`asyncini::Store` 为保留操作顺序，把删除视为全局合并屏障；完整保存的相同设置键因此不能跨保存轮次合并。写入速度低于连续设置变更时，4096 项队列会持续增长并拒收新操作；其内部错误码 8 又被旧 UI 误报为权限。

修复移除每轮的旧键删除，保留已有 `ManualReveal` 写入及加载优先级：存在新键时读取新键，否则仍读取旧 RevealMap。没有把删除移到启动阶段，避免新键写入失败而旧键随后被删造成设置损失。真实删除与 Flush 的顺序屏障保持，队列容量保持不变。

`Status` 新增失败来源（Native/Queue/Cache/Shutdown）、操作类型、section/key，保留错误码与路径，不公开配置值。特别区分内部容量错误 8 与 Windows 原生错误 8。

控制台展示实际错误来源、码、文件和失败项，并提供“重试保存”。重试只重放 Store 保有的最新期望设置，失败状态保持到相应操作成功；不调用 acknowledgeError 冒充已保存。运行期和关闭保存阶段可重试，真正清理阶段拒绝新请求。前端在恢复成功后重置重复错误提醒状态。

## 受控反例

`source/suite/tests/assist_save_queue_test.py` 从真实保存源码抽取固定 section/key 操作，并使用实际生产 Store。它以受控 gate 暂停写入器，再连续执行 25 轮保存；这种 gate 表示写入落后，不模拟用户 Windows 文件系统。

| 路径 | 每轮固定操作 | 待处理量／拒绝结果 |
| --- | ---: | --- |
| 旧 `76b28dc4` | 200（含重复删除） | 默认 4096 上限从第 21 轮开始拒收；共 315 次拒绝；排空仍保持失败，显式重试可恢复 |
| 当前生产路径 | 199 | 相同键合并为 199 项；0 次拒绝；排空后无失败 |

同一验证保留旧 RevealMap 兼容回退与新 ManualReveal 优先级检查。`async_ini_store_test.cpp` 覆盖原生错误 32、原生错误 8、内部 Queue/Cache 错误 8、Shutdown 995 的来源与字段，保持已有失败意图、顺序及重试回归。既有 console_model_test.cpp 与 console_store_test.mjs 均实际执行通过，覆盖严格重试消息、回执、失败/草稿保留及退出阶段边界；不计为浏览器呈现或 Windows 写盘验收。

原始证据路径：`analysis/pre13-save-regression/receipt.json`、`analysis/pre13-save-store/receipt.json` 及 `analysis/pre13-save-ui/receipt.json`。聚合结果为 `source/research/evidence/settings-save-recovery-release-validation-20261107.json`；构建与发布清单另绑定冻结提交及实际 PE/ABI。

## 已排除或尚未证明的解释

- pre.10 与 pre.12 Full 包均不包含 Assist.ini 成员；首次创建使用 CREATE_NEW 和 FILE_ATTRIBUTE_NORMAL，没有证据表明压缩包把该 INI 以只读属性分发。
- 三个 NULL 参数的 profile cache flush 特殊零返回已在基线处理，本轮没有把它重复认定为缺陷。Microsoft 文档区分普通写入失败与刷新缓存的零返回：[WritePrivateProfileStringW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw)。
- 未将所有失败当作成功，未修改 ACL、自动提升权限、删除配置或悄悄切换配置目录。
- 原生整节 API 路径仍保留；官方描述其单次操作为原子操作，不能只因多个模块写同一 INI 就断言存在写坏文件的竞争：[WritePrivateProfileSectionW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilesectionw)。
- 受控反例证明程序自身存在可触发的队列缺陷，不证明用户这一次必然由此造成。真实只读、ACL、文件占用或磁盘错误仍须新版实际错误码区分。
- Linux 受控队列与前端执行，以及 Windows 目标编译，都不是用户 Windows 文件系统、平台或游戏运行的验收。
