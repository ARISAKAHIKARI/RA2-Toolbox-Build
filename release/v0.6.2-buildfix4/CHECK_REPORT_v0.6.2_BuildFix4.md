# RA2 Toolbox v0.6.2 Full BuildFix4 检查记录

交付：已编译的 Win32 Full，文件版本 **0.6.2.4**。本次修改基于最新 **Full BuildFix3**，并核对其与老师上传的正式 0.6.2 源码之间仅有编译修复差异。没有从旧版源码重新改起。

正式上传基线：`RA2-Toolbox-v0.6.2-Read-Plan-Entry-Source(1).zip`  
SHA256：`b523052f725f97b2df231335a3b2660bedb1f0d0bd15c4cbb5e26f0b2c6ba8c5`

## 修复内容

| 项目 | 检查结果与修改 |
|---|---|
| 前三轮编译问题 | 保留 BuildFix3 的注册表声明顺序、汇编标签重名、scheduler 枚举头文件和 Unicode 资源参数修复。四个组件实际重编通过。 |
| DLL 启动入口 | 原 Zig 脚本在链接默认 C/C++ 运行库的同时强行以 DllMain 为 PE 入口，绕过 CRT 启动和静态初始化。现在使用标准 DLL CRT 启动入口，再由运行库调用 DllMain；不在 DllMain 中执行游戏修改。 |
| 协议误判 | 优化器会内联 mapName 并拆开字符串，旧脚本扫描连续字节时会误报。新增 AssistProtocolInfo、AssistMappingPrefix 显式导出和 EXE 的 AssistHostProtocolInfo；离线校验真实导出数据。 |
| 运行时组件匹配 | EXE 在安装 Assist Hook 前核对 DLL 的协议号和 sizeof(Shared)。缺导出、协议不同或结构大小不一致时停止这条控制连接并提示整体更新组件。共享协议仍为 30。 |
| EXE 版本 | 原资源仍写 0.5.2、清单仍写 0.2.2.0。文件版本、产品版本和清单身份现统一为 0.6.2.4，界面产品版本仍为 0.6.2。 |
| Windows 构建入口 | 当前 BUILD_WINDOWS.cmd、BUILD-0.6.2-ON-WINDOWS.cmd 均进入同一 Full 构建路径；识别 toolchain\\zig.exe；要求 Zig 0.13.0；失败不生成验证标志或替用旧产物。 |
| 内置 Python | 内置运行环境通过 python312._pth 隔离搜索路径。校验/打包脚本显式加入自身 tools 目录，修复本地辅助模块导入失败；以隔离模式验证。 |
| 发布检查 | 增加 Hook 导出、协议/结构大小、系统依赖、版本/管理员权限/DPI 清单检查。打包前重新验证实际文件和已记录的 SHA256。 |
| Full 完整性 | 对原包 2,987 个资源文件逐个验证 SHA256 和长度，涵盖 assets、profiles、runtime、engine、licenses、third-party；当前源码和重建脚本随包交付，排除历史程序、构建中间文件和编译器。 |

## 运行逻辑核对

下列结论来自源码、便携测试和 PE/原游戏 EXE 的静态核对，没有替代实机执行。

| 路径 | 已核对的关系 |
|---|---|
| 主程序与三页 | 一个 EXE 管理光标、统计、战术三条 UI 线程；切页通过消息调整子窗，不停止模块。关闭仍走原有模块退出和恢复请求。 |
| 路径与运行环境 | 配置、资源、三个桥接 DLL 依据 EXE 所在目录定位。预处理由 CreateProcess 启动内置 x64 Python，未把 x64 库加载进 x86 EXE。 |
| IPC | UI 与 Assist DLL 均使用 RA2Assist_v037_；实际导出协议号和 Shared 字节数一致；Hook 导出的未修饰名称满足 GetProcAddress。 |
| 默认提交 | 原版构造器填充 111 字节事件，SP DoList / MP OutList 分流；保留原版送时回调、队列余量、预算和请求/回执检查。没有为本次构建新增效果层写入。 |
| 可选高层入口 | MegaMission 和 ClickedEvent 保留独立开关、默认 0；仍要求会话、对象、线程、入口签名和 OutList 追加证据。 |
| 调度 | 默认消息模式。帧模式仍有原字节与线程/帧推进检查，未满足时走既有消息兜底。没有据此宣称实验性帧入口在所有平台都可用。 |
| 显示与列表 | 运输/铁幕/冻结等显示依赖外部只读快照；保留原有高刷设置、静态类型清单和 INI 持久化逻辑。读取受限平台的能力没有扩大。 |

## 实际完成的验证

- 使用 **Zig 0.13.0**，GNU x86 Windows 目标；SEH 边界使用 MSVC x86 对象 ABI。四文件实际完成编译和链接，不借用旧版 EXE/DLL。
- **46 项原生 C++ 测试 + 16 项源码/UI 检查 + 1 项模拟 Win32 读取测试**，共 63 项全通过。日志：`validation-v062-full.log`。
- 四模块都为 **PE32 x86**；必需 Hook 导出、Assist 协议及结构大小一致；EXE 版本/清单正确；项目组件没有未附带的第三方或编译器动态库依赖。
- 使用独立 pefile 解析器复核新发布检查器，四模块全部导出表和导入表完全一致。
- 在 Python 隔离模式运行发布校验与打包，验证本地模块导入不依赖默认脚本目录搜索。
- 在临时副本上验证发布阻断：错误协议、错误结构大小、错误架构、缺失组件、未附带的动态依赖均被拒绝。
- 预处理运行环境的 **170 个 PE 文件均为 AMD64**。其非系统依赖都能在 runtime 内找到；Windows 系统组件仍由操作系统提供。这是静态依赖闭合检查。
- 两份用户游戏 EXE 各 **21/21** 处基础签名匹配；QueueMegaMission 指定片段匹配；6 个候选 CALL 的 21 字节上下文及目标均一致。日志：`abi-v062-full.log`。
- Full 原资源 **2,987 文件 / 388,429,451 字节**逐项与 BuildFix3 包核对。清单：`FULL_RESOURCES_MANIFEST.json`。

## 验证范围

本环境没有启动 Windows 版工具箱，没有注入或运行游戏，也没有进行双人联机、CNCNet/LBW、实际热键和覆盖层实测。因此不能把本次结果描述为“全功能实测通过”或“已证明不会联机掉线”。

本次没有恢复或重写科技侧栏许可、OwnerFlags、Prerequisite；科技与特殊点击功能保持 0.6.2 正式基线的现状。此次交付解决构建、组件一致性及 Full 打包问题。

## 使用和重建

完整解压后优先运行 `Start-Message.cmd`。更新时先关闭工具箱和游戏，再整体换用这份 EXE 与三个 DLL。Windows 10/11 x64 可以独立运行内置预处理环境。

需要重建时使用 Zig 0.13.0；运行 `BUILD-AND-PACK-FULL.cmd`，自动编译四组件、校验并打包。Linux 也可通过 `ZIG=/path/to/zig bash build-zig.sh` 重编，再运行发布校验和打包脚本。编译器不包含在运行分发包中。
