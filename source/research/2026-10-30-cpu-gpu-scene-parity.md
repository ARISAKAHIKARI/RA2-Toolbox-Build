# 2026-10-30 · CPU 基础渲染与 Vulkan GPU 场景同源校验及缓存优化

## 问题与依据

用户反馈普通“渲染升级”比 GPU 模式少大量画面元素，希望两条路径对齐并优化 GPU 耗时。核对 `SceneRenderer::render`、`camera_assets::Context::renderActor`、`ViewportCanvas`、`render_upgrade_host.h`、`render_feedback_host.h` 和既有 Vulkan 研究笔记。此次是**源码静态路径审计与合成测试**，不是实际 Windows 游戏/显卡逐帧截图比较；没有得到对应的两端截图，不能说具体缺失物都已修复。

| 类型 | CPU 基础模式 | Vulkan GPU 模式 |
| --- | --- | --- |
| 原生 TMP/地形、矿石/覆盖图、阴影 | 共享原生 bank、材质、光照 | 同一 bank，可 GPU 复用原始页 |
| 步兵、载具、建筑、建造/出售动画 | 同一 `Frame.markers`、`ActorArt` 和顺序 | 同源，目标像素 GPU 合成 |
| VXL、本体炮塔、车辆阴影/坡面 | CPU `voxel::render` | GPU deferred packet、atlas、当前光照 |
| 建筑动画、抛射物、维修扳手 | 同一 marker 和 repair pass | 同一 marker 和 repair pass |
| SHROUD/FOG、地形深度/遮挡 | CPU mask / native Z | 同一采样与 GPU mask / depth |
| 光束、采矿、框选、单位 pips、光标 | 公共 post-scene `feedback` 裁孔/覆盖层 | 同一公共后处理 |
| xBRZ、连续 RGB/扩色、额外 VXL 密度 | 须显式开启画质 | 同样须显式开启画质，不绑定 GPU 开关 |

## 修正的路径差异

1. **Vulkan 与画质解耦**：原 FastPath 将 `qualityGPU=renderExtraQuality && renderVulkanEnabled`，并只在精细模式允许 VXL GPU；不开额外画质时即使勾选 Vulkan，实际上不能执行 GPU native 合成。现在可以独立使用基础 1×/最近邻/原生 DAC/原生光照 + Vulkan GPU 合成，而不偷偷启用 xBRZ、扩色或精细 VXL。失败时仍 CPU fallback。DLSS 仍要求额外画质、精细场景、64位工具箱和 Vulkan。
2. **共享 VXL 图像缓存**：原 `SceneRenderer` 每帧无条件清除所有 VXL sprite，CPU 每次重新 raster，GPU 即使下层 packet/atlas 命中也会再次准备。现在只在实际 `camera_assets` 暴露 VPL revision 时启用缓存：完整 `ImageKey` 覆盖姿态、模型、动画及 shadow posture；原有 palette refresh 会失效缓存，VPL 内容签名变化会主动清除。没有 revision 接口的模拟/外部资产继续每帧重建以避免误用陈旧材料。
3. **可见对象覆盖诊断**：`SceneRenderer` 报告绘制的当前视角 marker 数量/应绘制数，`render_upgrade_host` 在状态文字显示，例如“对象 6/6”。同视角切 CPU/GPU 可以识别素材等待与真正的画面内容分歧，但该数字不代表原版全部粒子/光效。

保留原有独立线程、高频整帧生成、现场光照、同源迷雾和 GPU 一次最终回读；没有修改游戏代码、同步/网络、内存写入或 Hook。

## 性能与未决边界

GPU 仍在 CPU 解析 INI/SHP/TMP/VXL、采样、准备姿态/VPL，绘制完要回读最终 RGBA 给 Win32 layered window；软件 xBRZ/近邻、VXL 缓存或 GPU 合成哪条更快必须按真实帧耗时判断，不能保证 GPU 必定提速。NativeCanvas 局部页缓存与 Stage 的图集/同帧合并仍保留。

**未补齐的原版特效**：部分光棱/激光、矿车采集通过经确认的原生裁孔保留，电弧近似；辐射束、波纹、粒子、部分原生状态 UI 等仍有未完成范围。两条路径共用的 feedback 不会因启用 GPU 自动支持这些缺失效果。用户报告的具体缺失物还需要同画面对比确定。

## 证据/验证

- 变更：`radar_scene.h`、`render_upgrade_host.h`、`tactical_controls.h`、`ui_dependencies.h`、`render_backend_parity.cpp`。
- 定向编译/合成测试：基础与精细均覆盖 6/6 markers；VPL 未变时下一帧 3 个 CPU VXL 的重复栅格调用数为 0；VPL 修订后重新 raster 3 个。旧 `live_voxel_scene` 在无 VPL revision provider 下仍每帧处理 12 个 VXL；`render_upgrade_scene` 的本机画布裁剪/迷雾像素保持一致，`gpu_scene_record` 的同帧重放与 CPU 回退夹具通过。
- 本项没有新增原生 PE 地址、字段偏移、ABI、反汇编证据或原版游戏函数调用。所有游戏地址沿用本目录既有笔记，不对新 PE 作推断。
- **未运行**实际 Windows 游戏/真实 GPU、未测硬件 FPS，也未重新完成 Windows 可执行文件交叉编译；合成测试不等于 NVIDIA/AMD/Intel 驱动验收。
