# DLSS 5 NGX 实际接口、独立进程与运动历史

## 问题与来源

用户要求在工具箱增加 DLSS 5，并允许宿主改为64位。先前仅保存请求的开关不足以产生画面，本次增加实际 NVIDIA NGX Neural Rendering 创建和执行路径。它不修改游戏代码、地址、命令或模拟状态。游戏 PE SHA、函数地址和 ABI 没有新增结论。

调查前查阅研究索引、VXL GPU 光栅记录、小地图 GPU 记录及持久笔记 v81／98条。以下区分公开 SDK 来源、受控执行、真实神经推理；后者尚无支持显卡上的证据。

## 公开分发核验与被排除的假设

实测官方 Streamline2.14.16下载包276,016,170字节，SHA256 `32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b`，511项中没有 NR DLL／专用头。公共头与变更记录仍声明 NR 功能；不能推导为 NVIDIA 完全没有可用接口。

随后从 NVIDIA RTX Remix 的公开 Packman 配置找到 `rtx-remix-ngx_sdk_dlnr@2`，实际为 `.7z`：

- 包：367,851,723字节，SHA256 `4bb8651b3ac2180f23df1f39422c489810a13d445a94dc836c876c18c93ecefe`。
- NR头：`nvsdk_ngx_defs_dlssnr.h`、`nvsdk_ngx_helpers_dlssnr_vk.h` 等。
- x64运行库：`nvngx_dlssnr.dll`，版本310.9.16.0，266,424,944字节，SHA256 `2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f`。
- 基础 SDK 固定 NVIDIA/DLSS 提交 `374959484e79a640feaba44c93ac8cfb0a03f5b5`。

抓取工具校验包与每个成员；不执行下载的 DLL。官方 Remix 的 NGX 调用及 fast tone mapping shader用于核对输入约定：显示空间 LDR 归一化 RGBA16F，无需额外做 inverse-sRGB。源 ARGB32 显式拆装通道，避免按 RGBA8 上传时红蓝互换。

## 实际处理链

独立 `RA2NeuralRenderer.exe` 使用 MSVC x64、单独取得的官方 NGX 静态 loader 和 Vulkan typed helper，调用 Feature DLSSNR 的能力查询、创建与执行。主工具箱继续使用自身 xBRZ／Vulkan 管线；SDK 不链接进该宿主，也不进入源码归档。运行库由固定来源的可选安装脚本取得，许可证与组件说明随附。

宿主传递世界 ARGB、float2运动、反向正交深度和保护置信度。独立进程上传 VkImages、执行 NGX、回读结果。文字、指令线和高刷光标在神经处理之后叠加。颜色的 alpha、全透明、半透明和保护区域保持输入；全保护帧是合法跳过，不永久停用后端。最终仍回读并使用 Win32 呈现，存在额外传输和推理开销，没有 FPS 收益证明。

`neural_frame_protocol.h` 的256字节固定头包含容量、尺寸、generation、frame、状态、处理时间及消息；只通过指定继承句柄共享映射和事件。成功结果必须身份、尺寸、状态完全匹配后才复制，失败不公布部分图。取消令牌在 UI 提交时捕获，启动前、传递后和等待中核对，关闭期间的旧世界任务不能随后重新创建进程。显式关闭释放空闲子进程；忙碌等待以25ms间隔响应取消。首次冷启动预算15秒，热帧1500ms；这些是上限，不是实测性能。

## 运动引导与历史

`gpu_motion.h` 将相机及每个游戏对象的实际绘制位置相邻对应。对象身份包含 object、id、type、kind 和组件路径，不以共享姿态图地址代替个体。相同几何的平移可给出当前像素到前一帧像素的位移。动画／姿态无法对应、对象增删、投影／地形／上下文变化及异常时间会保守重置历史，不预测未来运动。

场景 shader 可选输出与颜色同一帧的运动、反向正交深度和保护信息。关闭时没有额外引导缓冲或回读；可选引导分配失败保留同帧普通 GPU 画面，失败尺寸被有界缓存，避免逐帧重复申请。

神经历史与 GPU 场景历史有各自序号。后期标注、缺少引导或推理失败可能导致 GPU 已前进而 NGX 未消费；`neural_history.h` 仅在推理成功后记录消费序号，任何序号断档、重复、回退或 context 改变强制 Reset。即使 NN 已处理帧最终因 UI 新上下文而没展示，其消费历史仍按实际处理推进。

## 验证与尚未证明的部分

- `assist/tests/gpu_motion.cpp`：同姿态多对象的独立运动、相机、上下文、姿态及时间重置。
- `assist/tests/vulkan_guides.cpp`：真实软件 Vulkan 的引导图；既有场景像素 oracle 保护颜色／反馈不变。
- `vulkan_allocation_fallback.cpp`：引导 OOM 同帧原画、失败缓存、关闭／尺寸变化后重试。
- `neural_history.cpp`：跳过／失败、显式重置、序号及上下文断档。
- `neural_pixels.cpp`：ARGB／half转换、保护区域和非有限值。
- `neural_transport_windows.cpp`：真实子进程 IPC、错误身份、取消前后、超时、关闭和可恢复跳过；测试后端不代表 NGX 模型。
- 运行库安装脚本的真实 Windows／PowerShell5.1测试覆盖安装、重装、错误包保留、被占用文件回滚和 NVIDIA 有效签名，未运行 NR DLL。

最终源码的完整 Windows 链接、跨位数 Hook、GPU 引导及回退结果以同提交 CI 另记。没有在支持 DLSS 5 的实体 NVIDIA 显卡上执行模型，没有真实游戏画质、性能、DWM 或联机验收结果，不能把受控夹具或软件 Vulkan说成这些验证。
