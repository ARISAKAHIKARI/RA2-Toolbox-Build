# pre.12：光标物理坐标、DPI 作用域与取点失败恢复

日期：2026-11-07 UTC。审计基线：`c092a70e160f3eb223eab4f3fa1049b9baead463`（pre.11）。

问题：用户报告在 4K 屏幕运行 2K 游戏时，光标位置没有随显示大小正确对应。本轮没有补充截图、当前桌面显示模式、Windows 缩放比例、包装器配置或现场坐标日志。因此能够确认源码中的坐标域约束和失败处理缺陷，**尚不能确定用户现场属于 DPI 虚拟化、独占全屏切换、包装器 viewport、手动覆盖值或其他原因**。

## 检索、样本与证据范围

调查前检索了本仓库 INDEX、[实验性视角缩放](2026-10-23-experimental-view-zoom.md)、[铁幕圆心与显示锚点](2026-10-24-display-anchor-137-pre14.md)、[CnC-DDraw GPU 参考](2026-10-29-ddraw-gpu-reference.md)和[隐藏光标与提示框布局](2026-11-02-cursor-hide-tooltip-layout.md)。同时只读检索同一持久笔记 `RA2-Reverse-Engineering-Notes.sqlite` v153，物化文件 SHA-256 为 `1173b5304959fb9501c2cc97818b7243444514c6544d10244f2840f5c4010db1`。检索包括 DPI、CnC-DDraw、cursor position、coordinate、光标和坐标。

- 本轮未取得新的用户游戏映像，未对用户当前 EXE 做反汇编或受控执行；**没有新增受支持 EXE SHA-256**。历史笔记中的样本指纹不构成本轮对现场程序的验证。
- 没有新增原生函数地址、ABI、游戏字段偏移、代码补丁或注入入口。下文出现的地址仅用于说明既有生产接口，不是新的逆向结论。
- 证据来自固定提交的 Toolbox 源码、固定提交的 CnC-DDraw 上游源码和 Microsoft 官方 API 文档。数学反例与测试设计不等于 Windows 实机复现。
- 本笔记不包含私有 EXE、原版函数字节或内存转储。生产坐标受控测试结果见本文末节；冻结提交的完整构建与归档摘要由交付清单和持久笔记另行绑定。

## 四种坐标不能混用

| 坐标域 | 本轮核对的含义 | 正确使用边界 |
| --- | --- | --- |
| Windows 物理屏幕坐标 | 当前桌面显示模式下的屏幕像素；多屏原点可为负 | 外置覆盖层的位置和光标热点位置使用同一域；不再乘游戏分辨率比 |
| 客户区坐标及其屏幕矩形 | 从目标 HWND 的客户区到屏幕的转换，受调用线程 DPI 上下文影响 | 在明确的同一 DPI 作用域内取得客户区两角并转换；宽高由转换后的两角相减 |
| 游戏逻辑画布坐标 | 游戏当前绘制宽高，可能被 CnC-DDraw 拉伸、居中或装入 viewport | 游戏内部命中、侧栏和世界投影需要明确的逻辑到呈现变换；不直接作为外置覆盖层的屏幕位置 |
| 光标素材画布与热点 | 原始完整 SHA 画布内的热点，以及最终缩放后热点 | 图像尺寸和热点按相同规则缩放；最终左上角为屏幕热点减去缩放后热点 |

Microsoft 明确说明 PMv2 应用看到显示器的原始像素；`GetCursorPos` 使用调用线程的 DPI 逻辑坐标域，`GetPhysicalCursorPos` 明确返回物理屏幕坐标。[官方 DPI 说明][ms-dpi]、[光标概述][ms-cursors]。这两种 API 的区别并不意味着所有 PMv2 调用都要再缩放：正常 PMv2 域下两点应一致。

一个无需游戏映像的算术反例是：当前桌面为 3840×2160，游戏逻辑画布为 2560×1440，Windows 物理鼠标点为 `(1920,1080)`。外置光标的热点应保持在该点；若把屏幕点再乘 `3840/2560=1.5`，热点会变成 `(2880,1620)`。这证明“按 4K/2K 比例乘屏幕点”会破坏正常坐标，**不证明现场已经经过某一错误公式**。如果游戏独占全屏改变了当前显示模式，也不能把面板标称 4K 当作当前 Win32 桌面宽高。

## pre.11 源码能证明的缺陷

### 取点失败仍继续消费输出

基线 `source/cursor/source/main.cpp` 的显示更新路径使用 `POINT pos; GetCursorPos(&pos);`，没有初始化 `pos`，也没有检查返回值。随后直接用它进行客户区内外判断，并计算 `UpdateLayeredWindow` 和覆盖层放置位置。官方 API 明确允许失败并返回零；失败后不能把输出当作有效点。[GetCursorPos][ms-getcursor]

同一文件还有两处同类保留代码：`pollPhysicalClick()` 虽把 `POINT p{}` 初始化为零，仍忽略取点失败，可能将零点当作真实采样；`clickTick()` 的重发检查使用未初始化的 `POINT p` 并忽略失败。**基线主循环没有调用 `clickTick()`，`pollPhysicalClick()` 也只被它调用，两者当前是 dormant 路径**，不能当作现场正在运行的点击根因。由此能够证明失败处理不完整，不能据此断言已经发生随机移动、错误点击或用户报告的偏位。

最小受控反例应让取点 API 返回 `FALSE` 且不写输出，检查生产调用链停止使用该点；点击路径还应取消余下 burst，并同步本次实际按键边沿，避免恢复时把旧边沿补成新操作。这是明确可测试的错误条件；**实际取点失败反例已由生产代码抽取夹具执行；这仍不是用户现场的 Windows 视觉复现**。

### 正常缩放链并未遗漏整个屏幕位置的倍率

基线 `cursorScale()` 计算的是图像尺寸，`superhot::scaledAnchor()` 用相同目标尺寸处理热点。外置路径使用 `pos - scaledHotspot` 放置图像；原生 HCURSOR 路径把相同的缩放后尺寸和热点交给 Runtime。屏幕位置本身不需要随图像尺寸再乘倍率。

`gameW/HOverride`、`screenW/HOverride`、`dpiOverride` 和 `dpiMode` 参与图像大小计算，不能成为改变鼠标屏幕位置的依据。现有 `refreshClickRegion()` 则把逻辑侧栏区域映射到客户区，并有 aspect-fit 分支；其结果是否覆盖特定包装器的整数缩放、黑边或裁切，仍需要真实呈现矩形验证。

### `draw()` 的命令光标区域忽略了已配置的 aspect-fit 黑边

基线 `draw()` 的 `mapPoint` 首先以整个客户区判断是否在游戏内，之后只用 `x * gameWidth / clientWidth` 排除右侧侧栏；没有使用 `aspectFit`，没有计算呈现画布的居中偏移，也没有排除上下或左右黑边。该布尔值参与 `commandcursor::read()` 提示的启用判定：还需存在命令模式、没有菜单暂停、符合普通光标/进楼光标规则且绑定按键仍按下，才会显示相应命令光标。它决定的是提示外观准入，不是一次实际游戏命令已经执行。

这是与现有 aspect-fit 设定不一致的确定源码缺陷。最小几何反例：客户区原点 `(0,0)`、尺寸 1920×1080，逻辑画布 1024×768，保持比例时实际画布宽 1440、左右各留 240。点 `(100,100)` 位于左黑边，旧 `mapPoint` 仍为真；当上述其他条件满足时，命令光标可被错误放行。侧栏边界也应从同一呈现变换投影，不能使用整客户区横向拉伸的阈值。该反例证明判定公式缺少黑边处理，不证明用户当前配置就是此比例或真实渲染器一定按本工具的 aspect-fit 偏好呈现。

## 本轮修复边界

### 明确物理取点，并把几何和输出放在同一个 DPI 作用域

独立 Cursor 和统一 Host 的 manifest 已声明 `PerMonitorV2,PerMonitor`。Assist、UnitMonitor 的旧入口仍有 `SetProcessDPIAware()` 调用，但不能凭这一调用断言已由 manifest 建立的 PMv2 被成功降级；也不能仅凭 manifest 推断所有回调实际运行时的线程上下文。[官方 DPI 说明][ms-dpi]

本轮将外置光标的采样和几何边界收敛为以下合同：

1. 优先通过 `GetPhysicalCursorPos` 取得物理点，显式检查成功状态。点、客户区矩形和最终覆盖层位置必须处于可确认的同一域。[GetPhysicalCursorPos][ms-physical]
2. 在局部 DPI 作用域内取得目标客户区两角并转换为屏幕坐标，保留负坐标、非零窗口原点和各轴实际长度；不要混用虚拟化宽高与物理原点。每个 API 的失败都应传播为本次采样不可用。[ClientToScreen][ms-client]
3. 作用域覆盖需要使用该几何的窗口创建或放置操作，离开作用域时恢复进入前的线程上下文，包括提前返回和异常路径。`SetThreadDpiAwarenessContext` 返回旧上下文，失败返回空；可用性和失败必须处理。[SetThreadDpiAwarenessContext][ms-thread]
4. 若无法建立或转换到可确认的同一域，应报告取点/几何不可用并停止该次依赖操作；不得用 `(0,0)`、未初始化点、猜测的 `×1.5` 或陈旧坐标冒充成功。若保留兼容回退，必须单独说明其坐标域和判定条件。
5. 失败采样不驱动新的合成点击，已有待发 burst 应取消。恢复后的实体按键边沿不能重放为额外点击。对当前 dormant 的保留路径，此项是清理其错误处理合同，不能表述为修复了主循环实际发出的重复点击。光标的可见性退让仍遵守既有所有权和清理路径。
6. 命令光标区域与侧栏区域共用显式的逻辑画布到呈现矩形变换：`aspectFit` 开启时使用较小轴比例并居中，排除黑边；关闭时按各轴比例拉伸。这个变换用于区域归属，不用于再次缩放已经取得的物理鼠标点；它也不等于自动侦测任意平台的真实 viewport。

这不要求修改游戏或平台的 DPI 模式，也不需要 `SetCursorPos` / `SetPhysicalCursorPos`。后两者会改变真实鼠标状态，不是修复覆盖层呈现的位置计算。不要为光标渲染修复额外创建进程级 DPI 切换或游戏内坐标重写。

`GetDpiForWindow(game)` 不能无条件作为显示器真实 DPI：官方定义其结果取决于目标窗口的 awareness，DPI-unaware 窗口返回 96。图像尺寸需要的 DPI 测量与取点域应分别核对。[GetDpiForWindow][ms-window-dpi]

### 保持既有 Runtime 和热点合同

`source/cursor/source/bridge.h` 的既有 Cursor v155 协议发布 `nativeW/nativeH/nativeHotX/nativeHotY/nativePixels`，不包含屏幕位置。`hide.cpp::nativeBuild()` 使用这些字段创建 HCURSOR；`nativeRefresh()` / `CursorRuntimePost()` 通过 `SetCursor` 维持外观，屏幕位置由系统处理。因此本轮 Host 坐标修复不需要新增 Runtime 屏幕位置字段。

既有采样使用 WWMouse 指针 `0x887640`、完整画布热点 `+0x3C/+0x40`，并将符号值 `12345/54321` 归一为画布中点/末像素。现有主表面指针 `0x887308` 的 `+4/+8` 用于采样宽高。这些均为旧实现引用；不在本轮扩大其样本支持范围。热点保持完整素材画布语义，不能再加裁切偏移，也不能因透明热点而自行改为可见像素中心。

## CnC-DDraw：游戏内包装与外部 Host 的边界

本轮读取官方仓库固定提交 `279a057ee7e1e4d584b56141c014ab01f3c6ee6d` 的两个文件；没有把可变 `master` 当作版本证据。

- [`src/winapi_hooks.c`][cnc-hooks] 的 `fake_GetCursorPos` 在有效游戏状态下，把真实位置减去鼠标偏移，按 `unscale_x/y` 转换并限制到游戏逻辑宽高；某些状态返回缓存逻辑点。`fake_GetClientRect` 对游戏窗口返回逻辑画布尺寸，相关 client/screen 包装也有特殊路径。
- [`src/mouse.c`][cnc-mouse] 的锁定/解锁路径取得真实客户区屏幕矩形，并考虑 `render.viewport` 偏移。`mouse_lock()` 仅在 `adjmouse=true` 时把逻辑点乘 `mouse.scale_x/y`；`adjmouse=false` 时直接加窗口/viewport 原点。`mouse_unlock()` 则使用缩放后的点。

这证明包装器在**游戏进程内部**维护了独立的输入/呈现变换，并不证明外部 Toolbox Host 的 User32 调用被该包装器或平台同样改写。仅看到游戏的 2K 逻辑画布和 4K 输出，不能推导 Host 应调用游戏内的逆变换。

其中 `adjmouse=false` 是本轮必须保留的未决边界：`fake_GetCursorPos()` 跳过 `unscale_x/y`，`mouse_lock()` 跳过对应放大，因此真实 OS 指针可能没有随 renderer 的图像倍率变化。若游戏软件光标随整个画布放大，而 OS 指针仍保持这条未缩放路径，Host 准确跟随物理点也不能自动使两者重合。**本轮物理域修复不覆盖所有这类包装器语义差异**。没有现场配置和运行观测，不能认定用户设置了 `adjmouse=false`，也不能无条件给所有物理点补倍率或改写平台设置。

旧视角缩放研究已经区分当前显示宽高 `0x8A00A4/0x8A00A8` 与设置宽高 `0xA8EB84/0xA8EB88`。`view_zoom_legacy_bridge.h` 在游戏内取得未包装 API，再映射一次到当前逻辑画布；不能把这段游戏内逻辑照搬到外置光标。上述地址为旧研究结果，本轮没有重新验证用户映像。

## 必要观测与待验证项

诊断应能区分“采样失败”“不同 DPI 域”“图像尺寸/热点设置”和“游戏呈现矩形不同”。以下是研究建议，**不表示本轮已全部实现或已取得现场值**；实际完成范围以最终源码及收据为准。建议只在状态变化或有限频率记录如下信息，不把逐帧日志作为正常渲染负担：

| 观测 | 需要保留的值 |
| --- | --- |
| 鼠标 API | `GetCursorPos` 与 `GetPhysicalCursorPos` 各自返回状态、成功时的点、失败时即时捕获的错误码；标识实际采用哪条路径 |
| DPI 作用域 | 进入前、有效作用域、退出后线程 awareness；目标游戏与覆盖层窗口 awareness；作用域建立或恢复失败 |
| 几何 | 客户区两角转换结果、显示器矩形、当前显示模式和渲染窗口身份；负原点应原样记录 |
| 缩放输入 | 当前采样游戏宽高、各项 override、DPI 模式、aspect-fit、手动缩放和热点偏移 |
| 最终输出 | 光标原画布、最终图像尺寸、最终热点、拟放置矩形；必要时读取实际覆盖层矩形核对 |

后续验收应覆盖：普通 PMv2 的 4K/2K 组合不二次乘屏幕点；100%/150%/200% DPI；有负原点的混合 DPI 多屏；取点或客户区转换失败；窗口跨屏；全屏切换和 Alt+Tab；有黑边/居中的包装器 viewport；手动覆盖值；外置与原生 HCURSOR 两条路径。包装器版本、配置、系统兼容性选项和现场点位缺失时，不能把这些项目全部标为已验证。

## 实际实现与受控验证

新增 `cursor_coordinates.h` 与 `cursor_coordinates_windows.h`。创建覆盖层、读点、客户区两角转换和最终窗口位置提交保持同一 DPI 作用域；`GetPhysicalCursorPos` 不可用时，只在已确认物理域中用 `GetCursorPos`。无法建立 PM 作用域时，保留完整的 caller-logical 回退，不把物理点混进逻辑窗口。

覆盖层记录创建时的坐标域。逻辑回退恢复为物理域时重建覆盖层并强制提交新图像；已经创建的物理覆盖层不会在暂时切换失败时接收逻辑位置，下一帧可以重试。创建失败按 1 秒退避，保留错误码。诊断 HUD 可独立恢复，不成为原生 HCURSOR 的依赖。

`cursorMapPoint()` 与 `refreshClickRegion()` 共用显式 aspect-fit 的 viewport。等比例设置未开启时保留拉伸模型；不从显示器与游戏分辨率比猜测平台 viewport。屏幕点只减去最终缩放热点，图像大小、DPI 模式与手动缩放不再参与屏幕位置的倍率。

新增 `cursor-coordinates` 记录写入 `cursor.log` 与统一 Host 的 `logs/Runtime.log`，记录调用域、取点 API、原 caller 点、采用的点、客户区/显示器矩形、游戏尺寸、图像倍率、热点、目的位置和错误。两次取点是顺序采样，不能用它们的比值估计 DPI。变化记录最短间隔 1 秒，稳定状态 15 秒；移动点不进入变化键。

| 实际执行项目 | 结果与范围 |
| --- | --- |
| `assist/tests/cursor_coordinates.py` 默认配置 | 30 场景、48 检查通过；实际 production helper 和 main 中抽取的 guard、viewport、窗口重建、诊断及提交表达式 |
| 缺少线程 DPI API | 1 场景、2 检查通过；整条链使用一致的 caller-logical 回退 |
| 缺少物理取点 API | 30 场景、48 检查通过；已建立 PM 域后使用同域 `GetCursorPos` |
| pre.11 旧生产代码负对照 | 6 场景、11 检查中的 7 项按预期失败，退出码 1；包含取点失败、黑边/侧栏判定及新物理域合同差异；普通 PMv2 点不二次缩放的控制项保持通过 |
| `cursor_visibility_order.py` | 7 项源码顺序/恢复合同检查通过；不是 Win32 输出执行 |
| `cursor_settings_ui_test.py` | 实际设置 handler 在 portable Win32 控件替身中通过 |

主坐标原始收据：`analysis/pre12-cursor-coordinates/receipt.json`；规范收据：同目录 `normalized-receipt.json`。独立存量检查：`analysis/pre12-cursor-existing-independent/validation.json` 与 `receipt.json`。这些路径相对于本次工作区；聚合源码证据另存 `source/research/evidence/cursor-coordinate-recovery-release-validation-20261107.json`。

测试在 Linux / g++ 中运行，Win32 API 与图像提交接收端是受控替身。默认/缺 API 的同类检查不合并成唯一场景数量。旧逻辑 caller 的失败表示没有实施新的显式物理域合同，**不表示 Windows DWM 一定会把同样在逻辑域创建的旧窗口画错**。旧代码未取第二个客户区角的失败同样属于新增几何合同的负对照，不证明旧公式在普通 PM 下必然错误。

Cursor 启动/start/status 片段与 `c092a70e` 保持逐字一致；其需要 Windows 执行的旧 startup fixture 本轮没有运行，也不计为通过。原生 HCURSOR 图像创建、实际 Windows/DWM、用户平台、全屏切换视觉和游戏/多人运行未执行；没有把 dormant 旧 Cursor 补点路径重新启用。

交付使用冻结提交的完整 x64 Host / x86 Runtime+broker 构建，构建结果和实际 PE/ABI 检查在独立 provenance 与交付清单中记录，不从本节的 Linux 受控通过推定 Windows 实机表现。

## 官方主源与精确版本

以下主源均于 **2026-11-07 UTC** 访问。Microsoft 项目记录页面显示的更新日期，不把该日期当作用户 Windows 的版本；CnC-DDraw 项目固定到提交和文件 Git blob SHA。

| 主源 | 固定版本或页面更新日期 |
| --- | --- |
| [Microsoft：About Cursors][ms-cursors] | 页面更新 2026-09-21 |
| [Microsoft：High DPI Desktop Application Development][ms-dpi] | 页面更新 2026-09-10；文档注明 PMv2 从 Windows 10 1703 引入 |
| [Microsoft：GetCursorPos][ms-getcursor] | 页面更新 2022-11-19 |
| [Microsoft：GetPhysicalCursorPos][ms-physical] | 页面更新 2024-02-22；最低支持 Windows Vista |
| [Microsoft：ClientToScreen][ms-client] | 页面更新 2024-02-22 |
| [Microsoft：SetThreadDpiAwarenessContext][ms-thread] | 页面更新 2024-02-22；最低支持 Windows 10 1607 |
| [Microsoft：GetDpiForWindow][ms-window-dpi] | 页面更新 2024-02-22 |
| [CnC-DDraw：winapi_hooks.c][cnc-hooks] | commit `279a057ee7e1e4d584b56141c014ab01f3c6ee6d`；blob `33655c1b2378d234242e1ac7eff6eb0dfae62c50` |
| [CnC-DDraw：mouse.c][cnc-mouse] | 同一 commit；blob `46aca134f7d0c4ff2ba3aaf09aec43d7ccf59594` |

[ms-cursors]: https://learn.microsoft.com/en-us/windows/win32/menurc/about-cursors
[ms-dpi]: https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
[ms-getcursor]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos
[ms-physical]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getphysicalcursorpos
[ms-client]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clienttoscreen
[ms-thread]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext
[ms-window-dpi]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow
[cnc-hooks]: https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/winapi_hooks.c
[cnc-mouse]: https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/mouse.c
