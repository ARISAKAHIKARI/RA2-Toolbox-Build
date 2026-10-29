# 滚动触发的控制台失效与渲染耗时分项

用户补充：五个大分区可以切换，但所有内容页点击滚动条／滚轮后，滚动和具体按钮失效。这个触发与此前退出等待环、隐藏模态框并不等价；没有现场线程栈，不能宣称已确认唯一根因。

## 控制台调整

源码中所有内容页共用 `frontend_panes.h`：每次滚动／按钮通知后投递高优先级 `SnapshotMessage`，随后同线程同步 `WM_PRINT(PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN)` 遍历整个页面。显示布局也必须等待抓图完成才能发布 applied 状态，因而抓图工作会直接延迟真实控件的展示。

本次把抓图改为最后一次编辑之后 120 ms 的低优先级 WM_TIMER；有鼠标捕获、移动／菜单模态循环、GUI 线程状态读取失败时继续推迟。显示完成不再以抓图完成为前提，隐藏时也不抓图；销毁清理定时器，已排队的过期 timer 不重复抓图。120 ms 只影响拖动主窗口时使用的静态预览，不延迟按钮或滚动操作。本次是针对已知公共同步工作路径的修正，不等于已复现并证明用户机器的全部失效原因。

`assist/tests/frontend_snapshot_test.py` 执行生产 dispatcher／requestSnapshot，以受控 Win32 边界验证 1,000 次滚动期间不进入抓图，按钮仍分发，捕获／菜单／移动结束后恢复一次抓图，定时器失败与销毁正常。旧 HEAD 负例应在有捕获时错误进入 capture。`frontend_panes_windows.cpp` 增加真实窗口、鼠标捕获与 201 次输入的案例；仅交叉编译不代表实际 Windows 执行。

## 帧率调查与测量范围

`render_upgrade_host.h` 的同一 worker 串行做读取／状态采样、场景构建、GPU 计算／回读、原生反馈和可选神经处理。新逻辑帧才重新采样对象；高素材刷新开启时请求目标受 informationFPS 约束，关闭时 refreshPeriod 上限 60 Hz。请求队列只保留正在执行的一项和最新待处理项，耗时超过预算时中间请求会合并，目标刷新率不是新帧率。

新增已完成帧的采样、场景、后处理耗时与观测间隔。Vulkan 增加 fence 等待与回读整理的 CPU 墙钟耗时，后者包括 mapped-memory invalidate、拷贝与可选 guide 解包；不是纯 PCIe 传输时间，前者也不是 GPU timestamp 测量。二者包含在场景时间中，不能重复相加。本包没有移除 GPU→CPU 回读，也没有完成 GPU 直接呈现。

已复核的公开对照是 CnC-DDraw 提交 `279a057ee7e1e4d584b56141c014ab01f3c6ee6d` 的 `src/render_d3d9.c`：surface／palette 分别消费更新标志，持久纹理上传后用 quad 和 Present 直接显示游戏已绘好的表面；并不重新读取和重建所有游戏对象。详见既有 `2026-10-29-ddraw-gpu-reference.md`。这个结构差异说明仅增大采样频率不够；没有现场测量，不能断言回读或采样中哪一项占比最大。

## 基地车展开边界

已检索 INDEX、建筑部件／原生建筑笔记及持久 SQLite v85（104 条）。现有代码已有 BState／ActuallyPlacedOnMap 选择 make art、独立缓存 variant、buildingFrame 处理的通用路径；Hotfix2 没有针对用户本次基地车展开问题做专项修复。此次未分析新私有函数或修改游戏偏移，也未修复／验证这一实机场景，保持待查。
