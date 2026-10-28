# 渲染覆盖层、滚屏连续性、采样分配与迷雾边缘修复

## 问题、基线与证据范围

用户实机报告：渲染升级盖住信息层、覆盖层周期卡顿、拖屏退回原图、底部迷雾出现大色块且刷新迟缓，并建议保留游戏内存指针以减少重复采样开销。基线为 pre.6 Fix1：远端 `d8c7633cfb07a070388d8f018656f41bcc2c8bcc`，本地同产品修改 `c277eb99913ada9f6c58f440154a9aaf1f66be87`。调查前检索研究索引、既有覆盖层/原生迷雾/呈现连续性笔记及持久 notebook v79。

本笔记依据生产宿主源码、受控读取/窗口边界夹具、像素对照和用户截图；不新增游戏调用入口或修改游戏进程。没有执行私有 EXE 的新增原函数，因此本项不以某个 EXE SHA/ABI 作为已重播证据。指令线、放置格和 VXL 原函数的样本身份、地址及调用约定分别记录在[原生反馈](2026-10-29-native-order-placement-feedback.md)及[覆盖图与坡面](2026-10-29-overlay-opacity-voxel-posture.md)笔记。未把夹具计时说成实机 FPS、DWM 呈现延迟或联机验收。

## 两种层级必须同时正确

问题同时存在于共享像素 Surface 顺序和实际 HWND 顺序。原场景 Surface order110 高于 World10/Statistics20；共享 compositor HWND 的旧层级90又高于 Minimap30、Watch40、Aircraft50、Construction60。只修其中一种仍会遮挡另一种覆盖层。

场景 Surface 和 `overlaystack::Layer::RenderUpgrade` 均固定为 **5**，位于 World10、Statistics20及其他信息/交互窗口下方。旧设置迁移为“场景固定在信息层下方”。场景使用目标视口 clip，避免扩展边框覆盖侧栏。建筑放置反馈另用已验证 SHP alpha 生成的单行 cutout，具体规则见原生反馈笔记；稀疏孔为 source-relative、h=1、最多16384条，非法范围拒绝发布。

`overlaycomposition::Incremental` 保留不可变图像引用，只重合成变化 Surface 的新旧范围并更新相应 DIB 区域；切换/删除/排序、clip、cutout 与上下文变化都纳入失效。`overlay_incremental.cpp` 的 **160 例**与全量合成逐像素一致。4K 背景上每次横移1像素的32×32光标，30帧共重合成31680像素，即1056像素/帧。此计数只证明 CPU 合成工作范围；既有约0.016ms/帧计时不包含 DIB 拷贝、UpdateLayeredWindow、DWM、游戏或调度，不能据此推出显示帧率。

## 周期工作、呈现频率与读取存储复用

统一覆盖层使用 compositor 维持可见性，但 World 旧 Recovery 仍观察刻意隐藏的 legacy HWND，约每250ms强制一次 GDI+ 栅格工作。`world_presentation.h` 在 unified 模式暂停该旧窗口恢复计时；Surface 心跳及真实 legacy/focus/error 恢复仍保留。生产 helper 的20秒模拟验证 unified 模式没有这种周期强制栅格，legacy 恢复仍有界。不能由此断言用户所见每一次卡顿都已定位。

独立整合审查发现：去掉该周期补画后，只有固定提示而无移动标签时，旧画面会因 camera 身份不符被拒绝，却没有像素变化来触发新工作。新增 `ContextRevision` 同时观察已提交 camera 和采样 camera，任一变化即增加 paintRevision；后者保证采样落后一轮后仍会重试。静止相机不因此恢复定时栅格。`world_presentation.cpp` 的优化及 ASan/UBSan 验证通过，保留已有 camera 不匹配拒绝条件。

compositor 提交频率采用 `min(用户请求5..500Hz, 已确认的当前显示器Hz)`，微秒周期向上取整；DEVMODE 的0/1、查询失败或未知模式**不猜成60Hz**，保持请求值。显示模式查询位于 compositor 线程，稳定目标每1000ms至多一次；窗口/显示器变化和 WM_DISPLAYCHANGE 立即失效。它不降低游戏时钟、采样、光标或 worker 的请求频率，也不保证操作系统按此节奏显示每帧。`overlay_present_rate.cpp` 和受控 `overlay_present_monitor_test.py` 覆盖已知/未知、失败、切屏及 tick wrap。

`ProcessReader` 过去每个采样 epoch 销毁页缓存节点，重复分配4KiB缓冲。现在按远端页地址保留 **8192页/32MiB** 的存储槽和 epoch 标记，新 epoch 第一次访问仍从游戏重新读取；只复用分配，不复用上一 epoch 的字段值。到达容量以 C++17 node extraction/rekey 替换最旧地址，关闭进程时全部清除。部分页读取失败不会把残缺缓存标记为有效，仍允许有限字段回退。

`process_reader_pages_test.py` 同时检查新 epoch 字段确实变化、部分读取失败/重试、关闭失效、超过容量的地址轮换；稳定8页在预热后100个 epoch 的全局 `operator new` 计数增长为 **0**。这是受控稳定工作集结果，不等于所有场景零分配。缓存对象地址/关系可以减少查找，但外部进程地址不能在工具箱中直接解引用，仍需远程读取、身份及世界状态校验；地址槽复用也不是跨进程原子快照。

状态文案移到发布完整组与提交下一工作之后，稳定状态每250ms最多构建一次，相同文本不重复 WM_SETTEXT；状态/代际变化立即更新。它不降低画面、光标或 worker 更新率。

## 滚屏使用已绘制边框，原子提交相机和整组图像

worker 开始工作时采用当前相机，在世界身份和投影不变的前提下允许相机移动。渲染预留 **96个原生像素**边框，再按实际输出倍率换算；遇8192边长/16M像素上限时缩小 padding，不降低采样质量或输出分辨率。呈现阶段平移并裁剪已完成的共享图像，同时变换物体 bounds 和放置 cutout，不复制整帧。

4K/2×示例：原视口1920×1080，输出3840×2160，含边框4224×2544，栅格工作量增加约29.6%。这不是免费的性能提升。相机移动超过已绘制边框、投影/世界变化或旧图超期仍退回原生画面；不能声称所有拖屏都永不回退。已完成图的完成年龄上限1500ms，样本年龄上限2500ms；短暂视口读取宽限120ms不是正常帧等待目标。

旧 `configure(camera)` 再 `replaceGroup(layers)` 存在两次锁之间的相机/图像不一致窗口。现在 `configure(...,deferCamera=true)` 只返回拟用 Context，`replaceGroupAt` 在同一锁下验证组/身份并一起提交相机与图像；原默认 API 行为保留。反馈读取若只遇相机变化最多重试一次，世界或视口变化不无限重试。

`render_scroll.cpp`：49组位移、17150000次像素比较、真实 SceneRenderer 的9组整数裁剪一致；1.5×分数平移最大目的像素误差0.5。`render_atomic_camera.py` 使用实际 Host 方法，先复现旧 configure 空档，再验证1000次并发观察下的原子提交及非法提案拒绝。优化版与相关 ASan/UBSan 均通过；未替代真实 Windows 滚屏图像验收。

## 迷雾大色块是缺少源 TMP 露出底色，不能只平滑遮罩

用户截图 `image(20261028-160841).png` 的底部/右侧色块精确匹配 `terrainColor(land,level,false)`：

| RGB | 生产粗底色匹配 |
| --- | --- |
| 110,129,83 | land0、level4 |
| 120,125,117 | land3、level4 |
| 127,123,85 | land7、level4 |

截图像素观察只确定露出的颜色。独立生产夹具进一步重现因果：完全隐藏的**源格**被提前剔除，但其抬高/extra TMP 像素本应进入已探索的**目标区域**，结果那里露出60×15粗色块。旧基线同一关键断言得到 `ff66794b`、mask127、tile loads0；新路径必须得到实际TMP `ffa03020`。

采样保留合法隐藏源格的 TMP/ground overlay 元数据及 `sourceColor`，新增 `CellArt.sourceHidden` 使用原padding而不增大结构体；原 `colors` 的 actor/diagram 可见性准入保持不变。主视口让最终**目标像素迷雾**决定遮挡；普通小地图仍保留原隐藏源策略。遮罩收集范围按真实60×30 SHP交集，而非雷达2×1平面格足迹，覆盖可见半格的上/右边缘。不能把“保留绘制源”解释为显示未知区域的物体。

仅改变探索/迷雾不再重建静态地形或使 xBRZ cache 失效；真实资源帧变化仍失效。为避免因此栅格整张未知地图，`terrain_coverage.h` 对当前视口使用256像素 padding/quantum及4像素滤波halo，保守纳入512×512 TMP union可能触达的源格。夹具完整地图32640个源，当前区域绘制4624个、289页；16像素平移不重建，离开范围重建背景但保留已解码素材。

`radar_live_shroud.h::Live` 每轮直接读取当前视口及选择器邻居的可见性，生产 `SharedReadView::fresh` 绕过地形/资产旧页。缓存仅在本次 capture epoch 内合并相邻远程页，下一次采集刷新触及的页；同逻辑帧内位变化也能观察到。前后检查 scenario、house、cells、bounds、offset 和帧跨度，失读/变化拒绝发布；这仍不是冻结游戏后的原子快照。

当前实现每次读取完整格指针表 `512×512×4`，即 **1MiB**，不能说每帧只维护少量指针状态或完全不复制。900×450受控区域示例：496个遮罩源、74次fresh调用、40个页、1212862字节（包含该指针表）。这是仍可继续优化的带宽开销，不应被缓存收益掩盖。

`live_shroud_scene.cpp` 验证保留同一旧地形bank即可看见实时显隐、同帧位变化、跨页/读取失败、真实SHP边界交集、可见性变化零新素材加载/静态重建及资源帧变化的确失效。相关9项回归通过。`live_scene_lighting.cpp` 的旧多pass夹具填了4097个非法/屏外数组槽，新覆盖范围正确跳过后不再跨预算；现改为4097个范围内合法源，保留原所有全局/局部光照变更及旧bank拒绝断言，优化版重新通过。

**仍存限制**：实时迷雾目前和 worker 完成的场景一起发布，没有独立立即提交的遮罩层；渲染耗时仍会形成可见延迟，失败时旧完整画面可能保留至上述年龄上限。不能声称迷雾延迟已消除，或截图问题已经过用户实机复核。

## 验证记录

汇总见 [render-feedback-performance.json](evidence/render-feedback-performance.json)。GPU集成前的CPU portable回归 **42/42通过**，另有强制纯量缩放、进程读取、素材、原子相机、保活和呈现频率夹具。Windows compositor/stack等已交叉编译；本记录不提前宣称正在进行的新构建CI执行通过。后续 GPU 阶段另立笔记与验证，不以本次CPU结果替代其实际dispatch或回读测试。

首轮主集成 Windows CI `40736169061` 在 `live_shroud_scene` 失败。Zig 0.13 的 Win32 `-O2` 默认定义 `NDEBUG`，夹具中的 assert 准备步骤被跳过；同轮增量测试甚至报告 0 次重画，不能把其 PASS 文案当作断言已执行。所有 Windows 测试编译现在明确加 `-UNDEBUG`，产品构建选项不变，并重跑完整检查；另修正该夹具在 32 位平台的 uint64 打印格式。Linux g++ 测试的断言原本已启用，此前的 42 项结果不受这项测试建置问题影响。
