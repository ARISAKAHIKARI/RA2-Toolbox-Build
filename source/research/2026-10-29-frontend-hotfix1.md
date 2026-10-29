# pre.7 前端补查：清单依赖、队形手势与窗口退役

## 问题与证据边界

用户要求继续修复，并询问 CI 的含义。本次补查对象为工具箱设置和既有队形输入状态；没有新增游戏样本、函数地址、ABI、字段偏移或游戏代码段写入。支持样本 SHA-256 不适用，不能用工具箱源代码审计代替某个游戏二进制的行为验证。

pre.7 已交付包不因工作区修改而自动更新。真实 Windows 桌面拖动、实际游戏操作、NVIDIA 推理性能和多人联机均未在本次验证。

## 类型清单的父开关依赖

补查发现，原策略覆盖复选框和快捷键等子项，但另有 19 个类型清单文本输入框缺少相应“类型清单限制”开关的约束。关闭限制后，这些输入框仍可编辑，与其他依赖控件的行为不一致。这里的父项是该清单自己的 `custom` 限制开关，不能误写成整个功能的总开关。

`assist/source/settings_dependencies.h` 将文本框 ID `9201+2*feature` 加入现有依赖适配器，并通过 `typeListNeed(feature)` 为每个清单分配独立位；`ui_dependencies.h` 从 `featureTypes.filters[f].custom` 发布状态。总依赖位数为 53，并以 `static_assert` 约束不超过 64。关闭限制时禁用相关清单，保留已保存内容并说明原因；重新开启限制后恢复编辑。清单编辑的合成通知经过相同检查，不能只依靠控件灰色外观。

20 个类型槽中的 `BurstRefresh`（f=17）为保留共享 ABI 的退役槽，`tactical_controls.h` 不创建该控件，因此不加入 19 项清单依赖。`assist/tests/settings_dependencies.cpp` 对 19 个清单分别验证关闭、开启、互不串联及退役槽排除。

## 队形双击状态的上下文失效

`assist/source/formation_host.h` 的 `formationHotkey()` 原来拥有函数内静态 `typekeys::Tap`。`hotkey_host.h` 的 `serviceHotkeyBatch()` 在失焦／聊天／暂停等分支只清通用 `typeTap`、`variantTap` 和注册表，无法清除这份独立状态。第一次按 X 后，在 350 ms 内切走焦点、进入聊天、关闭重开队形预览或修改设置，再按 X，旧第一次按键可能污染下一次双击识别。

现将 `formationTap` 与其他输入状态统一声明在 `main.cpp`，在失焦／聊天／暂停、无共享对象或命令连接过期、预览关闭、快捷键重注册、`settingsRevision` 变化、功能禁用和非匹配按键时清理。连接新鲜度检查放在 `formationInputOwns()` 早退之前，避免队形持有输入时跳过断连清理；同一局短暂失联后恢复也必须从新的第一次按键开始，不继承旧上下文的半个双击。

`assist/tests/hotkey_host_test.py` 和 `formation_switch_test.py` 增加生产路径断言；用修改前的两个头文件替换回归时，新断言分别失败，修复后通过，既有 27 个队形桥接受控用例保持通过。另验证保持队形预览与输入所有权时，命令连接过期再在同局恢复的清理。这是源代码路由与受控状态检查，不能声称用户游戏内的所有输入时序已验收。

## 设置窗口销毁与迟到通知

`suite/source/main.cpp` 原来另行保存 `panes[]` 与 `displayPanes[]`，而 `frontendpanes::Host` 在 `WM_NCDESTROY` 只清自身窗口指针。模块退出后，宿主可能继续向旧 HWND 发送布局、显示设置或关闭消息；若句柄被别的窗口复用，`IsWindow()` 也不能证明它仍是原设置窗口。此外，宿主等待关闭确认时，如果负责确认的模块已经退出，原 `closeAsked` 状态会使关闭流程继续等待。

现删除两份冗余 HWND 缓存，统一通过 Host 的活动窗口槽查询和投递；`Host::post()`、布局消息发布与 `WM_NCDESTROY` 退役以槽互斥锁协调。退役同时清除待处理状态和快照，通知宿主重绘；迟到的 `PaneReady` 必须仍匹配活动槽。等待关闭确认期间，只要负责模块线程已结束，就继续关闭其他模块，而不是等待不再可能发出的确认。

后续历史澄清：正式冻结的 Hotfix1 本地提交 `9fcc585dbbfa3247fe89b1682ba54367f3d0902f` 中，`Host::cloak()` 确实持槽 mutex 调用外部 `DwmSetWindowAttribute`，不能将该已交付代码描述成 DWM 外调用无锁。Hotfix2 将此外调用移出锁，消息投递与退役仍互斥；具体回调边界检查见[渲染与退出补查](2026-10-29-render-hotfix2.md)。此澄清不改写既有第 103 条 notebook 记录，纠正附加到下一条研究记录。

`assist/tests/frontend_panes_windows.cpp` 增加窗口销毁后活动槽、快照、消息投递与重绘通知断言；`suite_display_test.py` 检查生产消息路径与关闭分支。Windows 目标的双架构 fixture 链接和 suite 对象编译均通过，共 4 项，记录为 `frontend-followup-private/drag/compile-result.json`；这些二进制未在 Windows 执行，因此不宣称实际窗口句柄复用或桌面平滑度已经现场验证。

## CI 只读复核

CI 指 GitHub 自动执行的编译与测试。复核四个最近产品／发布 run：`40846843744`、`40846843893`、`40846843761`、`40873088420`，最新共 9 个任务均没有执行步骤，`runner_id` 为 0 或 null；前端 run 已是 attempt 3。发布后没有新增 run，本次未触发重试。

三份产品 YAML 正常解析，预期 8 个产品任务均已创建；未发现可证实解释此次启动前失败的 workflow 语法或运行器标签错误。正常解析不等于完整 actionlint 或执行成功。插件不提供所需检查注释／单任务页面，底层原因仍未知，不能判断是计费、额度、GitHub 故障或代码编译错误。

只读证据：`frontend-fix-private/ci-followup/readonly-20261029.json`。它是诊断记录，不能作为 CI 通过凭据。继续用本地真实编译与针对性检查验证修复；明确区分交叉编译、受控 Windows 执行和实机游戏验收。

## 最终验证与未决项

本地 17 项针对性检查全部通过，记录为 `frontend-hotfix1-private/focused/results.json`。覆盖手势状态机、生产热键路由、队形桥接、19 个清单依赖、配置保存、滚轮保护和 UI 几何；既有滚轮行为保持。双架构窗口 fixture 和 suite 对象 4 项交叉编译检查通过，未执行 Windows 控件 fixture 或真实游戏。

最终包内 `Build-info.json` 记录冻结后的唯一源码提交、本地实际编译与 PE 校验结果。不能用本研究笔记代替打包后身份核对，也不把已有 pre.7 下载包描述为已经包含本次修复。CI 的启动前失败原因、用户桌面的实际交互观感与真实游戏操作继续保留为未决项。
