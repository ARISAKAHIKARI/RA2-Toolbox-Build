# pre.15：游戏画布拉伸时的光标位置映射

日期：2026-11-07（北京时间）。审计基线：`00266f7d4cc11d330625ad38541ac7d06c5525cd`，产品基线 `1.0.43-pre.14`。

## 问题与证据边界

用户再次反馈光标图像在实际游戏光标的左上方，并补充“游戏应该是把 2560×1440 显示在左上方的图像映射到全屏了”。前一轮已经统一外置窗口的 DPI 作用域和屏幕取点，但尚未覆盖游戏软件画布与系统指针使用不同位置变换的情况。

调查前检索了研究 INDEX、`2026-11-07-cursor-coordinate-recovery.md`、`2026-10-23-experimental-view-zoom.md` 关联线索，以及同一持久笔记 `RA2-Reverse-Engineering-Notes.sqlite` 的 v156。没有重新读取、执行或发布用户私有 EXE；没有新增受支持 EXE SHA-256、游戏函数地址、ABI、字段偏移或第三方 Hook。本文结论来自当前生产源码、固定版本包装器源码与受控函数测试；不等同于用户电脑上的视觉复现。

## 已确认的生产覆盖缺口

旧 `draw()` 最终一直调用 `spriteOrigin(systemPoint, scaledHotspot)`。它只能把图像热点放到系统取点处；`Viewport::project()` 虽然已经用于地图/侧栏区域判断，却没有参与最终位置。图像宽高和热点的缩放一致，没有证据表明应再添加裁剪偏移或改变原生热点。

如果输入仍被限制在左上角 2560×1440，而游戏把该画布显示为 3840×2160，输入点 `(1280,720)` 对应的游戏显示点就是 `(1920,1080)`。旧路径仍画在 `(1280,720)`，偏差随向右下移动而增长。这个数学反例证明缺少该呈现模型，不证明用户现场一定采用某个包装器设置。

正确分层为：

```
displayPoint = output.origin + (inputPoint - source.origin) * output.size / source.size
spriteOrigin = displayPoint - finalHotspot
```

普通 Windows 屏幕坐标已经是最终显示点，必须保持恒等；不能只见到 4K 显示器和 2K 游戏就无条件乘 1.5。图像缩放百分比、Windows DPI 倍率与手动热点仍只控制图像/锚点，不成为位置倍率。

## 包装器主源依据

固定 CnC-DDraw 提交 `279a057ee7e1e4d584b56141c014ab01f3c6ee6d`：

- `src/winapi_hooks.c`，blob `33655c1b2378d234242e1ac7eff6eb0dfae62c50`：`fake_GetCursorPos()` 先减去鼠标区域原点；只有 `adjmouse` 开启才使用 `unscale_x/y`。`fake_ClipCursor()` 也按 `adjmouse` 决定是否把限制范围放大到 renderer viewport。
- `src/dd.c`，blob `b9e5b1284b9ea5759896443a44eebf55de97cea4`：鼠标限制范围默认等于逻辑画布宽高；`adjmouse` 开启后改为 viewport 宽高。`lock_mouse_top_left` 可以把输入原点设为左上。
- `src/mouse.c`，blob `46aca134f7d0c4ff2ba3aaf09aec43d7ccf59594`：锁定时按相应原点及倍率设置 OS 鼠标位置与真实 clip；解除锁定走单独的恢复路径。

以上说明“系统点落在较小输入范围、游戏图像再放大”是实际存在的实现模型。没有读取用户当前包装器配置，不能称现场 `adjmouse=false` 已确认。

## 本轮实现

新增 `cursor_presentation.h`，区分三个用户选择：

| 光标位置 | 行为 |
|---|---|
| 自动匹配游戏（默认） | 保持正常系统点。仅在物理坐标域、真实客户区恰为当前显示器范围、有效实际游戏尺寸、clip 宽高恰等于游戏画布、原点吻合、两轴等比放大且点在 clip 内时，启用窄兼容映射 |
| 跟随系统鼠标 | 明确保持系统点，提供退出兼容推断的选择 |
| 游戏画布拉伸到全屏 | 明确采用用户选择的画布模型；源宽高取成对游戏尺寸设置或实际游戏采样，目标取当前显示器实际范围；按保持纵横比选项处理居中黑边 |

自动判断不使用手填的游戏/系统宽高或 DPI 推断源域。显式模式允许成对游戏宽高设置；系统宽高和 Windows 缩放设置仍不进入位置变换。源、目标包含完整屏幕原点，负坐标多屏不直接乘绝对坐标。越界点不夹取为伪成功位置。

每帧重新读取 clip 和显示器几何，不缓存旧的识别结果跨越失焦或显示模式变化。继续沿用既有前台、窗口有效性、客户区、取点失败与配对 Hide 恢复边界。没有调用 `SetCursorPos`、修改真实点击位置或重新启用旧的 dormant 补点循环。

`GetClipCursor` 只能证明共享 OS 光标当前的限制矩形，不能证明由谁设置，也不能单独证明游戏输入变换。因此自动 route 明确命名 `automatic-canvas-clip-compat`，日志标明兼容推断。具有相同几何但不同输入变换的特殊平台仍可能需要明确选择“跟随系统鼠标”或“游戏画布拉伸到全屏”。没有声称自动识别所有包装器、裁切或整数缩放。

最终显示点同时用于外置图层提交和命令光标的地图/侧栏分类。手动图像倍率、DPI 模式、热点偏移保持独立。原有区域补点路径未被重新开启。

## 原生 HCURSOR 的有效呈现方式

`SetCursor` 只设置形状，不接收显示位置。需要非恒等映射时，原生 HCURSOR 无法自行跟随新的游戏显示点。因此保留用户的显示方式偏好，实际临时采用外置图层，并通过已有 `nativeBlank` 退让 OS 图像；映射解除后恢复原有原生选择。

`cursor_presentation_route.h` 统一决定原生、空白与外置三种有效状态。路径切换强制提交图像、清除原生不可见计时；原生不可见 watchdog 与硬件光标状态采用实际路径。已有共享结构、映射名称、协议版本与 Runtime 入口保持不变。

此次还修正了启用该路由必需的三个状态边界：

1. `nativeRefresh()` 与 `CursorRuntimePost()` 的 Windows 对话框处理优先于游戏空白光标，防止菜单也被隐藏。
2. 从空白呈现切回原生时，先恢复空白路径持有的旧光标，再建立新的 HCURSOR 所有权，避免遗留 `blankActive`。
3. 进入菜单清除空白请求；退出菜单按实际映射路径恢复，不能无条件打开原生 HCURSOR。外置窗口准备/提交失败继续沿用配对 Hide 还原。

新增 `cursor-presentation` 限频日志，记录选择、实际 route、实际游戏尺寸、输入点、显示点、clip、客户区、显示器、源/目标矩形和原生请求/有效状态。移动点不作为变化触发键。

## 验证

生产纯映射、实际 Host/Runtime 路由及设置 handler 的受控结果见 [evidence JSON](evidence/cursor-fullscreen-presentation-20261107.json)。

| 实际执行 | 结果 |
|---|---|
| 生产纯映射 helper | 26 场景、51 检查通过 |
| 默认 API 配置的生产坐标/呈现/菜单/恢复路径 | 51 场景、97 检查通过 |
| 缺少 PhysicalCursor API、已确认 PM 域 | 51 场景、97 检查通过 |
| 缺少线程 DPI context API | 1 场景、2 检查通过，保留一致逻辑域 |
| pre.14 实际生产位置路径负对照 | 7 场景、8 检查，4 项按预期失败 |
| pre.11 与中间恢复版本负对照 | 分别按预期失败 7 项 |
| 实际设置 handler | 三种 PositionMode 保存与写入失败重试通过 |

受控检查还覆盖外置窗口持续失败不重复订阅、从失败的旧 mapped 路由切回原生、keep/SHA 动态尺寸刷新、自动映射忽略尺寸 override 后仍正确分类地图/侧栏，以及菜单 ShowCursor 增量配对。测试中的 Win32 边界是替身，不模拟 DWM 或真实平台。产品交叉编译、PE/配套校验与归档结果另由冻结提交的构建收据绑定。

真实 Windows/DWM、用户当前平台、实际全屏切换视觉及多人游戏均未执行。后续现场核对仍需区分自动匹配是否命中、显式源尺寸是否正确，以及平台是否具有本次未建模的 viewport。

## 主源

- [CnC-DDraw winapi_hooks.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/winapi_hooks.c)
- [CnC-DDraw dd.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/dd.c)
- [CnC-DDraw mouse.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/mouse.c)
- [Microsoft GetClipCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipcursor)
- [Microsoft SetCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcursor)
