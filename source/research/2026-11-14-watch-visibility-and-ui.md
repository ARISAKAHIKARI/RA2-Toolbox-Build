# 副相机、显形迷雾与独立面板

问题：1.0.51-pre.1 副相机卡顿；显形叠加穿过黑幕；超级武器面板在菜单期间重现；局内说明过多、控制台说明缺少用途。

依据为用户本轮日志的聚合计数、仓库生产源代码与配套 fixture。未分析新游戏二进制；未新增引擎地址、ABI、字段或补丁。已有几何、Cell 遮罩、display 状态读取沿用原有实现和既有笔记。

## 日志与源码判断

活动进程全部 Runtime 原始采集失败计数为 0。副相机若干窗口出现 625/645、252/255 次任务失败；任务失败还包括源过期、派生场景失败和 sealed read 不完整，不能归为原始采集丢失。条件场景准备均值约 30–41ms，各相机绘制条件均值约 1.5–7.5ms，UI 阶段约 0.47–0.76ms。各均值分母不同，不能相加。direct=0 说明没有直接纹理呈现，不能推出未使用 GPU 计算。

生产 watch Renderer 的 prepareWorld(true) 每次准备整张地图。改为所有实际相机视口的包围范围，复用 SceneRenderer 已有保守地形覆盖和单位裁剪；保留整个地图坐标系及各自 Pipeline。准备失败或 missing read 不扩大旧图像时限。新增拒绝阶段计数、required read 缺页／不可读／非法参数计数；日志在连续失败期间仍发出。GetTickCount 队列差按有符号环绕并截断未来时间，原日志约十亿毫秒不是实测排队。

显形只在原画面上不透明叠加真实 Type/House，目前 compose 后没有任何目的像素迷雾遮罩。新增 special_unit_visibility.h：仅从被绑定的 sealed publication 读取当前局部 native SHROUD/FOG，合成遮罩后再应用半透明强度；全黑像素清除叠加，部分透明压暗 RGB，正常 sprite alpha 不变。原画、已经发布的图像和采样时间均不改。透明／明确全图显示模式使用清晰叠加。新增 Profile 字段只传递显形层的迷雾强度，不改变其他视口的地形 reveal 策略。

超级武器 Panel 已属于 Construction 图层，但 serviceConstruction 的另一调用入口在 paint 隐藏后仍会调用 Panel.update，后者不检查菜单状态而重新显示。服务入口接入相同 passiveDisplayState，Panel.update 与交互入口再次检查当前 display，菜单期间隐藏并清除按压／拖动。

控制台完整选项列表匹配继续保留；说明改为用途、操作、前提和具体差别。局内去掉统计说明与重复状态，保留名字、数量、进度和倒计时；深色橄榄金属配色。

## 验证路径与未决项

- superweapon_visibility_entry.py：执行生产服务函数；pre.1 负面对照重现菜单期间调用 update，修复后菜单／图层／聊天／失焦／关闭全部隐藏。
- superweapon_sidebar_windows.cpp、tools/test_superweapon_sidebar.py：实际 Panel 类 x64/x86，菜单插入按压与释放之间、特殊对话框隐藏及恢复。
- special_unit_visibility.cpp：实际 native selector/raster，黑／半透明／透明、同帧探索变化、合成缓存、图像不可变性、sealed 缺页拒绝。
- watch_world_performance.cpp、tools/test_watch_world_performance.py：真实生产 Renderer／Pipeline 的全图与裁剪对照，RGBA/Z/A、三相机、移动和失败恢复。
- world_frame_core.cpp：所需缺页／已观察不可读／非法读取计数，不改变 required/optional 完整性语义；begin 清零且 scope 结束保留统计。
- construction_paint_test.py、choice_descriptions.cpp、choice_help_test.mjs：既有绘制事务与完整选项帮助校验。

执行结果以当前提交对应 Actions 日志和 verification 包为准。受控性能对照不等于用户游戏 FPS；高失败率的具体现场地址仍需分阶段日志复核。未完成真实游戏、平台联机或真实游戏素材外观验收。原始用户日志不上传。
