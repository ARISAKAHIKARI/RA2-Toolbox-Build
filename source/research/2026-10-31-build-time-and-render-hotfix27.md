# Hotfix27：原生建造时间提示框与共同表面绘制补充

## 问题、版本与证据边界

问题：建造时间如何受低电量、生产倍率和54格整数计时影响；能否放在原生价格下；Vera范围射线、电弧与SHP/VXL混合部件的实现是否值得采用并交给GPU。

先检索 INDEX 与持久 SQLite 的 TimeToBuild/Factory/tooltip/EBolt/radial 记录；旧笔记只确立 Factory+24 的0..54与就绪边界，本轮新增以下静态依据。

本轮静态反汇编仅针对用户样本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。私有EXE、完整反汇编与游戏素材不入仓库或更新包。没有执行原生指令、上游oracle、CPU夹具、Windows/游戏、Vulkan/D3D、联机或FPS测试；上游oracle文档只作为对照来源，不算本轮执行结果。

固定参考：

- [Vera20K 3d42426](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)：factory、surface_line、radial_indicator、electric_bolt、unit_atlas、instances/shp、voxel_recoil。
- [YRpp d979554](https://github.com/CnCNet/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)：FactoryClass、ToolTipManager、CCToolTip、SidebarClass、SessionClass、FPSCounter、TechnoClass、EBolt。
- [Phobos f06797e](https://github.com/Phobos-developers/Phobos/blob/f06797ec39f2b60b9fa5b6da69288c36d189b712/src/Misc/PhobosToolTip.cpp)：只读合成接收对象、原生提示框文本的接入参考。

Vera/Phobos适配保留GPL-3.0与现有第三方署名。固定文件blob及本轮校验摘要见 [证据](evidence/build-time-render-hotfix27.json)。

## 建造计算

| 地址/字段 | 本轮静态结论 |
| --- | --- |
| `711EE0`，Type虚表+88 | 基础时间为 `ftol(Cost(+610) × Rules.BuildSpeed(+1748 double) × 0.9)` |
| `6F47A0`，thiscall，ECX=Techno，EAX=int | 依次应用国家BuildTime倍率、Type+608、低电速度、多工厂、围墙系数；在原生的每个ftol边界截断 |
| `50C0A0`、House+21C/国家 | 原生解析对应生产类别的国家时间倍率；不拿当前价格简单除速度 |
| `4FCE30`；House+53A4/+53A8 | 供电比例来自发电/耗电，充足或无耗电返回1，无发电返回0；6F4808溢存为float |
| Rules+578/+570/+574 | 速度=`1-(1-ratio)*LowPowerPenaltyModifier`，下限Min；仅低电时应用Max上限；零速度替换为原生0.01f，然后除速度并截断 |
| `500910`、Rules+57C | 每个额外工厂各自乘一次MultipleFactory并ftol；不能用一次pow再截断代替 |
| Type+1571、Rules+758 | 围墙最终应用double系数并ftol |
| `4C9EA0` / `4C9FB0` | 有符号总时间/54截断，每格夹在1..255帧；最小54格，最大54×255格计时 |
| `4C9B20` / `426630` | 当前格倒计时到0才前进，按存储rate重装；缺钱退回进度但已经重装该格计时；54格完成会暂停/清rate |
| `508D88 -> 4CA6E0` | 供电重评只写Factory+38 rate，不重启+2C start/+34 duration。因此当前格保留旧期限，后续格采用新的rate |
| Factory+24/+2C/+34/+38/+58/+5C/+6C/+70 | progress、当前格start/duration、后续rate、在建对象、缺钱、Owner、暂停；余时读取真实工厂，不由完整时间按进度百分比倒推 |
| `6AB5BE..6AB5E7` | 侧栏进度展示会使用rate+1；这是展示计时，不能据此给全部54格都加1 |
| Session+320=`A8B558`；`4C807D`、`55F472` | 联机FrameInfo更新的协商FPS，原生联机信息读取同值；与显示器Hz、外部GPU提交Hz分开。单机用ABCD44；未知时显示帧数 |

完整显示采用用户要求的就绪交接口径：`54 × clamp(trunc(nativeTime/54),1,255) + 1`。额外1帧是价格提示的就绪交接约定；本轮没有实机测量发令、Logic内访问次序、侧栏可见就绪之间的完整墙钟时延，不能把它宣传为对任意生产入口的实测证明。

运行中的剩余显示为 `max(0, duration-(frame-start)) + (53-progress) × storedRate + 1`。暂停、等待资金与已就绪分别显示。完整时间是当前条件下从0开始的生产计时，不能预知后续停电、现金补入、队列切换与网络变化。`4FA631..4FA68F`另有条件性的联机生产进度补偿，读取MaxAhead和DesiredFrameRate并写初始progress（最多53）；剩余显示直接读取该真实progress，未把它误归为固定的低电分段。

**排除假设**：本样本的TimeToBuild/FactoryAI/供电重评内，没有固定“前N格不减速、后54-N格减速”。不能按用户的待核查猜测造一个分段公式。已启动格保留旧计时和网络初始进度补偿是不同机制。

Phobos的GetBuildTime直接取 `max(54, TimeToBuild)`，用于其提示估计；本次沿用合成接收对象的只读查询模式，但补上真正工厂的除54、夹限与剩余期限。合成对象仅有verified Building虚表、Type+520、Owner+21C，不创建/注册/更新游戏对象，不扣钱、推进AI或消费RNG。只在匹配的原生实现与既有游戏线程捕获租约内执行。

## 原生文本框接入

`CCToolTip*`位于887368。虚表7F74C4的+14槽7F74D8原值479050，ABI=thiscall(this,unsigned ID)->wchar*。入口先运行原函数，再在既有GameEntry管理器内执行受SEH与完整FP状态保护的文本增补；关闭、断开或匹配失效时按同一所有权协议恢复。未知原函数/提示内部hook/数据冲突不覆盖。

`724AD0`每次Process通过+14取文本，复制到this+28的256个UTF-16单元，再经+4 Update测量，最终478E30画原有背景、边框和文字。因此新行自然位于原始名称/价格之后的**同一个框**，停留鼠标时也随刷新更新；不另画HUD。

原始B07BC4只有66 wchar，绝不原地追加。新静态缓冲限制255字符并NUL结尾，避免原Process的256字符拷贝边界；过长或读取不完整时保留原文。cameo ID>=1000；当前tab为Sidebar+539C，Strip起始+1544、stride3988，TopRow+44、Count+54、cameo+58/stride52、kind+4、factory+C；索引为 `TopRow*2 + ID-1000`。仅建筑/防御tab及kind7增补，超武和其他按钮沿原生文本。

`Display.BuildTimeTooltip`默认1，视频/监控和原生设置入口共用开关、即时同步并自动保存，独立于渲染升级。Assist协议82，Shared6580472；Runtime4、entry3、frame signal1不变。

## 绘制方法比较与落实

| 项目 | 采用的方法与GPU职责 | 保留的差异 |
| --- | --- | --- |
| 范围提示 | Vera共同表面更合适：原生7BB350双轴独立整数中点走步及DWORD回绕；轮廓不读Z/A，射线采用-500 Z和4BC750成对旧像素反馈。CPU只生成边界/线段，GPU按共同目标/Z/A合成；移除透明UI重复生产者 | 主相机本地选择反馈，不搬到小地图/其他玩家观察窗。CPU回退同一几何；全部x87中间精度仍未逐像素认证 |
| EBolt | YRpp+18为Z，+1C为绘制相位；4C1F20三股、8层栈、首次正弦偏移、每次9个扰动、外/外/内顺序、Z中点插值，经理逆序遍历。读87F6C4 converter+174索引10/5/15；GPU原生线段做替换、Z和A | Vera消耗它的Main RNG；外部采样器不能改游戏Main。本次使用独立可复现视觉流，不能声称随机顶点与原版相同；不修改生命周期、相位或FLH |
| 建筑SHP+VXL | 使用Vera独立部件方法；从真实B8/BC与C0/C4槽捕获，原SHP保持主体。炮塔/炮管各自原尺寸extent、HVA和Z，GPU光栅，缓存姿态和色表 | 不把一个同名VXL擅自当建筑主体，不从文件名猜槽；私有扩展自行插入的混合绘制不在此原生协议内 |
| 仰角/后坐 | 主朝向+388、炮管+370、计数+148；后坐+3D8/+3F8的travel+14、state+18只读双采样。独立平移，炮管绕Y俯仰；GPU packet键包含仰角和后坐 | 不复制Vera/原版“缓存键遗漏仰角而沿用首绘姿态”的粘滞缺陷；当前姿态完整入键。浮点几何仍有已有host与x87舍入边界 |

建筑绘制新证据：43DA80/43DF60以Type+16C5准入；建造/出售阶段与+16CA Artillary例外保持。43E006用各自HVA帧数，存在炮塔时炮管帧0；43E0C9按有符号TurretOffset/8（向零）做建筑pivot，区别于单位12/256换算；43E174/43E196读取各自后坐；43E1CB读取炮管仰角；43E216/43E538用TurretAnimX/Y(+11E0/+11E4)。四象限0/3先炮管、1/2先炮塔。43E900给出 `-AdjustForZ(height)+TurretAnimZAdjust(+11E8)`；新部件取消主体SHP的-2项，保持自己的Vertical Z，读而不写Z，不借用BUILDNGZ。原生炮塔隐形位移也按自身VXL标记进入共同目标反馈。

附带修正：GPU原生线段的行像素表在二分检索前按X稳定排序；原生近似根扩展到正常的负指数输入，供靠近轴线的范围几何使用；它们没有改变游戏数学表。

## 验证与未决

本轮仅生产跨编译、SPIR-V验证、真实Win32/Win64 ABI和PE静态检查。冻结源码及实际产物摘要另记在最终证据文件。跨位数680项声明一致（Shared6580472）；未把“编译成功”当成用户桌面视觉/性能/联机验收。

尚未认证：私有视觉RNG与原生主随机流的顶点一致性、任意扩展hook/自定义混合绘制、每一步x87舍入、增强重建与缩小后的反馈效果、真实游戏生产发令到可见就绪的墙钟时间。主视图、观察窗和小地图继续共享模型/材质，但范围UI只属主视图。既有侧栏主体与建筑放置合法性继续由游戏绘制。
