# 实验性视角缩放：50 / 75 / 100 / 125 / 150%

基线：`78c3deafa3c73b7c1c3a3c50ed362dbeed8fa34f`（v1.0.37-pre.4）。
问题：在既有 Runtime / 游戏线程入口内切换实际渲染分辨率，再由 CnC-DDraw 缩放到固定输出窗口；保留 50% 和原分辨率恢复。

## 样本与证据范围

仅以下私有样本做了新一轮反汇编和受控执行。未上传 EXE、内存转储或原版函数字节。

| 样本 | SHA256 | ChangeResolution 全体 0x58A 字节 FNV-1a |
| --- | --- | --- |
| gamemd-spawn(4).exe / CnCNet | c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a | 858162CF |
| gamemd-spawn(5).exe / MRGA | ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707 | C0FFC84E |

以上为兼容性指纹，包含原有平台跳板，不是密码学信任检查。其他补丁组合会拒绝此功能；不据此宣称 LBW 或其他 EXE 已验证。

## 反汇编确认

- `0x560BF0`：`bool __fastcall ChangeResolution(int width, int height)`；ECX=width，EDX=height，AL 返回结果，调用方不提供栈参数。范围到 `0x56117A`。内部隐藏鼠标、等待图形操作、销毁旧 WWMouse/主表面、调用 SetVideoMode、重建缓冲区、鼠标和侧栏/雷达，再置重绘。
- `0x561180` 是带确认对话框和倒计时的上层设置流程，不适合在 Runtime dispatcher 内调用。本实现只调用 `0x560BF0`，不模拟游戏设置对话框。
- `0x4A42F0`：SetVideoMode，ECX=HWND、EDX=width、栈上 height / 16 bpp；`0x533FD0` 重建表面；`0x7B8730` 构造 WWMouse；`0x4A8960` 更新屏幕边界；`0x6ABD30` 更新雷达。
- `0x8A00A4 / 0x8A00A8` 是当前显示宽高。`0xA8EB84 / 0xA8EB88` 是 GameOptions 中的设置宽高；直接 ChangeResolution 不改后者。不能继续拿设置宽高映射热缩放后的鼠标和叠加层。
- `0x886FA0..AC` 是战场区域；`0x886FB0..BC` 是完整画面。`0x887324` 指向 Tactical，中心 +D64/+D68、目标中心 +D74/+D78；`0x6D6000` thiscall(Point*) 用于恢复视野中心。
- `0x887640` WWMouse、`0x887308` 主表面、`0x8A0094` DirectDraw、`0xB73550` 游戏 HWND、`0x89F978` 原生窗口模式标志。DirectDraw vtable+0x54 的 SetDisplayMode 所属模块用于查找渲染器，允许 DLL 改名。

## 渲染器依据与实现

依据 CnC-DDraw 官方 [API 文档](https://github.com/FunkyFr3sh/cnc-ddraw/wiki/cnc%E2%80%90ddraw-API)、[dd.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/master/src/dd.c)、[winapi_hooks.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/master/src/winapi_hooks.c) 和 dllmain.c（2026-10-23 检查）：

- `DDEnableZoom()` 是 cdecl 无参函数；每次 SetDisplayMode 前启用，用于保留原输出尺寸。此开关由 SetDisplayMode 消费。
- `DDGetProcAddress` 为 WINAPI，返回未被包装器替换的 Win32 函数。必须用真实 GetClientRect 检查物理输出；额外拖屏采样也用真实 GetCursorPos/ScreenToClient，再映射一次到当前逻辑分辨率，避免双重缩放。
- 某些无边框/全屏路径仍会扩大输出，不保证支持。要求缩放前比例一致，缩放后物理客户区不变；失败回滚到上个模式。建议普通 CnC-DDraw 窗口，游戏自身保持全屏模式。
- 包装器在 zoom 模式可能禁用拖拽调整大小 / Alt+Enter。恢复 100% 只恢复逻辑分辨率，不宣称重置包装器的所有配置；完全退出游戏可恢复启动状态。

目标分辨率按 `base * 100 / percent` 四舍五入。50% 为两倍宽高、约四倍完整画面像素；1920×1080 -> 3840×2160。整张画面（包括侧栏）参与缩放，不能当作“侧栏大小不变”的独立战场相机实现。

默认关闭，实验页提供五档、恢复按钮和 Ctrl+Alt+- / = / Backspace。最小 640×480，单边最大 8192，总像素不超过 4000 万。不合格目标拒绝，不能靠夹取宽高悄悄改变倍率/比例。

复用现有 Runtime 游戏线程 lease，暂停高刷补丁并作废旧输入捕获后，在 MSVC x86 SEH 边界中调用。异常后不继续尝试同一原生流程，提示重启。正常失败回滚；重复相同失败请求不按帧重试。恢复原倍率、断开和新对局复用已有清理路径。原生菜单/临时暂停时不发起新缩放。

## 验证与边界

`view_zoom.cpp` 检查倍率、上下界、4K 到 50%、比例与无效输入；现有 portable UI / 外部读图 / 指针 / 侧栏测试覆盖相关集成。

`view_zoom_runtime.py` 对两样本运行实际新 x86 DLL、dispatcher 和 SEH 适配器。**渲染器 API 和原生 ChangeResolution 本体被模拟**；检查 fastcall 传参、busy 标志、全部倍率往返、四种 session 的退出/换局恢复、前台/聊天/按键门控、接口与指纹拒绝、失败回滚；检查未写设置分辨率、未新增事件、未推进模拟帧、未改单位数据。证据 JSON 包含受测 DLL SHA256。

未完成：真实 Windows DirectDraw 表面分配、原生重建全流程的实机运行、鼠标命中/拉框/雷达视觉验收、不同渲染器版本、50% 大地图性能、联网同步。受控测试不能代替这些结论。预计缩放为本地呈现操作，但不据此保证联机安全。
