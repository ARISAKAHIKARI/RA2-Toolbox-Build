# 渲染与退出补查：阴影、原生特效／UI、素材动画、拖屏与控制台响应

## 用户问题与证据范围

用户报告四项问题：坦克在斜坡上阴影位置不对；光棱攻击、矿车采矿等效果没有显示；刷新卡顿、延迟较大，随后明确为“经常跳帧”；右键快速拖屏仍会短暂出现原生画面。

在本轮尚未最终冻结／构建时，用户追加：UI 有时失效，飞机子弹数、IFV 载员等多种原生 UI 不全；高素材输出开启后部分动画错误；SHP 单位炮塔缺失；最严重的是每次进游戏再退出，控制台窗口仍在，但操作无效、点关闭也无效。最后一项不是“窗口打不开”。追加问题与前四项已完成局部检查分开记录，尚未取得证据的原因不能写成已定位或修复。

调查前已查阅研究索引、[覆盖图与 VXL 坡面](2026-10-29-overlay-opacity-voxel-posture.md)、[原生命令与放置反馈](2026-10-29-native-order-placement-feedback.md)、[反馈与性能修复](2026-10-29-render-feedback-performance.md)、[宿主闪烁与性能优化](2026-10-29-overlay-panel-render-optimization.md)，以及同一持久笔记 v84 的相关记录。此前明确未认证 `Shadow_Matrix`，普通攻击特效也不因已有单位／弹体渲染就自动获得支持。

新增原函数检查仅执行授权映像中的指定 x86 函数，使用合成对象和受控绘制边界，不是完整游戏、实际 Windows 桌面或真实显卡帧率测量。私有 EXE、原指令字节、原始内存与用户素材不进入公开证据。宿主的帧调度、裁剪与相机一致性修改不新增游戏调用入口、Hook 或共享 ABI。

## 坡面阴影

两份已授权引擎映像的 SHA-256：

| 样本 | SHA-256 |
| --- | --- |
| sample3 | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| sample4 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

旧阴影将模型压平后按平地绘制，并以整数 Cell Level 推算落影高度；本体已有 Drive／Ship 插值姿态，但阴影未采集独立 `Shadow_Matrix`。原生阴影使用当前格坡面与精确地面高度，不能因为车体已经正确倾斜，就认为阴影路径也已包含这些状态。

| 原函数／字段 | 用途与证据范围 |
| --- | --- |
| Drive `0x4B0410`、Ship `0x69FB20`、base `0x55A7D0` | `Shadow_Matrix`，ILocomotion 虚表槽 `+0x28`；stdcall，栈为 interface／out matrix／index，`ret 0xC` |
| Cell `+0x11C`；`0xB45188 + ramp*48` | 当前格 RampType 和对应 Matrix3D；不使用移动器平滑过渡中的车体矩阵 |
| base `0x55A8C0` | `Shadow_Point`，stdcall，interface／out，`ret 8` |
| Object `0x5F5F40`；`+0x9C/+0xA0/+0xA4`、`+0x8C` | `GetHeight`，thiscall；坐标与桥上标志 |
| Map `0x578080` → Cell `0x47B3A0` | 实际 `GetFloorHeight(Coord*)`，thiscall、`ret 4`，按格内 XY 与坡面计算地面高度 |
| Cell `+0x11B`；`0x89E7C0` | 有符号 Level 与每层世界高度 |
| `0x89E770 & 7`、`0x89E740`、`0x81C900+(ramp-1)*40` | 坡面表初始化标记、scale，以及每项五个 double：xCoeff／yCoeff／add／max／offset |
| `0xAC13BC` | 桥面高度修正 |
| `0x6D20E0`；`0xB0CD48` | 世界 Z 到屏幕高度投影；ECX 为 Z，无栈参数，使用实际 scale |
| `0x753F90` 绘制路径 | 反汇编路径：阴影足迹经 Shadow_Matrix 变换后清世界 Z，再作视图投影；不在下述原函数执行矩阵内 |

`assist/tests/voxel_shadow_native.py` 每份映像执行 672 个阴影矩阵用例、10 个 Shadow_Point 用例，以及 1,575 个实际 Map→Cell 地面高度用例。矩阵和 Shadow_Point 组使用合成对象／坡面及明确替身化的地图边界；独立 floor 组实际执行原生坡面表初始化与 Map／Cell 函数，不替身化 floor 结果。表数据在执行中产生，不导出原始表字节。

两份合计 1,344 个矩阵和 3,150 个精确 floor 用例，与实际编译的 `voxel_shadow.h` helper 比较；矩阵系数最大误差小于 `6×10⁻⁸`，floor 高度误差为 0。地面 Z=104 且物体同高的受控用例，原生落影偏移为 0，旧整数 Level 算法却下移 15 像素；桥上例也需要独立桥高修正。精简证据含各原函数区段 SHA：[voxel-shadow-native.json](evidence/voxel-shadow-native.json)。

生产 `voxel_shadow.h` 分别采集当前格坡面、精确地面高度和阴影状态，保留原车体 Draw_Matrix 路径；采样身份／表或字段不一致时拒绝本轮结果。不据此扩大为所有 locomotor 的阴影支持，当前原函数认证范围为 Drive／Ship。`visual_pose.h` 采样独立 shadow 状态，`camera_assets.h` 两条栅格准备路径使用相同投影，`radar_scene.h`／`watch_engine.h` 使用精确 drop 和独立阴影姿态缓存 key。

另需区分 `Shadow_Point.x=0` 与原生最终绘图的偏移：既有[原版 VXL 光源笔记](2026-10-25-native-voxel-light-direction.md)已由四份 valid／stable 捕获确认 `0x887420=(2.9999964237,0,0)`；此次 `0x754029..0x754056` 反汇编显示它在视图投影后另外相加。补丁保留默认约 +3 像素，没有新增 live `0x887420` 采样，动态／MOD `ShadowShift` 仍未支持。足迹仍采用既有平面 mesh，不能声称完整逐体素柱／像素与原版相等；本轮修复范围是 CPU／GPU 共用的精确 drop 与当前格 Ramp。

`voxel_shadow.cpp`、既有 `voxel_posture.cpp` 和整套 `game_assets_host_test.py` 通过；资产 fixture 验证坡面独立改变阴影轮廓、GPU packet 的 CPU fallback 与直接 CPU 结果颜色／深度完全一致，以及车体／阴影各自姿态变化只失效对应 packet。这是生产数据准备与回退一致性检查，没有实际执行新阴影的 Vulkan dispatch 或硬件像素对照。

## 光束与矿车采矿效果

旧场景覆盖没有采集这些原生效果；已有弹体／单位素材绘制也不能生成未读到的激光或采矿状态。对已认证的 Laser／HouseColor 光棱路径，新增 `native_beam_effects.h` 只读取活跃绘制对象并生成细带 cutout，让游戏已经绘制的真实像素透出，保留原生颜色、深度和混色。细带包含少量保护边，局部背景也可能透出；不是工具箱独立重绘全部攻击效果。

| 原函数／字段 | 认证用途 |
| --- | --- |
| `0x550260`、`0x5509F0` | Laser Draw、HouseColor 绘制路径；均为 thiscall、无栈参数 |
| `0x6D2140` | 实际坐标投影入口，`ret 8` |
| `0xABC878`，数组 `+0x04/+0x10` | Laser 活跃对象列表指针／count |
| Laser `+0x24/+0x30` | 起点／终点世界坐标 |
| `+0x20/+0x1C/+0x4C/+0x51` | HouseColor 标志、厚度、duration、跳绘／闪烁标志；使用原生 Draw_It gate，不另造生命期时钟 |

`native_beam_capture.py` 对上述同两份映像各执行 1,024 个原指令几何用例，覆盖 8 个方向、8 种厚度（1..64）、普通／HouseColor、Supported、高低质量和透明分支。观察到 HouseColor 逐层 halve byte RGB，支持路径最多留下 6 个 offset steps；轴向可在单轴偏移 ±6 像素。此前按斜线推测的 ±3 包围范围被原函数轴向反例否定，因此生产 corridor 按实际最大偏移另加 1 像素保护。

两份合计 2,048 个原指令几何用例，在 1×／2×／4× 显示比例下共 6,144 项 corridor 覆盖检查通过；生产 fixture 另验证 duration／flicker、列表变化和重复地址拒绝、单行 runs、不同对象重合光束的合并。精简证据：[native-beam-cutout.json](../verification/native-beam-cutout.json)。

原生证据不支持自行添加“最多 128 个活跃对象”的 gate；生产路径移除该限制，129 个有效对象以及 129 个含 128 个 inactive 的回归均通过。光束内部先合并重合单行区间，再由宿主对全部特效／放置孔合并后应用 16,384 条 compositor 上限，避免重复光束耗尽最终预算。

这些用例认证原生提交的绘制几何及其 corridor 包含关系，RNG 替身固定为 0，绘制终点为受控替身；没有执行 native 像素栅格／AA 或实际 Windows 游戏画面。RadBeam 与 ParticleSystem 仍未列为支持项；不能据此承诺所有武器／MOD 特效。原生孔均沿用 exact-camera 校验。

矿车另认证 sample3 和 sample5；sample5 SHA-256 为 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`，不能把 sample4 的认证范围误套到该样本。`tools/native_mining_effect_audit.py` 执行原指令区间 `0x73D0D1..0x73D29C`，预置 ESI=Unit、EBP=brightness 与所需栈局部；不是单独调用完整游戏绘制函数。实际执行 Drive `Is_Moving @0x4AFC20`／`Is_Powered @0x4AFB80`、Facing `0x4C9480`、量化 sin／cos `0x4CACB0/0x4CAD00`。扩展 phase 边界后，每份 2,352 用例中，588 次到达真实最终 draw 调用边界，1,764 次被 gate 拒绝绘制；两份合计 4,704 用例，取代早期每份 1,008 的局部矩阵。

| 矿车字段／入口 | 已支持用途 |
| --- | --- |
| Type `+0xE0E`；Unit `+0x6D2/+0x538` | Harvester、IsHarvesting、动画 phase |
| Unit `+0x270/+0x271/+0x278` | 隐藏状态与特殊绘制状态筛选 |
| DriveLocomotion `+0x30/+0x3C`；`0x8A0790` | next／destination 与 EmptyCoord，限定已证明的静止 Drive 条件 |
| `[0xB1CF98]`、`[0x87F6C0]` | 原生 Gather SHP 与 converter |
| 最终 `CC_Draw @0x4AED70` | 探针捕获其调用参数；flags=`0x2A00` 含 Centered，最终像素栅格为替身 |

实际 frame 为 `(7-rounded8Facing)*15 + (phase+Frame)%15`，其中相加按 32 位、余数使用有符号语义。新增 phase=8192／65536／INT_MAX／-1 的原函数用例后，移除未经原生证据支持的 phase<8192 门禁。效果世界偏移使用原量化 sin／cos × **30.0**，不能套用未经认证的 160。原 x87 运算保留微小负分量，cardinal 方向可能产生 -1 的坐标差；生产 helper 保留该截断结果。GetCoords、两项 hidden 检查、最大速度 getter=256、GetZ=17、Tactical 投影=(100,200) 和最终 SHP raster 均为明确受控边界。证据：[mining-native-stencil.json](evidence/mining-native-stencil.json)。

另只在 sample3 做静态 getter 核对：Unit 虚表 `0x7F5C70` 的 `+0xAC` 指向 wrapper `0x41BE00`，再调 `+0x48` 的 `0x5F65A0`，后者复制 Unit `+0x9C` 起三个坐标到 out、`ret 4`。这支持采样 Location 的选择；未检查 sample4／5 的这条 getter 链，两份 mining probe 中 GetCoords 仍是替身，不能改述为已实际执行完整 getter。

`native_effect_stencil.h::EffectStencil` 按真实当前 SHP 帧的非零索引 alpha 生成孔，保留游戏的颜色／光照／遮挡；未知／MOD locomotor 不猜测采矿条件。只在有活跃矿车时读取真实 `0xB0CD48` scale，失读／零／NaN 不用预览默认值替代。采矿、beam、单位 UI 与 foundation 孔最终合并为 h=1、最多 16,384 条。`NativeFailureGuard` 在 mask 失败，或上一组／当前组含原生孔而后续 selection／cursor／view 反馈失败时，标记 unsafeView 并清孔；取消状态跨一次重试保持，禁止旧孔宽限续用。成功得到无孔反馈后恢复普通 120 ms 宽限；从未有孔时的普通短读失败也保留原宽限，避免新读取反而使整层闪退。`native_effect_stencil.cpp` 优化及 ASan／UBSan（关闭 LSan）通过；集成的 `render_feedback_host.cpp` 验证上述失效、beam 消失、相机变化及边界单行裁孔，`validate_native_feedback.py` 通过。没有实际 Windows 游戏画面验收。

## 经常跳帧与帧调度

`shared/source/overlay_compositor.h` 原来完成 CPU 合成和 `UpdateLayeredWindow` 后，再休眠一个完整目标周期；真实周期成为 `work + period`。例如目标 144 Hz 的周期约 6,945 μs，合成／提交工作另耗 5 ms 时，额外完整休眠会使周期显著超过目标；频率设置本身不能抵消该等待。

现立即准备下一幅图，在 ULW 提交前按上一次实际尝试时间加 `ceil(1e6/hz)` 算截止，仅用可中断 `cv.wait_until` 等待剩余时间。工作超过周期时不再追加休眠，变长工作后也不追赶式连续短间隔提交；保留当前显示器已确认频率上限。暂停、停止、世界／相机／配置变化可中断等待。小地图 worker 同样改为从本轮开始计算截止时间，采样／绘制完成后只等待剩余部分。

`overlay_present_rate.h::Cadence` 的生产 helper 在 `overlay_present_rate.cpp` 中验证 18,000 帧：60／144／240 Hz，以及固定 0／2／4／25 ms、交替 2+18 ms、2+2+18+2 ms 工作；动态频率和停顿 5 秒后的恢复也通过。`minimap_rate_test.py` 抽取实际 Worker 并控制时钟／CV，144 Hz 下 5 ms 工作仅等 1,944 μs、18 ms 工作零额外等待、2 ms 工作仅等 4,944 μs，检查通过。这里验证调度计算和生产路由，不是用户游戏帧率实测。

Host 的 `uploadStatistics()` 统计成功 ULW 完成次数、连续提交间隔、连续最大间隔与失败次数；闲置没有新 revision 时中断连续统计，隐藏／暂停清间隔和最大值，换上下文清完整统计。计时来自 `steady_clock`，不能解释为 DWM 实际显示 FPS。现有 Windows retry fixture 加入统计／失败／暂停断言，x86／x64 两项交叉链接通过，未在 Windows 执行。证据为 `render-latency-private/result.json`。

减少额外等待不能保证重场景自身的 GPU／CPU 栅格工作满足 144 Hz，也不取消已有观测平滑的显示延迟。最终 ULW 仍经过 CPU／DIB 像素路径，本次没有把窗口呈现宣称为纯 GPU 零回读。

## 右键快速拖屏

旧 `render_presentation.h::scrollUsable()` 要求已完成源图的 overscan 完整覆盖当前视口；默认每侧 96 个原生像素，前向预取偏移 64 后反向余量只剩 32。快速拖屏越过任一边界，render host 会清掉整组场景，使整屏跳回原生画面。

新 `scrollVisible()` 按实际相机平移并裁出已完成图与当前视口的真实交集，保持交集中的高分画面，只让尚未渲染的边缘条带透出原生画面。没有凭空生成新区域的纹理；完全没有交集、世界身份改变或完整帧超期时仍按既有保护退让。`sceneCutouts()` 同时把原生反馈孔裁到源图边界，避免越界孔导致 compositor 拒绝整层。

`render_feedback_host.h` 原先把前后采样期间只移动相机也视为 `unsafeView`，立即撤掉整组反馈。实际世界反馈按本次 `request.view` 投影，屏幕 band／cursor 坐标不应再次偏移；没有原生孔时，现在仍校验投影、世界与帧序，允许纯相机变化。有任何原生 cutout 时保留 exact-camera 条件，防止透出另一相机位置的游戏像素。

`CameraPrefetch` 原来只在 worker 开始时观察相机，观察间隔大于 250 ms 就拒绝预取；耗时超过该值的场景会持续失去预取。现由 UI 每轮呈现采样真实相机，随不可变 Request 传给 worker，仍以 worker 的新鲜实际相机为基准作最多 64 像素的有界偏移。同逻辑帧的重复 UI 轮询合并最多 32 ms，避免高频轮询把较低频率的相机步进错误清零；新逻辑帧、持续静止和反向仍停止或翻转。没有改变游戏相机、单位时间或光照时间。

世界 Frame 完成时记录 `steady_clock` 完成间隔，UI 同时显示该值及 Host 的 `UpdateLayeredWindow` 提交间隔／连续最大间隔，用于区分渲染完成和窗口提交。它们不是屏幕实际显示 FPS，也不以渲染耗时倒数冒充刷新率。

验证 `render_scroll.cpp` 通过 17,358,000 个原像素比较、3,840,000 个部分覆盖／裁孔像素和 15,600 个生产 xBRZ 像素；`render_feedback_host.cpp` 验证采样期间相机移动时 band 像素不移，以及原生 foundation 孔仍拒绝相机不符。两项优化版及 ASan／UBSan 通过，关闭 LSan，不能主张泄漏检查；`render_atomic_camera.py`、`render_presentation_gate.py` 和四项源码接入检查通过。x86／x64 完整 assist 翻译单元各完成一次实际 O0 对象编译，未链接或执行。以上共 12 项局部检查记录为 `render-scroll-followup-private/result.json`，先前缺少 xBRZ 链接实现及 syntax-only 驱动失败另行保留，均已用正确命令重跑通过；这些是最终特效集成前的局部检查，不代替最终包资格验证。

## 追加问题：SHP 单位炮塔

SHP 车辆本体的采样没有独立炮塔 frame，`camera_assets.h` 只解码 body 与 shadow，因此即使素材同一 SHP 内含炮塔帧，也不会自行显示。新增炮塔帧是独立呈现 metadata，不改变共享 IPC。

同 sample3／sample4 的 `assist/tests/shp_turret_native.py` 实际执行 `Unit::DrawAsSHP` 内 `0x73CCAE..0x73CD08`，包含 `FacingCurrent @0x4C93D0`；最终 DrawObject 虚表 `+0x50C` 为观测替身。每份 426 个 raw facing 边界与 WalkFrames 用例，合计 852；实际 SHP、offscreen origin=(128,128)、flags=`0x2820` 以及 frame 均与编译的生产 helper 对照，frame 全部一致，没有执行最终像素栅格。

认证公式为 `8*int8(Type+0xE5C WalkFrames) + ((bucket(FacingCurrent(Unit+0x3A0))+4)&31)`，开关为 Type `+0xCA1`。`visual::Pose` 新增 `turretFrame`，radar／watch 呈现 key 纳入炮塔 frame。精简证据：[shp-turret-native.json](evidence/shp-turret-native.json)。

反汇编 `0x73C7AC..0x73CDDB` 显示原版使用 256×256 临时面，body／turret 以同 (128,128) 原点合成，再整体提交。生产 `camera_assets.h` 用同 shape／palette 的原始 indices 透明覆盖 body＋turret 为一个 Image，再统一执行现场 light／xBRZ／depth，保留原 body shadow；不把两个有不同 crop 的帧独立变成附件，避免生成两条 standing-Z 深度梯度。

`shp_turret.cpp` 与整套 `game_assets_host_test.py` 通过；新增归档字节／direct live SHP 两路径、两个独立炮塔 heading、透明孔保留不同 body 像素、source indices 精确值与缺帧 incomplete 重试。上述较大绘制区间是静态反汇编依据，原指令执行认证仍只覆盖 852 个提交帧用例。SHP 车体附 VXL barrel 分支尚未实现，也没有执行原版完整 offscreen cache 混色或完成实机验收。

## 追加问题：飞机弹药与 IFV 乘员提示

这些 pips 是原生单位 UI，而非另一个世界 actor；单位图像绘制完成不能自动覆盖它们。新增 `native_unit_ui_stencil.h::UnitUIStencil` 仅对 CurrentObjects 中选中、当前玩家拥有的 Vehicle（vtable=`0x7F5C70`）／Aircraft（`0x7E22A4`）保留 Ammo／Passengers 像素，不扩展到建筑、陌生 vtable、hover 或全部状态图标。

| 原函数／字段 | 本轮认证与采样用途 |
| --- | --- |
| `0x709A90` | `DrawPips`，thiscall、`ret 12`；实际绘制调度的原指令入口 |
| `0x6F6A8E`，虚调用槽 `+0x450` | 非建筑 pips 的原始锚点先偏 (-10,+10)，DrawPips 再偏 (-5,0)，步进 (4,0)；Ammo 再 y-3 |
| `0x5F65A0`，`0x6D8C60/0x6D2140` | GetCoords 从 Object `+0x9C` 复制坐标（thiscall、`ret 4`），原坐标投影路径 |
| `0xA8ECB8` 的 `+0x04/+0x10`，`[0xA83D4C]` | CurrentObjects 的 items／count 与当前玩家 House |
| `[0xAC1480]` | 非建筑 pips SHP；不能误用另一套 `[0xAC147C]` |
| Type `+0x3D4/+0x684`；`GetPipMax @0x716290` | Ammo 模式 1 与最大值；当前 Ammo 在 Object `+0x2FC`，允许原生 -1 |
| Type `+0x3E4` | wrap；分层 frame 14／15+layer；非 wrap 保留原生 spawn shim 的 12／13 两帧 alpha union |
| Type `+0x5E0/+0x805` | Passengers 模式 3 的 capacity 与 spacing 分支 |
| Object `+0x114/+0x118/+0x30` | 乘员 count／first／next，另检查 parent `+0x11C` 与身份 |
| Type `+0x380`；Infantry Type `+0xDFC` | 整数 Size 与实际乘员 Pip 帧；大体积填充 3，空 slot 0，保留不同 IFV 乘员图标 |

`tools/native_feedback_audit/unit_pips.py` 在 sample3／4 各执行 6 案，合计 12 案：Ammo、空弹药、wrap、空 IFV、不同乘员和 gap；包含实际 GetPipMax／GetPipFillLevel、`0x473460` 与 FTOL 指令。外侧 GetCoords／原锚点／投影调用链为静态核对，探针以合成 point 入 DrawPips，不将这些外侧函数列入 12 案的实际执行范围。最终 SHP draw、self-heal predicates 与 allocation／free 明确替身化，没有 native 像素栅格或实机截图。生产采样双读身份／owner／type／point／ammo／乘员链、CurrentObjects 头项、frame 与 shape；仅活跃 UI 时读真实 `0xB0CD48` scale，失读／零／NaN 拒绝开孔。

精简证据：[unit-ui-native-audit.json](evidence/unit-ui-native-audit.json)。初稿保存报告时复用了 draws 列表，导致落盘每案展示成最后一案；逐案原函数断言并未失效。现以独立列表保存并重跑两样本，另读取最终 JSON 核对各案帧序列：`[13,13,13]`／`[]`／`[16,15,15,15]`／`[0]`／`[5]`／`[5,0,0]`，纠正前的报告不作最终证据。

现场 sprite 仅解码非零索引 alpha，并缓存 shape／frame 和同次 capture 的 unique frame；h=1 孔在宿主合并／裁切，内部 131,072 条预算与最终 16,384 条上限区分。身份或链变化、环、错误 parent、scale 失败不提交半组；与其他原生孔共享 exact-camera 和失败清孔策略。`native_unit_ui_stencil.cpp` 优化及最终 ASan／UBSan（关闭 LSan）通过，覆盖 Ammo／空／wrap／IFV、Size／gap、alpha／cache、camera／2×、身份／链环／parent／无效 scale，记录为 `render-latency-private/unit-ui-final.json`。

## 追加问题：素材高刷新跨动作混合

`material_motion.h::adjacent()` 原来只检查 frame 数值相邻和方向／variant 等，注释虽说跨动作应 snap，却没有检查动作序列。步兵从走路切开火时，即使两帧编号恰好相邻也不是同一动画的插值；旧路径会混合不相干帧。

复用既有[步兵运行时取帧认证](2026-10-23-watch-assets-137-pre4.md)：`Infantry::ShapeNumber @0x518D80` 为 thiscall 无参，Type `+0xE3C` 指向 42×36 字节序列表，Infantry `+0x6C4` 为当前 sequence、`+0xF8` 为进度。此次把已有 sequence 记录进呈现 Pose，SHP 车辆另区分 fire／stand／walk。`adjacent()` 要求 sequence 与独立 `turretFrame` 均一致才允许 blend；跨动作、炮塔帧变化或循环边界使用新观测帧，不交叉溶解。本项复用旧 `visual_pose_runtime.py` 的原生证据，没有新增字段地址或声称重新认证所有动作。

第二处在 `projectile_visuals.h`：旋转 SHP 弹体的 frame 编码方向，原 body 却一直为 0，两个数值相邻的方向帧可能被误认为时间动画。现在非动画旋转弹体 body 记录其已认证方向；AnimLow／AnimHigh 接管 frame 时 body 保持 0，避免速度变化打断时间动画；sequence 同时区分旋转／NoRotate／Anim 模式。只改变宿主呈现 metadata，没有改游戏时钟、原生帧号或共享 ABI。

`material_motion.cpp` 优化及 ASan／UBSan（关闭 LSan）、`material_scene.cpp` 的实际 Timeline→Renderer 跨动作像素、`visual_pose.cpp` 的 sequence／原取帧、`projectile_visuals.cpp` 的方向与 AnimLow／High metadata 全部通过。用旧 `adjacent` 谓词替换新谓词运行相同回归，在跨动作 `frameFrom==-1` 断言失败，旧值为 10；私有记录 `native-effects-private/material-motion-before-result.json`。这是给已有插帧补缺失的准入边界，没有替换像素 blend 算法或改写原游戏动作表。

## 追加问题：控制台仍在但无法操作／关闭

源码复核发现一个 User32 等待环：assist 主线程 `WM_DESTROY → renderupgrade.shutdown → compositor.shutdown` 等待 compositor 线程退出；compositor 同时可能经 `overlay_stack.h` 的同步 `SetWindowPos` 等待 assist 所有的 overlay／watch／minimap 窗口线程处理位置消息。窗口所属线程此时正在 join compositor，不再处理该消息，彼此等待可使控制台保持可见却无法响应。它是可由调用链支持的死锁路径，尚未取得用户这次失效的现场线程栈，不能断言每次症状均由它单独造成。

`overlay_stack.h` 改为跨 owner 线程的窗口位置请求带 `SWP_ASYNCWINDOWPOS`；微软[SetWindowPos 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)说明，位于不同输入队列时该标志会向 owner 投递请求，避免调用线程同步等待。异步高层锚点尚未实际就绪时，不继续安排低层；后续轮询观察完成后续接层级链，并检查组内真实相对顺序。请求成功只表示已排队，不能直接当作布局完成。

拒绝将现存问题误记为“registry mutex 包住 SetWindowPos”：当前 User32 位置调用早已在该 mutex 之外；detach 当前也没有 join，神经取消使用 try_lock。cursor hookStop 的 1,500 ms 和 broker 的 2 s＋abort 1 s 是有限等待，不能只凭这些上限推出永冻。`overlay_keepalive_test.py` 的 busy-owner 队列与迟到乱序恢复断言通过，旧头文件运行新版 mixed-owner fixture 在缺少 ASYNC flag 处明确失败；新增实际 Windows owner 不泵消息／raw wait／join producer 回归的 x86／x64 EXE 交叉编译链接通过，未在 Windows 执行。记录为 `console-response-private/overlay-stack-validation.json`；没有增加 shutdown 超时绕过。

另一条独立风险位于 cursor 的提示框：`error()`、`CloseCheck → cursorCloseAllowed()`、恢复确认与结果等使用 `MessageBoxW(mainw,...)`，但统一宿主的 pane0 可以独立隐藏／DWM cloak。微软[DWM 文档](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)区分从 owner 继承的 cloaked 状态；因此该 owner 关系可能使模态提示不可见，cursor 线程停在对话框内，宿主等不到关闭确认。这里是结合源码与 API 语义的推断，未在用户现场复现。

新增 `settings_dialog.h`／`cursorDialog()` 让统一宿主下这几类提示在原调用线程使用 NULL owner，附 `MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST`；独立 cursor 保留原 owner／flags，不给对话框引入另一个线程的 shell owner。[MessageBoxW 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messageboxw)说明 NULL 无 owner、TASKMODAL 限定当前线程窗口。另移除 `Host::cloak()` 持槽 mutex 调外部 `DwmSetWindowAttribute` 的锁范围，保留消息投递／退役锁；生产方法的受控 callback 测试验证可重入边界，恢复旧锁时 `mutex.try_lock` 断言失败。它证明锁契约，不证明真实 DWM 每次必然回调。

`settings_dialog_test.py` 两模式、`frontend_cloak_test.py`、`cursor_settings_ui_test.py`、`suite_display_test.py`、`modern_ui_test.py` 五项脚本已通过，旧锁负对照明确失败；x86／x64 的 pane fixture 链接、suite 对象与 cursor 对象共六项交叉构建通过，记录为 `frontend-exit-followup-private/result.json`。Windows fixture 增加隐藏 pane 的真实 MessageBox、no-owner／同线程／非 cloaked／task-modal／关闭恢复，以及忙 worker 下 callback 重入，尚未在 Windows 执行。实际 UI 可见性与进出游戏后的响应仍待用户实机验收，没有用超时绕过、新 Hook 或跨线程 owner 隐藏问题。

## 验证与未决项

已完成的证据分别是：两样本 1,344 个阴影矩阵／20 个 Shadow_Point／3,150 个精确 floor 用例，2,048 个 Laser 原指令几何及 6,144 个比例覆盖检查，两样本 4,704 个矿车 gate／frame／world-point 用例，852 个 SHP 炮塔帧、12 个 pips 原指令调度用例，18,000 帧受控提交调度，以及上述生产资产、孔洞、拖屏、相机、读取失败和源码接入检查。各组边界不同，不把这些数量合并为“真实游戏画面验证”总数。

最终包内 `Build-info.json` 绑定冻结源码提交、本地真实构建、PE 校验和最终实际执行的检查；各代理开发中的局部检查不是冻结后构建凭据。用户游戏内画面、跳帧频率、Windows／DWM 呈现、全部 MOD／其他 locomotor／未支持效果类别，以及多人联机均保留实机验证边界。快速拖屏超出已有源图时边缘仍会透出原生；带原生孔的帧维持严格相机一致性，安全退让仍可能发生。

最终本地综合资格检查 **83／83 通过**，记录为 `render-hotfix2-private/checks/results.json`；私有原函数 probe 数量另列于各节，不混入该 83 项。初轮 80／83 的原记录保留为 `initial-results.json`：单位 UI 接入暴露的无孔宽限回归已修复并保留 latch／有孔失效断言，atomic-camera fixture 同步新增 statistics POD 后原实断言通过；另一次把字面 `validate_$name` 当脚本名是测试 runner 错误，已改跑七个具体 validator。仅重跑受影响检查，最终结果保留这些纠正的依据。

模态／退出链、动作边界、SHP 炮塔与单位 UI 已纳入最终检查，但仍不能扩大为所有 rank／hover 或全原生 UI。83 项通过不表示新增 Windows fixture 已实际执行，也不表示这轮新阴影／UI 在真实 GPU 或用户游戏中验收；这些边界随最终包的 `Build-info.json` 一起保留。
