# HF28 渲染整合自审：迁移不完整，尚无实机验收

## 结论和验证范围

用户追问“测试了吗”“是不是抄得不全”。结论：迁移不完整，既有缺少的功能，也有已接入路径之间的合同不一致。HF28不能被当作已完成迁移或已验证可用的版本。此前交付只完成生产交叉编译、680项跨位宽ABI、PE/依赖/SEH链接和封装摘要静态检查，没有执行该版CPU重放测试、Windows游戏、GPU/D3D/DirectComposition、联机或性能测试。

本轮是源代码审查，未执行测试套件或驱动；未修改产品代码或产生新二进制。基线：本地 `a0c0b9dddeee54f53e3724bdcaaeafb12d792cfa`，产品 `0f53aae65978588bdd6af06c1feee7a9b68a9b62`；远端 `c2c4675ea6c6990217cb6c37a94634b8e7ad3a79`，产品 `66bddb5727fb614bf6d7114e3060b34b6e1bceb3`。先查研究索引与同一SQLite笔记v105，再沿生产调用链审查。

文件摘要、固定参考和未执行项见[机器证据](evidence/render-integration-self-audit.json)。

参考是已固定的 Vera20K `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f` 的 `draw_passes.rs`、`object_stages.rs`。以下原生地址沿用该固定参考或此前静态证据，**本轮没有新增原生反汇编或执行**。此前支持私有样本SHA-256仍为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`；私有EXE和原始dump不发布。

## 已确认的源码问题

### F1 / P1：建筑基底与Display动态部件未拆成原生阶段

Vera `draw_passes.rs:376` 明确先执行Ground的建筑base，再在Display阶段处理动态炮塔/独立Anim，引用入口 `6D9920 -> 43CEA0/43D290` 与 `6D916C -> 43DA80`。我们的 `radar_scene.h:482..502` 将建筑作为一个Marker调用一次完整image绘制；`world_scene_pipeline.h:118..120` 再按整组重排。现有 `scene_extra_layers.h::ground` 是smudge/overlay等，未承接这个建筑base阶段。

触发：建筑主体、炮塔、独立动画与其他父对象交叠。仅更换五层Display顺序不足以表达“base早画、动态部件后画”；同一组附件内部顺序也不能替代跨阶段拆分。两个后端都消费同一错误阶段安排时，CPU/GPU相等也不能证明它等价于原生。

修复方向：命令应携带明确的原生阶段和父项顺序，拆分base/Display子项；不能再只把完整建筑image挂在一个排序键下。

### F2 / P1：本地状态UI与世界光效的阶段边界错误

`render_feedback_host.h:85..95` 将选择/血条等和状态图示制作成独立覆盖层，排在已完成世界图之上；它们不消费世界的原生Z/A，也不在该图的Chrono目标反馈内。Vera `draw_passes.rs:655..703` 的对象Extras扫描在持久光与Laser/EBolt等表面效果前，`910..929` 才执行战场Chrono并切到屏幕固定chrome。

另外，`world_effects.h:82` 将Laser等先绘，再绘Spotlight；Vera `draw_passes.rs:681..703` 的持久光先于表面线效果。我们两个后端共享该Geometry调用顺序，CPU/GPU都做成同一种顺序仍会共同偏离参考。

触发：图示与激光/聚光灯重叠、Chrono冻结期间改变选择/状态。世界图结束后再盖UI不能恢复这些目标颜色操作的先后关系。既有简化图示和尚未迁移的完整原生图示应分别记录，不能把“画了一个近似图标”写成原生Extras已完成。

修复方向：世界状态图示、表面特效、范围/行动反馈及Chrono进入同一按阶段执行的目标事务；屏幕固定消息/侧栏/光标再在其后合成。

### F3 / P1：CPU/GPU的原生Z水线裁切取整不一致

`camera_raster.h:98..101` 的CPU原生裁切上界为 `min(nativeViewHeight, ceil(ay/sy + material.clipBottom - .5))`（主视口原点为0）。`gpu_scene_record.h:71,76` 则先将 `ay + material.clipBottom*sy` 变成输出坐标，再在GPU命令的Walk里计算 `ceil(min(outputHeight, clipBottomOutput-.5)/sy)`。

不触及视口底部时，前者减的是 **0.5个原生像素**，后者减的是 **0.5个输出像素再除以sy**。手算反例：`sy=.5`，`ay=0`，`material.clipBottom=100`，视口下界充足。CPU得到 `ceil(99.5)=100`；GPU得到 `ceil(49.5/.5)=99`。这是源码表达式的代数反例，本轮没有运行它。

`native_surface.h:39..45` 用裁切后的有效高度生成Vertical/bridge-split的种子和递增，差别会改变Z，不只是少一行颜色。触发条件包括缩小输出的有限水线裁切；1x普通不裁切用例无法覆盖。

修复方向：两个后端必须共同消费同一坐标空间、同一取整的Walk描述，避免分别推导半像素规则；需独立预期的水线/桥体遮挡用例。

### F4 / P1：HF28 GPU异常边界没有覆盖最终呈现和部分资源退出

`vulkan_scene.h:108..113` 的SEH包装覆盖FrameResolve场景生成及其正常失败清理。`overlay_compositor.h:118` 在另一个呈现线程直接调用 `gpupresent::Compositor::present`；`gpu_compositor.h:107..137` 的D3D Map/Draw/Present、DirectComposition Commit和 `:106` 的reset均没有该边界。

FrameResolve析构 `vulkan_scene.h:103`、DirectOutput析构/close `vulkan_present.h:54..59` 也在该叶包装之外。HRESULT检查只能处理返回的失败值，不能阻止宿主访问异常终止线程/进程。HF28日志因此不能保证覆盖“开GPU就崩”的所有位置。

结论：上一版加入的是**局部隔离与定位设施**，没有找到并证明此次崩溃根因，不能写成“GPU崩溃已修复”。修复应先检查真实资源/同步合同，再补齐隔离；捕获异常不等于恢复设备安全。

### F5 / P2：CPU回放仍重复做GPU专用准备，行分片又可重复整张素材处理

`viewport_canvas.h:87` 在nativeScene存在时，即使用户关闭GPU仍启用recording；`:137` 进入append，而 `gpu_scene_record.h:81..84` 会准备GPU重建源。CPU完成阶段 `viewport_canvas.h:171..176` 跳过GPU提交，转而Surface重放。

HF28的 `gpu_scene_record.h:159..165` 只在拥有顶层gpuRaster的image上提前物化；其余调用在每个相交行band上重入Surface。`camera_raster.h:117..119` 在每个band为相交的放大SHP重新分配并着色**整张源图**，再查询该线程自己的xBRZ缓存；有GPU附件的CPU父图仍可在每band再次物化附件。图像越跨多个band，重复的源处理越多。行并行保持目标写入顺序，不自动消除这些重复准备。

这是可定位的重复工作路径，没有测出耗时占比；不能认定它就是用户报告HF27约250ms的全部原因。尤其band重复是HF28新增风险，不应倒推成HF27原有现象的实测根因。

修复方向：拥有者线程按完整图像/光照合同准备不可变的CPU源、重建结果与递归附件，行worker只读输入并写自己像素；GPU专用准备按实际需要延迟，预算按实测分项决定。

### F6 / P2：原生Display读取事务的最终帧检查放早了

`native_display_order.h:14` 在对象身份循环前检查帧号，`:15` 仍会进行多次远程ID读取，随后`:16` 立即发布排序。若身份读取期间游戏前进，返回true的排序事务缺少最终的帧围栏。它不能支持“所有这些数据都经过同帧尾检查”的表述。

外围 `render_upgrade_host.h:115..116` 在native帧通知模式已有提交尾检查；该模式能挡住跨帧发布，不能据此报它必然失效。非native/退路模式`:112..114` 只拒绝captured.frame小于raw.frame，允许较新帧，因此缺少局部尾检查在该路径仍可见。此处不证明Display清单内容一定错误或是乱序根因；同一帧内变化也不是只靠帧号可完全解决。应至少在最后一次身份读取后复核帧号，并与现有采样提交围栏保持一致；失败时不得发布部分排序。

## 未确证的资源风险和已排除的误报

- `vulkan_present.h:58` 对普通在途超时只return，随后的C++成员析构仍释放slots内的D3D共享所有者；只有abandoned分支显式保留了这些所有者。这与“所有未完成故障都保留整组所有者”的宽泛描述不符。Vulkan导入的NT对象另有引用语义，**单靠本次源码审查不能断言COM释放就会导致显存失效或此次崩溃**；把它列为资源合同/故障退出验证项，不能当成已确认UAF。
- `radar_scene.h:473` 的 `*cached.image` 看起来像默认空指针，但 `SpriteCached` 在`:38`已默认创建image，排除这条空指针猜测。
- `Compositor::cutouts` 的行索引没有本地重复验证，但调用前 `overlaycomposition::order/valid` 会验证单行span的范围；未据此报当前越界。
- CPU完成帧复用键已有Z/A、色阶、位移等字段；此次不重复声称漏键。缺少覆盖证据与确定缓存键错误应区分。

## 参考的阶段与当前接入状态

| 阶段 | 固定参考 | 当前边界 |
| --- | --- | --- |
| 地形/格子内容/桥影 | draw_passes的cell body与shadow扫面 | 已有入口；原生完整等价未验收 |
| Ground建筑base | BuildingBase阶段 | 未独立迁移，F1 |
| Display父项与动态子项 | 分层父序、附件及目标编辑阶段 | 有顺序读取；阶段和特殊家庭不完整 |
| 对象Extras | 完成body之后、持久光之前 | 简化覆盖层与未实现图示混在一起，F2 |
| 持久光/表面线/电弧/范围 | 共享目标/Z/A，按原生阶段 | 已接入多家族；光效顺序及边界不完整 |
| 行动线/心控/空袭指示 | 对象访问中交错 | 行动线已有；心控/持续空袭缺口未补 |
| 战场Chrono | 捕获完成战场目标后执行 | 保存世界而非完整战场事务；范围会被清掉 |
| chrome/光标 | 屏幕固定的后续阶段 | 有单独路径，不能承接前面的世界状态绘制 |

Fly/Jumpjet/Hover/Rocket姿态、完整建筑选框/分段血条、炸弹倒计时、自愈/讲话等既有缺项，仍以HF27/HF28记录为准。此处没有把任何一项写成新完成。

## 测试审查：存在测试文件不等于本版已测

| 已有文件 | 源码显示的范围 | 覆盖缺口 |
| --- | --- | --- |
| `assist/tests/gpu_scene_record.cpp:7..10,17,26` | 默认不创建GPU设备，24×18的CPU重放；`--require`才请求实际GPU比较 | 432像素低于parallelrows的262144像素门槛，触发不了HF28行并行 |
| `assist/tests/camera_native_rows.cpp:4..29` | 48×36直接Surface及旧RGBA/float-depth摘要 | 未覆盖新行分片、原生u16Z/A、Display采集或最终D3D呈现 |
| `assist/tests/vulkan_scene.cpp:26` | 非require时设备不可用可返回0 | 进程exit0不等于GPU执行通过，需要明确SKIP及执行帧计数 |
| `assist/tests/render_backend_parity.cpp` | 合成的资源/基本与精细路径覆盖 | 不能等价于当前完整游戏数据及真实GPU呈现验收 |

本轮也没有执行这些用例。源代码里未找到针对HF28 `outputRowOrigin`、`displayOrder/nativedisplay`和 `RA2GuardGpuCall` 的专用回归；读过测试与运行通过必须分开。

必要验收应覆盖同一独立参考输入下的ARGB、u16Z、A三者和阶段轨迹；包括达到行并行门槛的大小、0.5/1/1.5/2x、水线/桥体、RGB555/565、附件越界与跨行位移、完整图示/光效/Chrono遮挡，以及真实Windows适配器上的生成→共享纹理→最终呈现→模式切换/退出。独立原生参考不能只调用同一Surface代码生成“正确结果”。这些是未完成的验证要求，没有在本轮伪称执行。

## 后续整改边界

优先恢复完整阶段、统一CPU/GPU原生Z/裁切合同并核查共享资源/最终呈现；再做不可变源准备和GPU性能优化。仅补新图标、再加排序键或扩大异常捕获都不能代替上述整合。HF28仍是未实机验收的候选，稳定latest保持v1.0.38。
