# Vulkan 分数倍率解析实验

## 问题、来源与范围

用户要求尝试让 GPU 执行渲染升级，并指出已有 Vulkan 绘图库。本次先迁移已合成、经 xBRZ 重建之后的分数倍率双线性解析；xBRZ 1.9 本体、SHP/TMP 场景合成和 VXL 光照/几何光栅化仍由 CPU 执行。结果仍回读给现有 Win32 分层窗口，因此这不是 GPU 直接呈现，也不是整个渲染器完成迁移。

已在 `INDEX.md` 和持久笔记中检索既有 VERA20K 研究。固定 VERA20K 提交 `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f` 是 Rust 引擎，render 使用 wgpu，Cargo 依赖 wgpu 29.0.4；并非可不经适配直接交给本 C++ 外部采样器的 DLL。官方 wgpu-native `v29.0.1.1` 有 `i686-pc-windows-msvc` 包，不能声称 32 位 Windows 不受支持。本次选择较小的 Vulkan 1.0 动态入口适配器，避免同时引入整个 Rust/wgpu 运行时；这不表示 wgpu 方案不可行。

依赖是未修改的 [Khronos Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/b379292b2ab6df5771ba9870d53cf8b2c9295daf)，tag `v1.3.290`，commit `b379292b2ab6df5771ba9870d53cf8b2c9295daf`。`source/third_party/vulkan/UPSTREAM.json` 逐文件记录官方路径、字节数、Git blob 和 SHA-256，保留 Apache-2.0/MIT 文本。只使用 Vulkan 1.0 功能。Windows 从系统目录载入 `vulkan-1.dll`，不增加链接导入项、不打包 Vulkan loader、不搜索游戏目录中的同名库。官方 PFN 保留 Win32 `__stdcall` ABI。

## 数据、缓存与计算

实现 `external_reader/source/vulkan_compute.h`。输入是不可变 PARGB8 tile 的 `shared_ptr` 别名，保留已经完成的现场颜色/光照；不是预先离线生成游戏光照。每个任务包括真实源宽高、目标矩形、首个源像素中心与每轴步长。

- GPU 常驻 atlas 为 96 MiB；缓存比较 shared ownership 身份和 vector 地址，单纯原始地址复用不会命中旧像素。只缓存弱引用，CPU 素材生命周期不被永久延长。
- 一帧统一整理 dirty tile，批量 upload、多个 dispatch、一次最终结果 readback。相同不可变 tile 的后续帧上传为 0。空间碎片不足时仅对当前活跃 tile 重排，不增长 atlas。
- 标准 Q8 双线性，每轴权重 0..256，通道使用相同正权重，只在最终结果舍入一次；保持预乘 alpha。CPU 计算边界和 Q8 坐标，shader 不要求设备支持 float64。
- 输入限制为 8192 每轴、16M 输出像素、最多 4096 不相交目标矩形、4M 坐标项。输出、读回、上传和坐标 buffer 按需增长；atlas96 + output64 + readback64 + upload128 + coordinates64 的逻辑容量上限约 416 MiB，另有驱动对齐/实现开销；不能把 96 MiB atlas 误当作整个后端内存上限。

Shader 为本项目小型适配程序 `external_reader/shaders/vulkan_bilinear.comp`，由官方 glslang 和 SPIR-V Tools 编译/校验，生成 `vulkan_bilinear_spirv.h`。源 SHA-256 `b826606b659b19782499d75fb7334a0dee0725bbbb2cb4573a90dd0bbf49a604`；SPIR-V `7e5910b0b46877ce2de687de7feadcf8504a3cd70db8f4eed6770563392ee15f`，3792 字节/948 words。它是标准双线性解析，不是自行发明替代 xBRZ 的纹理重建算法。

## 回退、同步与故障

宿主仅在用户启用实验选项、存在真实分数解析工作且可见中间 tile 符合预算时提交 GPU；失败使用同一批 tile 的 CPU 结果，避免整帧变黑或丢失。普通整像素直拷无需 GPU。产品不使用 Vulkan CPU 软件设备，以免把已有 CPU 工作再搬到软件 Vulkan；软件设备仅在显式 `RA2_VULKAN_TEST_SOFTWARE` 测试构建中允许。

命令先同步 host/transfer 写入，再 compute，最后 transfer/host 读回；host 不一致内存执行 flush/invalidate。等候 fence 后才复用资源。硬件执行 fence 安全上限为 100 ms；这是故障保护，不是目标帧时间，也不包含驱动实例、管线和初始 shader 编译所需的初始化时间。初始化/设备/运行时分配故障保持 CPU 回退，不逐帧重新尝试 OOM 分配。

fence 超时后禁止继续写入、释放或卸载尚在执行中的资源；后端隔离这些有界资源，进程级 sticky 标记阻止切换开关/重启 worker 再创建另一套超时后端。析构只在确认 fence 完成时清理，否则留至进程退出，避免无限 `DeviceWaitIdle`。这是实验后端的故障折衷，不能声称驱动挂死后所有平台进程退出行为已验证。

## 验证与限制

- `assist/tests/vulkan_compute.cpp`：真实 Vulkan shader dispatch 与独立 Q8 CPU 参考逐像素对照；premultiplied invariant、稳定 tile 零上传、一素材替换仅上传该素材、强制相同 vector 对象地址但新 ownership 不复用旧内容、每帧只读回一次、不合法尺寸/重叠/NaN 拒绝后仍可继续。
- 本地 Linux 没有 ICD：验证动态 loader/instance 不可用时干净回退，没有把这次运行记作 GPU 成功。Zig 0.13.0 Win32 x86 交叉编译通过；没有 Windows GPU 设备实测。
- Bootstrap run `40729123538` 验证 shader 编译与 SPIR-V 校验。第一次实际软件 Vulkan run `40734749488` 提前退出时 segfault；ASan/UBSan run `40735269615` 查明第一帧触发 100ms fence 超时，没有报告宿主越界。这不构成硬件性能结论。Mesa 初次 shader JIT 可能超过硬件保护门限，显式软件测试构建改用 10s 的独立等待预算，产品仍保持 100ms 硬件保护。
- 软件 Vulkan run `40735580583`（head `310853a8c6703e5e0addce296f892b82a6cc22e4`）在 Mesa llvmpipe LLVM 20.1.2 下真实执行 61 帧、1,483,520 像素，与独立 Q8 参考逐位一致；稳定上传 0、每帧读回 1，通过强制地址复用。编译启用 ASan/UBSan，未报告宿主内存错误。该 run 的外部 `rg` 日志检查命令缺失，不能把其 job success 单独当作已验证日志过滤器；已将检查换为不依赖 rg 的 Python 断言，最终整合 CI 继续检查 validation/VUID 输出。
- 软件成功不等于显卡帧率提升。完整游戏内画面、AMD/NVIDIA/Intel 实机、全场景性能和 GPU 直接呈现仍待验证。

本项不新增游戏内存地址、游戏函数调用或写入，不变更 IPC 协议或 CnCNet 逻辑；游戏 PE SHA/原生 ABI 不适用于本次 GPU 适配器。原有坡面/命令线原生证据见各自独立笔记。
