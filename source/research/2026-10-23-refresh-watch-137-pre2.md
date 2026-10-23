# 1.0.37-pre.2：强制场景重绘、DDraw 与真实地表观察窗

## 问题与样本

用户要求：在现有三个高刷模式旁增加可用于联机的强制模式，参考 DDraw 的实现；危险载员或手动指定单位在左中 11×11 格小窗显示，尽量接近第二相机。静态地形应使用真实资源，而非仅用地块颜色代替。

本次反汇编、受控执行仅覆盖以下私人样本；仓库不包含游戏 EXE 或原始转储：

| 样本 | SHA-256 |
| --- | --- |
| LBW | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| 战网 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

证据等级：反汇编、实际 x86 函数/DLL 受控执行、合成资源解析。**没有新增 Windows 实机画面或联机观察。**

## 强制重绘与呈现是两个环节

`MapClass::FlagRedraw`，VA `0x4F42F0`，thiscall，ECX=`0x87F7E8`，一个 int 参数，`ret 4`。传入 2 后设置 Tactical 对象（指针 `0x887324`）的 `+0xD7D` 字节、Map 的 `+0x0C` 重绘等级，并调用 `0x578AC0` 标记脏格。两样本该函数完整 46 字节一致。新模式校验全部 46 字节，入口或函数中段被改写均拒绝启用。

原生 Render `0x4F4480` 读取并清除 Map `+0x0C`，把重绘等级交给 Tactical `0x6D3D10` 的图层绘制。因此只额外调用 Render 不等于一定重建整个场景。LBW 的原 Render 入口存在启动跳转，本版继续调用原入口，不绕过平台逻辑。

模式 3 在原有 QPC 节奏下先调用 FlagRedraw(2)，再 Render；不主动推进游戏帧、不修改单位坐标、不新增逻辑 tick 入口。保留原有受管理的 ScrollMap 适配器、输入门禁和卸载恢复。按用户最新要求在 session 0/5/3/4 均可启用，不加单机限制。状态显示的是绘制调用频率，不能当作显示器实测 FPS。

被否定的假设：频繁 Render 必然产生新场景；DDraw 提高呈现 FPS 必然插值单位运动；联机必须强制禁用该用户选项。未决：平台原渲染入口的真实副作用、画面流畅度、CPU/GPU 开销和双机同步。

## DDraw 原始实现参考

- [CnC-DDraw](https://github.com/FunkyFr3sh/cnc-ddraw/tree/279a057ee7e1e4d584b56141c014ab01f3c6ee6d)，检查 `src/render_d3d9.c`、`src/fps_limiter.c`、`src/config.c`、`src/ddsurface.c` 和导出定义。`maxfps=-1` 取显示器刷新率；`minfps=-2` 使渲染线程按计时刷新并强制上传主表面。`maxgameticks` 是另一项限制，不能一并乱改。常见游戏节及 `/2`、`/wine` 变体会覆盖全局配置。
- [TS-DDraw](https://github.com/CnCNet/ts-ddraw/tree/5c1394b699fd9b6e37ab84b993cab147044b41a9)，检查 `src/render.c`、设置与入口源码。`[ddraw] TargetFPS=0` 使用显示器刷新率（失败回退 60）；独立呈现线程按目标节奏运行。这些代码没有提供 RA2 世界状态插值。

本版独立实现有限 INI 编辑器与只读 PE 导出识别，不复制或替换 DDraw DLL。CnC 修改当前游戏节及其变体的 maxfps/minfps；TS 修改 TargetFPS。保留 vsync、maxgameticks 和其他配置。CnC 可用环境变量改配置路径，TS 相对工作目录读取，故由文件选择器明确实际配置文件。备份与 applied 快照用于拒绝覆盖后续外部编辑。格式、边界与幂等性通过 `assist/tests/ddraw_config.cpp`；Win32 文件选择、ReplaceFile 和实际 DDraw 生效仍需实机。

## 加载后的 TMP 不是 SHP

基础地表（草地、道路、坡面、悬崖）使用 TMP；树木、覆盖物等使用 SHP；载具常用 VXL/HVA。资源静态意味着能缓存，但仍需正确处理子图、等距投影、格高、色盘和遮挡。

| 地址/字段 | 原生证据与用途 |
| --- | --- |
| `0x547020` | IsoTileType 加载器将 TMP 存到类型 `+0xA4`，设置 `+0xA8`；目录中相对偏移被改成绝对帧指针，没有转成 SHP |
| `0x544CB0` | thiscall GetImage；EAX 返回类型 `+0xA4`，未加载时可按类型条件懒加载 |
| `0x547150` | thiscall，`(index, width*, height*)`，`ret 12`，布尔结果在 **AL**；index 对列×行取模；悬崖高度为 tileHeight + blockY - extraY |
| `0x5471B0` | thiscall，`(index)`，`ret 4`，返回帧 `+0x2A` 的坡面字节；同样取模 |
| `0x547CF0` | 原生地表绘制，60 宽、29 个非空扫描行；存在 `+0x2BC` 变体链和 `+0x2F0` 数量，本版尚未复现变体选择 |
| `0xA8ED28` | IsometricTileType 数组；Cell `+0x38` 是类型索引，65535 时使用 `0xAA10B0` 默认地块及子图 0 |
| Cell `+0x11A` | 实际 TMP 子图字节，`0x47D34D` 送入坡面查询，`0x4802B8/0x4803A2` 送入地表绘制；不能照头文件名称把它当作高度 |
| Cell `+0x11B` | 有符号格高，`0x480416..0x48042E` 乘 15 后从屏幕 Y 减去 |
| `0xABBED0` | 当前 ISO 色盘，768 RGB 字节；`0x5454C0..0x5454E9` 加载后已经左移 2 位，外部读取不可再次左移 |
| Cell `+0x44/+0x11E` | Overlay 类型索引/帧；`+0x48` 是 Smudge，不能混用；OverlayType 数组 `0xA83D80` |
| `0xA8E988`, vtable `0x7F522C` | TerrainClass 数组/身份；`0x71BB90` 构造器对应 Type `+0xC8`、动画阶段 `+0xAC`、倒塌标志 `+0xCD` |
| `0x71C2B2` | 普通树使用格子光照转换器 `+0x34` 和地形亮度 `+0x10C`；特殊泰伯利亚树还有另一色盘，本版尚未复现 |

加载后 TMP 头为列、行、60、30，`+16` 是绝对子图指针表。每帧 52 字节头，后接 900 地表像素和 900 Z 像素；Z 偏移 952。菱形行宽为 `4*min(y+1,29-y)`。extra 标志存在时按其相对偏移读取悬崖矩形，0 号色透明；用子图列行投影修正 extra 起点。解析器限制尺寸、指针与工作量，并重读目录指针拒绝并发替换。本版没有使用 TMP Z 平面做逐像素跨物体遮挡。

格式交叉参考：[OpenRA TMP loader](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs)、[YRpp CellClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/CellClass.h)、[TerrainClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/TerrainClass.h)、[FileSystem](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/FileSystem.h)。仅用来核对格式与布局，最终字段依据以上样本原生调用。新解析器为独立实现。

## 观察窗实现与限制

外部只读采样危险载员目标或手动 pointer+ID 固定目标，读取中心 ±5 格，绘制已加载 TMP、Overlay SHP、树木 SHP、单位真实模型；合并按 x+y 排序，格高每级 15 原生像素。手动默认 Ctrl+Alt+W，悬停优先于选择；目标死亡或指针复用解除，回到自动危险载具。切进程/对局清缓存；采样 20 Hz，缓存与每次解码工作量有界。

单位暂固定朝向，VXL 为简化光照；SHP 单位为首帧，不是完整步行动画。未复现每格动态光照、阴影、桥层、全部变体、特效和原生逐像素 Z-buffer。未加载资源显示标记/地块色并计数，不能声称所有远处贴图均可读。小窗是外部重绘的局部观察，不是另开一个原生相机。迷雾目前沿用雷达式探索过滤，尚非原生相机完整遮蔽效果。

## 验证路径

- `assist/tests/terrain_native_runtime.py`：两样本执行上述 GetImage、尺寸、坡面原函数；构造已加载 TMP 内存，不调用磁盘加载器/渲染器。明确检查 AL 的布尔 ABI。
- `assist/tests/terrain_preview.cpp`：900 像素菱形、悬崖尺寸/锚点/透明、色盘、目录取模、SHP 多帧、坏指针与并发替换。
- `assist/tests/watch_view.cpp`：危险载员、悬停/选择、边界 121 格、探索过滤、死目标/地址复用、树和 overlay 正确字段。
- `assist/tests/scroll_refresh_runtime.py`：真实 DLL/FlagRedraw 与 ScrollMap/SetTacticalPosition；渲染器、脏格枚举及 Win32 使用桩，确认 session 0/5/3/4 强制请求、切换恢复、签名拒绝、QPC 节奏、无逻辑帧推进/单位改写/事件追加。
- `assist/tests/formation_runtime.py` / `formation.cpp`：屏幕拖动经 30:15 逆投影决定朝向；同格内短拖也更新，测试时推进时钟跨过正常 16 ms 鼠标预览节流。
- `assist/tests/formation_host.cpp`：有界模型/朝向图像缓存回收，奇数 seqlock 保留有效图像，避免类型增多或长期旋转后永久丢预览。

最终运行结果和二进制哈希见 `verification/review-137-pre2/summary.json`。
