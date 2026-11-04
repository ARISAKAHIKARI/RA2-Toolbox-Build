# CnC-DDraw GPU 呈现参考与外部重绘的边界

问题：能否参考现有 GPU DDraw，把工具箱更多绘制工作和最终显示留在 GPU，避免计算后回读。

调查前检索本仓库索引、`2026-10-23-refresh-watch-137-pre2`、`2026-10-23-experimental-view-zoom` 及持久 SQLite v79 同名笔记。本次只检查公开源码/API，没有新增私有游戏函数分析或实机画面结论。

## 固定一手来源

CnC-DDraw 官方仓库提交 **`279a057ee7e1e4d584b56141c014ab01f3c6ee6d`**，本次查询 master 后固定该提交读取：

- [src/ddsurface.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/ddsurface.c)：`dds_Blt`、`dds_Flip`、`dds_Unlock`。
- [src/ddpalette.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/ddpalette.c)：`ddp_SetEntries`。
- [src/render_d3d9.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/render_d3d9.c) 和 [inc/render_d3d9.h](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/inc/render_d3d9.h)。
- [src/render_ogl.c](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/src/render_ogl.c)、[inc/render_ogl.h](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/inc/render_ogl.h)、[inc/openglshader.h](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/inc/openglshader.h)、[inc/d3d9shader.h](https://github.com/FunkyFr3sh/cnc-ddraw/blob/279a057ee7e1e4d584b56141c014ab01f3c6ee6d/inc/d3d9shader.h)。

这是 Direct3D9/OpenGL 呈现后端，不是本仓库现成的 Vulkan 场景渲染库。官方 LICENSE 为 MIT；本次仅参考结构，没有复制上游实现代码。

## 实际做了什么、哪些可以借鉴

| 环节 | 上游生产代码 | 对工具箱的意义 |
| --- | --- | --- |
| 游戏交付画面 | `dds_Flip` 在临界区交换 surface、bitmap、DC、mapping 指针，再设置 `surface_updated/screen_updated` 并唤醒 render semaphore；Blt/Unlock 也按主表面及 flip 超时条件标脏 | 游戏主动调用包装的 DirectDraw 接口。指针交换避免该环节整屏 memcpy，但不表示原生 VXL/TMP 绘制已经在 GPU |
| 调度 | render 线程由 semaphore/超时唤醒，`fpsl_frame_start/end` 管呈现节奏；`minfps==-2` 强制重新上传。游戏 tick limiter 在另一条逻辑 | 分开游戏观测、素材更新和显示提交。没有更新时复用 GPU 内容，不能把重复 Present 当作新游戏状态 |
| 表面/色盘分离 | 两个更新标志分别消费；8-bit surface 上传索引，palette 独立上传256项。`ddp_SetEntries` 的 changed-only 路径比较内容，未变不置脏 | 原索引纹理和当前色盘可分开缓存；色盘/光照参数变更不必重新传输每份原素材。现场光照和 fullbright 语义仍需我们的 shader 显式实现 |
| 保留纹理、轮转上传 | D3D9 两组 surface/palette texture，`LockRect→blt_clean→UnlockRect`；OpenGL 四组 texture，`glTexSubImage2D`，按 pitch 设置 `GL_UNPACK_ROW_LENGTH` | 借鉴持久纹理和有界上传轮转；这里更新 surface 时上传的是整张主表面，不能误说上游已做逐素材或脏矩形 GPU 场景更新 |
| GPU 后处理 | 8-bit palette shader 查表；16-bit RGB565/555、32-bit texture 有对应格式；D3D9 绘一个四顶点 quad，OpenGL 可经两个 FBO 运行可选 shader | 对整屏缩放、色彩展开和后处理有成熟参考；场景深度、SHP 遮挡、VXL 原几何仍须我们自己的 command/mesh 路径 |
| 最终提交 | D3D9 `Present` 或 OpenGL `SwapBuffers` 直接提交游戏自己的窗口；失败按既有 device-lost 流程处理 | 正常呈现不需要先把后处理图像读回 CPU。值得借鉴的是 GPU 内容直接进入显示链，而不是仅把一次 CPU 算法换成 shader 再读回 |

因此“参考 GPU DDraw”有实际价值，但直接安装/调用它只能处理游戏已经绘好的最终表面，不能自动迁移工具箱 `voxel::render` 的三角形像素循环或 `Surface::blit` 的逐像素深度/光照计算。原素材到现场场景的 GPU 重建，仍要提交 mesh、RGBA/index/depth 和有序绘制命令。

补充：DXVK 官方 [README](https://github.com/doitsujin/dxvk/blob/0cfe0898289fb11e3d8d0e1c62cd344b825b7d57/README.md)（提交 **`0cfe0898289fb11e3d8d0e1c62cd344b825b7d57`**）描述的是 D3D8/9/10/11→Vulkan，不包含原始 DirectDraw 接口。再套一层 D3D9 翻译也不会自动接走我们独立 CPU 生成的 VXL 几何。

## 最终 GPU 直出与透明外部窗口

当前 `overlay_compositor.h` 使用带 per-pixel alpha 的 `UpdateLayeredWindow` 和 DIB/HDC。微软 [UpdateLayeredWindow 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow) 的源参数是 HDC，没有接受 `VkImage` 的接口。把计算结果留在 Vulkan 后，还不能直接把现有调用的参数换成 GPU image。

4K RGBA 一帧为31.64 MiB；若每帧仍回读，144次/秒仅 GPU→CPU 原始数据量就约4.45 GiB/s，另外还有 CPU 拷贝/合成和窗口提交。这是数据量估算，不是现机瓶颈实测。应分别测上传、计算、fence等待、回读、memcpy、CPU叠层和 ULW，再决定优先级。

可行的长期路线是自有 HWND 的 DirectComposition visual + 带预乘 alpha 的 DXGI composition swapchain。微软 [CreateSwapChainForComposition](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition)、[CreateSurface](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice-createsurface) 和 [GPU 窗口合成说明](https://learn.microsoft.com/en-us/archive/msdn-magazine/2014/june/windows-with-c-high-performance-window-layering-using-the-windows-composition-engine) 提供该显示模型；它能让 GPU 纹理直接由桌面合成器使用，绕开 CPU 图像回读。

若保留 Vulkan 计算，需新增 Vulkan↔D3D11 共享图像和同步适配：检查同一物理 adapter/LUID、可共享格式/usage/外部内存 handle，建立 image layout/外部 ownership 转移和跨 API fence/同步，再由 D3D11 复制/绘入 composition swapchain。Khronos [VK_KHR_external_memory_win32](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_external_memory_win32.html) 定义跨 API Windows handle 导入/导出，但资源可用性必须查询，不能默认所有 Vulkan1.0 驱动都支持。此路线可能还有一次 GPU 内部 copy，但不必回读到 CPU。

普通 Vulkan Win32 swapchain 也应评估，但不能仅因 image 有 alpha 就承诺桌面逐像素透明；必须查询 [supportedCompositeAlpha](https://docs.vulkan.org/refpages/latest/refpages/source/VkSurfaceCapabilitiesKHR.html) 及实际窗口平台行为。保留原生建筑格子的透明开孔、sidebar裁剪、其他信息 HWND 的层级、跨进程鼠标穿透、焦点、DPI/滚屏原子提交、独占/Alt-Tab和设备丢失均需重新验证。

尤其 [CreateTargetForHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice-createtargetforhwnd) 要求 HWND 属于调用进程，所以外部工具可以绑定自己的覆盖窗口，不能直接绑定另一个进程的游戏 HWND。`HTTRANSPARENT` 的[官方定义](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest) 又只明确同线程下层窗口转发，不能仅凭这一返回值就证明新窗口模型对游戏输入透明。

结论：GPU 原生场景合成和 GPU 最终直出是两个独立改造。先让各像素 pass 在一份 GPU frame plan 内完成、最多一次最终回读，可立即避免每素材来回传输；再增加经能力检测的透明 GPU present 后端，才有机会彻底移除这次回读。不能把后者当作 CnC-DDraw quad/Present 的简单复制，也不应为了省回读改用不透明窗口遮掉全部原生 UI。

## 验证等级与未决

本笔记是固定公开源码/API 审查和带宽估算；未运行上游 DDraw、DirectComposition 或 Vulkan/D3D11 共享纹理，也没有硬件性能或实机透明度结论。现有 CPU/GPU 基线 CI `40737404524` 的 Windows Vulkan loader 不可用；实际 Vulkan shader 运行证据来自 Linux llvmpipe，不能用于保证 Windows 跨 API 显示路径。
