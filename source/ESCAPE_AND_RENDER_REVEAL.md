# 1.0.6 逃生与地图绘制测试

## 逃生问题

参考 YRpp `Sequence` 枚举，并核对当前参考 EXE：Infantry 虚表 `0x7EB058 + 0x554` 指向 `0x5228D0`，原版 `IsDeployed()` 对序列 27–30 返回真。其中 27 是 Deploy 动画；28–30 才是 Deployed / DeployedFire / DeployedIdle。原实现见到 true 就清除跟踪，因此部署动画被原版散开移动打断后不会再补命令。这是已确认的代码缺陷；未在用户现场运行游戏。

`CanDeploySlashUnload`（虚表 +0x314）是原版能力判断，不能当作“已站稳格子”的证据。首次尝试仍立即按其结果构造原版 Deploy；不把五个乘员串成同格等待队列。实际部署完成需观察 28–30 连续稳定两帧且无 Move/排队 Move。序列 27 或已排队的 Deploy 不重发。看到执行后又中断，保留原版散开移动；该乘员稳定两帧、距上次发送至少 12 帧后重试。每个弹出乘员最多 12 次尝试、整体仍有 900 帧超时和原有 180 帧散开容忍窗口。手动取消、死亡、身份/归属变化、开关关闭终止；关闭只为确实处于 Sleep 的遗留任务补 Guard，不打断仍在散开的乘员。

所有部署继续经现有 nativeMission → 统一事件网关 → 原版构造器 → 单机 DoList / 多人 OutList；不写任务、位置、动画或移动器，不增加帧/输入 Hook。智能卸载入口仍停用。

## 地图绘制测试

新 `[Experimental] RenderReveal=0`，自动保存。与旧 RevealMap 许可互斥；Runtime 亦拒绝在 renderReveal 开启时执行旧 RevealMap 命令。旧功能已经揭示的视野不能撤回，测试应使用新对局。

| 位置 | 原始字节 | 新路径 |
|---|---|---|
| `0x47EFE0`，遮罩绘制叶函数，6 字节 | `55 8B EC 83 EC 34` | 取得 GameRuntime Capture 租约并复核状态后，跳过本次遮罩绘制；不符合条件执行原始指令 |
| `0x47F250`，迷雾绘制叶函数，5 字节 | `A0 C6 E7 89 00` | 同上 |

`0x4801F0` 原调用者仍运行两次 `TacticalClass::GetOcclusion` 及 `Cell.Visibility/Foggedness` 原有缓存更新。没有替换全局 GetOcclusion、IsShrouded，没有写全图 Cell/House 标记、揭示计数、观察者状态，没有调用原版 Reveal。开关改变时仅请求一次原版 `TacticalClass::RegisterDirtyArea`（`0x6D2790`）重新绘制当前区域；不逐帧扫描全地图或 RegisterCellAsVisible。

安装与恢复仍只能由原消息入口经 `ManageGameEntry` 执行：白名单、准确原始字节、目标属于本 Runtime 映像、绑定线程和所有权检查。两个位置成组启用，安装失败回滚；外国改写保留为冲突，不覆盖。叶回调不消费任务队列或构造事件，保留寄存器、标志和 x87/SSE。关闭/失联/错误线程时回落原分支；生命周期清理恢复自身位置。

范围：去除黑幕/迷雾绘制的测试实现。原版对象绘制、鼠标目标判断、视野规则及雷达仍有限制，配合现有只读敌方标记查看对象。未宣称完整原生全图、LBW 全兼容或联机安全。真正画面效果、缓存重绘、实际开销和双端同步仍需 Windows 实机验证。

## 验证

- 更新旧 x86 回归夹具的协议布局和命名，并在模拟执行前强制比较实际 DLL 导出 Shared 大小，避免旧测试结构静默错位。
- 逃生回归执行本版实际 DLL 与原版事件构造器；覆盖五乘员同格、部分部署/移动、动画被打断、队列去重、关闭、取消和散开超时。
- 8 项实际绘制入口桩 ABI 用例；18 项原版 Cell 调用者对比，在 GetOcclusion/最终绘图模拟下，开关前后的完整 Cell/House 字节一致。
- 7 项实际 Runtime 安装/回落/恢复用例，覆盖失联、错误线程、关闭、安装前及安装后第三方修改。
- 12 项已有任意建造/原生路径点入口桩回归、五位置白名单/恢复/冲突测试、PE32/ABI/依赖与源码边界检查。

上述并非 Windows 加载器、真实绘图、屏幕捕获、联网或性能实测。源码中的旧报告保留各自版本，本版结果以 packages/v1.0.6 为准。

依据：[YRpp InfantryClass](https://github.com/Phobos-developers/YRpp/blob/master/InfantryClass.h)、[Sequence 枚举](https://github.com/Phobos-developers/YRpp/blob/master/GeneralDefinitions.h)、[TacticalClass](https://github.com/Phobos-developers/YRpp/blob/master/TacticalClass.h)，并核对本地参考 EXE。未把反编译地址当作跨 MOD 保证。
