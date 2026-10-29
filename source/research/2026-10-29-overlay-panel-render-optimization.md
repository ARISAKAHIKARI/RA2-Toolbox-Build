# 覆盖层、预定面板、小地图与拖屏连续性优化

## 问题与证据范围

用户在 Fix2 上继续报告预定面板等偶发闪烁，希望优化小地图、所有覆盖层与拖屏，并优先将有价值的像素工作交给 GPU。基线远端 `506d94e1bc79c37274fdf0013ae8c00fdaa800f8`，本地同产品树 `8aebf601db74737573d375c5c61545f0aaf9bfe8`。调查前检索 INDEX、持久 notebook v80（95篇），以及覆盖层保活、反馈与性能、小地图连续呈现笔记。

以下是生产宿主代码、像素 oracle、合成 GDI/Win32 边界与交叉编译结果。没有新增游戏调用、Hook、协议或进程写入；游戏 EXE SHA/ABI/字段偏移不适用于宿主缓存修复。用户报告属于实机观察；这里可重现的缺陷不保证是所有 DDraw/DWM/平台闪烁的唯一原因。实际 Windows 与最终产物检查以同一提交 CI 为准，受控测试不是游戏 FPS 证据。

## 临时采样失败不应撤掉整个信息层

原 main paint/tick 在 `snap.renderStatus!=1` 或独立 display 读取失败时立即隐藏 World、小地图、观察窗、预定面板。单次不完整信息采样被等同于已确认的菜单/失焦，因而可能显示一帧原图或空窗口。

新增 `overlay_display_lease.h::ReadLease`：可读且可见的战场与有效信息帧才续期；未知读取只允许保留已有像素，最多120ms，失败本身不续期。窗口/PID/match、客户区和原生尺寸必须与最后有效观察一致。已知菜单、禁用、失焦、聊天或身份/尺寸变化按既有主副屏政策立即撤除。HOLD 不重建内容、不刷新输入授权；预定操作仍要求新鲜显示与原命令校验。

正常热键、队列和鼠标检查共享最多8ms的同上下文好观察，避免每个 `usable` 调用重复三次远程 display 读取；坏读取和 HOLD 不缓存为好观察。paint 的独立新观察始终覆盖该缓存。`render_presentation_gate.py` 抽取实际 main 方法与 paint 入口，验证120ms截止、失败不续期、tick回绕、身份与显隐变化，以及100次 Visible/Fresh 查询只有一次探测。

## 预定面板：只提升完整图像与对应布局

旧 `WM_PAINT` 每次提交异步任务，如果旧结果的 tab/scroll/size 与新请求不匹配，就先 `FillRect` 纯色背景；切页、滚动或布局变化时会可见地空白。服务每100ms重绘，worker 反复创建 DC/DIB/font，稳定面板也重复栅格。

新的 front/spare `PanelBuffer` 在完整 RGBA 准备、上传及 GDI flush 成功后交换，像素与布局一起提升。新窗口有完整前台帧后才显示；待完成任务不覆盖旧完整帧，跨 PID/match/house 不保留。显示与操作视觉 key 不一致或实际窗口尺寸不符时，不将旧图点击映射到新操作。临时信息 HOLD 关闭原生输入策略并跳过目录/工厂刷新，不能把读失败的默认值画成空列表。

精确视觉请求相同时不再次提交；缺失图标1s后有界重试，呈现失败250ms重试。生产异步/GDI模型的1000次稳定更新没有增加 worker jobs、位置调用或 buffer 分配；tab/scroll/queue变化、第一帧、分配/选择/upload/flush/BitBlt失败与上下文变化均有回归。原 host 在同一负对照中触发纯色空白断言。保留现有生产队列与命令入口。

## 统一合成器与窗口层级

原 `UpdateLayeredWindow`/置位的一次临时失败被写进 `error`，随后 capture/publish/group 被当作 compositor 已禁用，生产者转到旧窗口；而合成线程仍会重试，可能在两个 HWND 所有权之间切换。现在只把窗口初始化失败视为失去所有权，暂时提交错误保留统一窗口并接收最新图像；原500ms host heartbeat与750ms layer时效没有延长。

`overlay_stack::place` 在实际矩形、可见性、topmost和前驱锚点都已正确时不再重复 SetWindowPos；游戏再次被提升时仍修复整个覆盖层组，真实层级变化触发检查。保留原250ms完整组查询节流。

首次主 CI `40802191836`（候选 `2d9f8d8b42bdc903507876da3128816c695c3eed`）在实际 Windows `overlay_stack_windows.cpp` 的 immediate-repromotion 检查失败：游戏再次提升后，覆盖层仍在它后面。原节流快捷判断使用“游戏是不是整个桌面的最上方 HWND”；隐藏或其他 topmost 窗口可位于游戏之前，不能由此证明本工具的每一层仍高于游戏。

修复改为在节流窗口内检查每个已显示的工具窗口是否都位于实际 foreground 之前，沿 `GW_HWNDPREV` 有界遍历；健康连续组只需按组成员数量查询，不触发 SetWindowPos。任何成员落到游戏后都立即走既有整组恢复，250ms完整审查仍保留。增加隐藏/可见外部 topmost 窗口和仅部分层落后两类反例；实际 Windows 的原失败断言没有删除或放宽。最终新版仍必须重新运行同提交 Windows CI。


`Incremental` 保留最多64个局部脏区域，按原层序重算并仅复制相应 DIB 区域，超过预算退到完整重算。256帧与完整合成逐像素对照通过；4K两端小变化由旧联合包围框6,959,064像素降至2,080像素，稀疏透明孔样例只重算3像素。最终 UpdateLayeredWindow 仍为整窗提交，不能把局部 CPU 复制说成局部窗口上传。

## 小地图：成功呈现后再确认与复用

旧 PaintGate 在触发 WM_PAINT 后无条件记为完成，即使 GDI 或 present 失败，也等后续新帧/周期修复；新 HWND 还可能先显示空客户区。新路径先在 spare 内完成地图、标记和视口矩形，再交换并显示；实际 present 成功才确认 gate，失败可立即重试。重绘曝光和1s保活复用已有 front，不再重新 HALFTONE/绘全部标记。

视觉 key 采用不可变 image owner、实际投影后的 marker/view rectangles，而不是完整 worker Frame 指针；相同图像的新包装不重画。标记改用 stock DC_BRUSH，避免逐个分配/销毁 brush。保留的前台帧用 game HWND/PID/match/mode/reveal/完整几何隔离，不能在换局或视野政策变化后保留旧世界。

`DiagramRenderer` 对新 Terrain bank 的 geometry/reveal/colors 做精确比较，相同内容保留同一输出图像并释放旧 Terrain owner；颜色、几何、尺寸、reveal或reset变化仍重新栅格，并与新实例输出比较。这里只减少重复缩放，不改变小地图裁剪、导航或迷雾准入。[GPU分页面积缩小](2026-10-29-minimap-gpu-area.md)单独记录，不将宿主 GDI 缓存描述为 GPU 绘制。

## 同一像素预算下的拖屏预取

原96个原生像素的对称边框仍有距离上限。`CameraPrefetch` 按前后两次实际相机观察与上一完成帧耗时，有限地偏移渲染 crop 中心；每轴最多 `floor(min(64, 实际边框×2/3))`。Frame 记录的是该源图像的真实渲染原点，显示仍截取实际当前相机，不改变游戏相机、不外推单位或光照状态，也不增加像素数。

停止/反向按下一次实际观察调整；世界/投影变化、帧号倒退、dt为0或超过250ms、超过384原生像素的跳转及无效耗时不作预取。原相机始终留在完成图像范围内。正常边框下前进余量可由96增至160，反向余量会减至32；在途帧不能追溯更改，大幅拖屏、急反向和瞬移仍可能退回原图。

`render_scroll.cpp` 比较17,358,000个模拟像素及15,600个生产 SceneRenderer 的原生/1.5x xBRZ地形像素，优化与ASan/UBSan通过，Win32 x86交叉编译通过。特定120ms移动120原生像素案例，旧边框拒绝而前移64后的图像能覆盖；不能把该例推广为任意拖屏速度。非整数倍率仍有既有最多半个目标像素的移位量化。

## 回归入口与限制

- 原有与新增生产回归：`render_presentation_gate.py`、`render_atomic_camera.py`、`construction_host_test.py`、`construction_paint_test.py`、`minimap_rate_test.py`、`overlay_keepalive_test.py`、`radar_paint_gate.cpp`、`radar_diagram_cache.cpp`、`overlay_incremental.cpp`、`render_scroll.cpp`。
- 实际 Windows GDI/窗口夹具：`construction_buffer_windows.cpp`、`radar_panel_windows.cpp`、`overlay_compositor_retry_windows.cpp`；本地仅交叉编译，实际运行交给同提交 Windows CI。
- 受控完整合成、面板/小地图缓存与拖屏回归已执行 ASan/UBSan（detect_leaks=0，不宣称LSan）。没有真实显卡、用户游戏 FPS 或 DWM 闪烁实测。

原生游戏世界绘制没有关闭；[原生绘制分离研究](2026-10-29-native-render-suppression-feasibility.md)解释已确认的阶段和不能当作开关的数据。不能把隐藏原图、跳过 Present 或这些宿主优化说成原引擎已停止渲染。
