# 可拆卸超武面板

问题：把 MO_ExperienceEnhancements 的独立超武栏行为接到当前工具箱，支持停靠、拖动、折叠和超武选择，同时复用已有命令桥。

参考固定为 `MagicShiba/MO_ExperienceEnhancements` 提交 `33ed558bd38f36b5012eddcc9c0fb109cc68e38a` 的 `src/sidebar/SWSBar.cpp`。本次是工具箱宿主行为实现，没有复制参考库的注入框架或增加游戏绘制钩。支持目标沿用当前 Runtime 启动校验；本轮没有取得用户现场 EXE 的完整 SHA-256，不据此新增可执行文件兼容性声明。

已有 `bridge.cpp::superSelect`、`external_reader/source/tactical_sampler.h` 使用的地址继续作为依据：当前 House `0xA83D4C`，House 的 Supers 容器 `+0x254`，容器 items/capacity/count 为 `+4/+8/+16`；Super 的 Type/Owner/Present/Ready/Suspended 为 `+0x28/+0x2C/+0x6D/+0x6F/+0x70`；RechargeTimer 为 `+0x30/+0x38`。实例充能总量 `+0x24` 为 -1 时回退 Type `+0xB0`，原始 SHP 图标在 Type `+0xB8`，ManualControl 为 Type `+0xF5`，ChargeDrain 旗标为 Type `+0xE5`、实例状态为 `+0x7C`。新增代码仅只读这些字段；对应来源是固定上游源码交叉比对，未在本轮重新反汇编。

宿主独立枚举最多 256 项，避免现有 Snapshot 的 32 项截断。一次采样要求容器、自家所属方和前后帧一致；异步读取失败立即关闭输入，超过 250ms 隐藏。手动选择进入现有 SelectSuper 队列，最终由 Runtime 重查实例、所属方、存在和准备状态。右键取消只清本地 CurrentSWTypeIndex `0x8809A0`，沿用 SetActiveFoundation thiscall `0x4A8BF0`；不修改超武充能或构造新的发射协议。

标题栏可拖动，箭头切换停靠/分离，减号折叠，滚轮分页。使用现有 overlaystack 的交互窗口注册和层级规则。面板无激活，点击不会把前台切到工具箱。保留原生防御页图标；没有把图标从原侧栏删除，避免缺图、关闭功能或读取失败后丢失原生入口。

地图缩小时逻辑视口 R 可以宽于物理 screenW，因此采样只按 8192 逻辑宽上限校验，不再要求 R 落在物理屏内。Panel.update 的末尾可传入瞬态 `physicalMapRight`（Shared 视口变换的 A.x+A.w，以游戏屏幕像素计）；停靠按该 A 右边界缩放到窗口 client 坐标，避免跟随放大的 R 飘到侧栏外。该几何值不持久化。对应受控测试覆盖 2560 屏幕、4800 逻辑宽。

验证：`assist/tests/superweapon_sidebar_model.cpp` 受控内存测试通过 41 项显示（超过 Snapshot 32 项）、所属方/不存在/ManualControl 过滤、实例与类型总时长、ChargeDrain、暂停及撕裂帧/容器不自洽拒绝；宿主新头文件 Windows x64 交叉编译通过。受控测试不证明 Windows 视觉、点击、图标、拖动或真实游戏/联机运行效果。

边界：SHP 图标直接只读解码；只有 Ares SidebarPCX 而没有 SHP 的条目暂显示名称。没有迁移依赖 Ares ExtData 的阵营列、隐藏/排序配置、PCX 绘制观察器、资金可用性显示或从原防御页去重。原生发射的判定和原有桥的能力不因本面板得到额外保证。
