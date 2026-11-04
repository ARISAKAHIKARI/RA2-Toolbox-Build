# Hotfix19：保留世界输入、当前相机重绘与独立窗口呈现

用户反馈快速拖屏残影、右键连续拖动时重复已绘制区域、地图边缘黑条和右侧 UI 黑屏，要求停止逐项诊断、按 Vera20K 成套迁移画面相关路径。本次是可编译的 C++ 呈现管线适配，**不是把 Vera20K 的 Rust/wgpu 引擎及模拟器原样替换进工具箱，也没有完成所有原生 UI／MOD 效果的逐像素移植**。

## 固定来源与实现对应

上游：Vera20K，YuriPlanet 及贡献者，GPL v3，提交 `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`。延续 Hotfix18 已接入的素材、阴影、深度及有序特效实现。

| 上游路径 | 本次采用方式 |
| --- | --- |
| `src/app/presentation/render/build_instances.rs` | 世界、UI、侧栏分别准备输入；本项目 `worldscene::Snapshot` 保存完整地形页、世界坐标对象／阴影调用、效果状态和素材引用。读取及解码与相机绘制分离。 |
| `src/app/presentation/render/mod.rs` | 一次实际相机采样用于地形、对象、阴影、效果、迷雾和对象反馈；`worldscene::Pipeline` 执行当前视口整帧。 |
| `src/render/batch.rs` | 保留素材和几何，按本次相机重新记录绘制；不通过平移旧 framebuffer 生成新视口。现有 Vulkan C++ 后端承担执行，不链接 wgpu。 |
| `src/app/presentation/render/draw_passes.rs` | 世界／效果限定战场 scissor；本项目同时限定 Surface.clip 和外部呈现 HWND 的窗口区域。游戏自身侧栏保持独立。 |
| `src/render/screen_presentation.rs` | 上游震屏复制不是普通相机拖动实现。本次没有将该复制用于拖屏；旧预判、旧帧平移、边缘复制和拖动留帧分支已删除。 |

## 进入生产路径的改动

- `SceneRenderer::prepareWorld(true)` 在主视口准备完整世界输入。相机移动不缩小或销毁地形页、不提前剔除视口外对象。素材更新仍通过已有的不可变页和图像所有权发布。
- 独立 presentation worker 在提交前读取实际相机，使用最新准备好的世界输入绘制整个当前视口；素材解码、地形采样、对象采样各自保留独立任务。CPU 和 GPU 主视口都使用此流程，CPU 地形解析结果按相机／地形版本缓存。
- 发布完整帧与它实际使用的相机。输入与场景帧同一原点，不再把旧图像偏移／扩展到新相机。源状态年龄上限500 ms，完成帧年龄上限100 ms；失败不刷新时间戳。相机不匹配时移除原生透视孔，不能透出错误坐标的旧原生像素。
- `extendEdges`、`CameraPrefetch`、`scrollVisible`／`scrollUsable`、`holdDuringScroll` 已从实现删除。持续拖屏不依赖鼠标按键释放才更新。
- GPU 模式使用 `WS_EX_NOREDIRECTIONBITMAP` 并移除 `WS_EX_LAYERED`；回到 CPU 时恢复 layered 输出。世界及反馈只占战场区域；外部呈现窗口另以当前图层边界的并集裁切。
- 主／副屏观察窗接入不可变共享 GPU 图像；尺寸缩放和边框由 GPU 合成。多个 HWND 共用的 D3D11 immediate context 用零等待互斥锁串行提交；共享纹理 SRV 从创建时复用，并持有 keyed mutex 到读取命令提交结束，移除上一版额外的整帧 `CopyResource`。
- 直接输出池允许最多12个槽，总像素存储仍为384 MiB。消费者持有帧时不覆盖其资源，防止多个观察窗之间复写输出。

## 原生依据与证据范围

沿用既有支持 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`、`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。本次没有新调用游戏函数，没有重新反汇编或运行这些 EXE。

沿用已记录只读布局：Tactical 指针 `0x887324`，相机坐标 `+0xB0/+0xB4`，战场矩形 `0x886FA0`，屏幕尺寸 `0x8A00A4/0x8A00A8`，Scenario `0xA8B230`，House `0xA83D4C`，逻辑帧 `0xA8ED84`。协议仍为 Assist80、Runtime4；未更改引擎 ABI、事件或模拟频率。

证据见 `evidence/render-hotfix19-final.json`：当前相机／完整地形缓存的172800个 CPU 像素与直接重绘一致；已有发布时效、反馈原生孔和特效计划夹具按移除的旧策略更新。以上为合成内存与 CPU 输出，不证明 Windows 可视结果。静态 HLSL/SPIR-V 编译、x64主程序／x86 Runtime和broker交叉编译及632项跨位数ABI检查分开记录。没有创建 Vulkan／D3D11 设备，没有运行 Windows loader 或游戏，没有 FPS 或输入延迟测量。

## 尚未完成及否定的说法

全图小地图仍使用现有共享 SceneRenderer 输出；原生侧栏、部分放置／特殊 UI 仍是原生整合，不是上游侧栏重建。CPU仍负责状态、源素材解码、静态地形和几何准备。A/Z、RGB565、抖动 RNG、特殊波形和任意 MOD UI 不宣称逐像素一致。

静态源码和合成结果不能证明真实残影、黑条或侧栏黑屏已经完全消失；这需要 Windows 游戏画面确认。整套相机呈现路径替换不等于完整引擎移植，采样目标1000 Hz不等于真实显示1000 FPS。没有采用“继续扩展旧图像即可补足缺失地形”作为完成条件。
