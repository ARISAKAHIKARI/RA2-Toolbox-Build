# Hotfix27 后的渲染遗漏与建造计时复核

## 问题与证据边界

问题：当前仍漏了哪些原生绘制；建造提示是否包含花钱速度参数、供电、54格整数截断、就绪尾帧和多工厂加速。

审计基线是本地产品提交 `988d9707fdac3e9bfd202e6c3b0e8ad4bb5ffc1a`，对应远端产品提交 `04c05425bb14e559dd7a496d3f28ac2a984403c3`；文档基线另含 HF27 冻结验证记录。先搜索本索引和同一持久 SQLite 笔记，沿用 HF27 的证据，不把旧 HF15/HF23 清单当成当前状态。

支持的私有样本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。本轮只做生产源码、固定上游和既有静态反汇编复核；没有执行游戏指令、CPU夹具、上游oracle、Windows/游戏、GPU、联机或FPS测试。上游注释中的执行结果归上游所有，不是本轮验证。私有EXE和原始反汇编不入库。

固定上游：[Vera20K 3d42426](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)、[YRpp d979554](https://github.com/CnCNet/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)、[Phobos f06797e](https://github.com/Phobos-developers/Phobos/tree/f06797ec39f2b60b9fa5b6da69288c36d189b712)。本轮新增读取的 Vera 文件与 blob SHA 见 [机器证据](evidence/render-timing-audit-after-hotfix27.json)。

## 建造时间：四项原生覆盖，一项已加但证明未闭合

生产实现：`assist/source/build_time_tooltip.h::AssistBuildTimeTooltip`。它在游戏线程使用已核对的只读 `6F47A0` thiscall：ECX 为仅设置 Building 虚表、Type+520、Owner+21C 的合成接收对象，EAX 返回原生整数时间。并非只拿价格除一个固定速度。未知 tooltip/TimeToBuild 改写不接管，不能据此承诺任意 MOD 的扩展倍率。

| 因素 | 当前实现和静态依据 | 判定 |
| --- | --- | --- |
| 花钱速度参数 | `711EE0` 计算 `ftol(Type.Cost(+610) × Rules.BuildSpeed(+1748,double) × 0.9)`；`6F47A0` 再应用国家和类型建造倍率 | 已包含原生 BuildSpeed；不是按侧栏折后价格自行反推 |
| 电量状态 | `4FCE30` 读 House 输出+53A4/消耗+53A8；`6F47A0` 应用 Rules+578/+570/+574 的惩罚和速度上下限 | 已包含原生当前供电；无消耗/满电与零输出边界由原生处理 |
| 54格舍入 | `4C9EA0`/`4C9FB0`：有符号除54向零截断，然后每格夹到1..255；提示取 `54 × rate` | 已包含；不能改成总时长向上取整或直接显示 raw |
| 最后就绪耗时 | 提示总时长与余时均显式加1；`4C9B20` 在同次调用到54就暂停、清rate并结清余额 | **代码已加1，但未证明所有入口都额外耗费一个游戏帧**；原生逻辑/侧栏访问次序及包含首尾帧的口径仍待闭合 |
| 多工厂加速 | `500910` 取相应类别计数；`6F47A0` 对每个额外工厂乘 Rules+57C `MultipleFactory` 并分别 ftol | 已包含；不能替换为一次 pow 最后才截断 |

另有国家时间系数 `50C0A0`、Type+608、围墙 Type+1571/Rules+758。直接调用原生保留其运算顺序与每次 ftol 边界。

当前完整提示公式：`(54 × clamp(trunc(nativeTime / 54), 1, 255) + 1) / fps`。联机 session3/4 的 fps 是 Session+320 / `A8B558` 的协商游戏帧率，非显示器刷新或工具箱渲染FPS；无有效值显示帧数。该秒数按当前协商帧率换算，不是卡顿、暂停或掉速时的未来墙钟预测。

运行余时读取真实 Factory：progress+24、当前格 start+2C/duration+34、后续 rate+38。公式为 `max(0,duration-(frame-start)) + (53-progress)×storedRate + 1`；54/暂停+70/缺钱+5C分别显示就绪、暂停、等待资金。`4CA6E0` 的供电重评只改 +38，不改当前格的 +2C/+34，所以停电前已启动的格仍按旧期限走，之后用新速度。当前余时保留这个区别。

资金是另一个边界：`4C9B20` 在格到期后按余额+60与剩余格整数计算扣款；支付不足会回退进度并标记+5C，但该格计时已重装。因此包含 BuildSpeed 不等于能预知后续缺钱造成的延迟。当前提示报告正在等待资金，未模拟未来收入、采矿卸载、退款或生产队列。

**排除的解释**：已查路径没有固定“前N格正常、后54-N格减速”；当前格保留期限、供电重评和 `4FA631..4FA68F` 的有条件联机初始进度补偿是三个不同机制。`6AB5BE..6AB5E7` 的 `rate+1` 是侧栏进度展示计时，不是给每格加1或最终就绪多一帧的证明。HF27 的额外1帧保留用户约定，本轮没有将其提升为已证实的原生事实。

## 确认仍不完整的绘制

以下“缺少”限定为独立重绘生产路径未找到相应 producer，不声称游戏本身不绘制；覆盖层门控退让时仍可能看到游戏原画。

| 项目 | 当前源码证据 | 相对固定 Vera / 原生的差异与后续方向 |
| --- | --- | --- |
| 选框、血条及悬停准入 | `feedback_selection.h::selected/selectionPrimitives` 只收选中项，以实际素材包围框画矩形角标和连续血条 | 缺建筑 Dimension2 立体前后角标、PIPS/PIPBRD 分段与原生类别尺寸、选中/悬停完整门槛；Vera `selection_brackets.rs`/`ui_overlays.rs` 对应 `6F60D0/6F5190/6F64A0`。几何CPU准备，线段和索引SHP应进共同GPU Z/A表面 |
| 炸弹、自愈和讲话状态 | `local_ui_renderer.h::statuses` 只有组号、原始军衔和 WRENCH；UnitUIStencil另处理载员/弹药等 | 缺伊万 BOMBCURS 倒计时前缀、自愈 PIPS13/20、进程讲话气泡；Vera `ui_overlays.rs`/`talk_bubble.rs` 对应 `6F519B..524E/70A6BB/6F5E8D`。需补真实指针、计时与可见性采样，不应凭颜色猜状态 |
| 心控连接和持续空袭指示 | 当前普通指令线、PlanMgr与 LaserDraw/EBolt producer 均非这些专用入口 | 缺 CaptureManager `472160 → 704E40` 曲线和鲍里斯 `705860` 持续指示束；Vera `control_links.rs`/`airstrike_lines.rs` 有独立实现。不能把已支持心控载额pips或普通激光算成覆盖；空袭颜色消费随机数仍需只读视觉方案 |
| 非Drive/Ship专用姿态 | `voxel_posture.h::capture` 仅准入 kind2 与 Drive/Ship 虚表 `7E7EB0/7F2D8C`；`visual_pose.h` 没有其他移动器 draw-matrix/point 分支 | 缺正常 Fly 速度/转向倾斜、Fly坠机倾斜和 draw-point浮动、Jumpjet倾斜例外、Hover摇摆/坡面及 Rocket CurrentPitch；Vera `instances/units.rs` 和 `locomotor_visual.rs` 对应 `4CF610/4CF830/54DCC0/513F40/663470`。这不等于飞机本体、真实XYZ高度、弹体俯仰或HVA全缺 |
| 原生绘制阶段及先后关系 | `world_scene_pipeline.h` 先提交所有 world instances，再统一 worldfx、bolts、beacon、sparkles/Chrono/radial；本地状态另层绘制 | 尚未完整保留 Display父对象内的 behind/body/extras、后置Extras扫描，以及Spotlight、Laser/EBolt/LineTrail、radial、beacon、BandBox/动作线的原生阶段。共同Z/A已实现也不能恢复错误阶段的旧目标颜色。Vera `render/draw_passes.rs`/`object_extras.rs` 可作分段命令来源 |
| Chrono期间的范围提示 | `world_scene_pipeline.h` 在 `chrono_.active()` 时直接清空 radials | 这是可见的明确准入缺口，不是范围几何不存在；需确认原生阶段后在整屏效果前后安放范围命令 |

Vera 的 Phobos盾条、数字显示/放置预览等扩展是另一个适配层，不能当成零售 YR 全部原生缺口。当前所加 Phobos矿车/电量指示器不代表整个 Phobos UI 已移植。未完成适配的任意 MOD hook、EMP/特殊状态样式也不能由通用 SHP 成功解码推定完整覆盖。

## 已接入的内容与仍有的等价性边界

HF26/HF27 已接入完整u16 Z/A主路径、跨页位移隐形、完整色阶、Wave0/1/2/3、RadBeam0/1/2、Laser AA/Spotlight/临时光、原生范围几何、EBolt分叉和建筑SHP主体/VXL炮塔炮管。HF22/23/25已接入 PlanMgr、保存图、沉船、水线、Smudge/废墟/桥影、LineTrail、信标/RadarEvent、PixelFX/Chrono以及特殊pips。这些不再按旧覆盖表列成“整个入口未实现”。

仍需区分：EBolt用独立视觉随机流，非原生主随机流同一顶点；host浮点与x87各步截断、目标像素放大/缩小和有旧颜色反馈的顺序仍有误差边界。能产生GPU命令不等于逐像素、各平台实机全部一致。

`local_ui_renderer.h::Painter::text/icon` 仍在CPU按字形像素和图标行生成矩形；文字/选框反馈有CPU光栅/上传环节。可迁移为常驻字形/索引SHP atlas及GPU实例，索引色、A门控和原生阶段应同时保留。只读内存验证、文本排版、稀疏命令准备继续CPU合理；不为“全GPU”而在GPU模拟游戏逻辑。世界逐像素Z/A、混合和特效反馈已有GPU路径，不能写成全在CPU。

## 未决项与验证

待补证据：生产开始/Logic访问顺序至可见就绪的完整一帧口径；实际资金阻塞后的墙钟；ScreenText目标名称/任务提示、屏幕震动与来源相机的关系；各MOD新增UI和扩展状态。本次不把尚未逐入口核对的项归成已确认缺失，也不声称清单穷尽全引擎。

本轮只有审计文档、覆盖表和持久笔记更新，无行为变更或新二进制。校验范围为引用路径存在、JSON可解析、Git差异/空白与SQLite完整性；不运行代码/CPU测试，不把旧构建结论当本轮游戏验收。
