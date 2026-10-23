# pre.4：载员气泡刷新、观察窗全体单位与朝向

调查前检索了 INDEX、pre.1/pre.2/pre.3 渲染笔记和持久 SQLite。问题来自用户：气泡慢于覆盖层；观察窗只有中心单位有图像、附近单位为示意点；地形无法读取。

## 样本与证据边界

- LBW：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 本轮证据为代码审查、反汇编、原生函数受控执行及合成资源/世界测试，没有新增实际游戏目录 MIX 或 Windows 画面。不能把修正的代码缺陷称作用户机器上唯一根因，也不能据此保证全部 MOD 图像准确。

## 气泡

`passengerView` 原有独立50ms缓存。覆盖层 paint 已按配置频率重置只读分页缓存、刷新镜头，故移除气泡计时缓存，直接在每次 paint 读取载员、位置并重建气泡。随覆盖层绘制节奏更新，不额外创建定时器；游戏逻辑帧不受影响。

## 全体单位与模型缓存

原采样并没有“只画选中者”的条件；但图像缓存仅32项、每50ms只处理1项，类型/所属方作为键，调用的绘制角度固定0。新增旋转也不会使旧图像失效。删除256个Techno/512个含地形的显示截断，原全局数组的边界校验仍保留。

现在按全体采样对象建立工作队列，不依赖选择标志；失败对象不阻塞后面的资源。共享原始VXL/HVA几何或SHP字节，单独缓存具体车体朝向、炮塔朝向和SHP帧。每轮最多16项，并在完成一项后检查6ms软预算；单个复杂模型仍可能超过预算。几何最多约400万面，图像最多3200万像素，防止坏数据耗尽内存。超过预算或资源缺失不会伪造模型。

取消单位置点替代。读取不到真实资源时暂不绘制该图像，并每5秒在Runtime日志记录已挂载MIX数、已绘制单位/地块数及一个缺失名称。`watch_host.cpp` 用300种类型、仅中心被选中且故意令中心资源失败，证明其他299种仍被调度和绘制；转动车体/炮塔只重新渲染，不重新捕获资产。该测试的资产后端和GDI是桩，不证明真实模型加载或画面。

## 原生朝向与SHP帧

| 入口/字段 | 新核对用途 |
| --- | --- |
| `4C93D0` | FacingClass::Current，thiscall，DirStruct输出指针，ret4；目标方向+0、起始方向+4、计时起点+8、时长+10、ROT短整数+14（均十六进制） |
| Techno `+388` / `+3A0` | 车体/炮塔FacingClass；不能只读取DesiredFacing，否则转向时会提前跳到终点 |
| `73BA68..73BA88`、`73C8B7..73C922` | 原生炮塔方向量化32向，并扣除车体方向；外部绘制分别旋转车体与炮塔/炮管，炮塔支点跟随车体 |
| `518D80` | Infantry ShapeNumber，thiscall无参，返回SHP帧；Type+E3C为42个36字节子序列，单位+6C4是当前序列，+F8为动画进度 |
| `7EAEFC` | 原生32向到步兵8向的查表；帧=Start+进度%Count+方向项*FacingMultiplier。不是简单使用帧0 |
| `73C613..73C725` | SHP载具8向使用四舍五入后+1偏移；Type+E1C StandingFrames、+E28 StartStandFrame、+E2C StartWalkFrame、+E30 StartFiringFrame、+E5C/+E5D步行/射击帧数 |
| `70DCF0`、`73B90E` | 多炮塔实际编号来自单位+124，不是武器编号+138；额外炮塔/炮管在类型+C8/+158按8字节索引 |

`visual_pose_runtime.py` 在两份EXE执行24组Current插值（含跨零和反向）及32组步兵帧选取。`visual_pose.cpp` 检查外部读取算法，`voxel_preview.cpp` 检查炮塔独立旋转改变图像。方向使用32向，未宣称平滑到任意角度、复刻完整灯光、动画、俯仰、后坐、伪装或Jumpjet朝向特例；步兵常规序列取帧和载具站立方向有原生依据。多炮塔额外部件目前依赖已加载内存，不推测MOD文件名。

公开布局参考：[YRpp Facing](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/Facing.h)、[Dir](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/Dir.h)、[InfantryType](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/InfantryTypeClass.h)。直接执行证据和合成测试在 `verification/review-137-pre4/`。

## 地形MIX查找

地表是TMP，不是SHP。剧场表`7E1B78`每项112字节；Control/Art/Palette名称分别在+48/+58/+68（十进制），扩展名+78。`5349C0`初始化函数明确组装 `%s.MIX` 与 `%sMD.MIX`，后者地表使用Palette字段，不能错用Art字段。

| 剧场 | 地表包（标准名称） | 地块扩展名 |
| --- | --- | --- |
| 温带 | ISOTEMP.MIX / ISOTEMMD.MIX | .TEM |
| 雪地 | ISOSNOW.MIX / ISOSNOMD.MIX | .SNO |
| 城市 | ISOURB.MIX / ISOURBMD.MIX | .URB |
| 沙漠 | ISODES.MIX / ISODESMD.MIX | .DES |
| 新城市 | ISOUBN.MIX / ISOUBNMD.MIX | .UBN |
| 月球 | ISOLUN.MIX / ISOLUNMD.MIX | .LUN |

还可能通过ISOGEN(MD)、剧场Control包及顶层RA2(MD)/扩展包覆盖。树木等Terrain对象和Overlay使用SHP；不能把所有地形资源当成一个固定SHP。

修正的查找缺口：嵌套目录此前只从游戏已登记节点的名称发现，未登记的标准子包不展开。现在主动搜索标准子包，仍按expandmd > ecachemd > ra2md优先级；带目录的已解密索引名只保留文件名，磁盘读取仍限定游戏根目录。加密MIX仍需要游戏已解密索引，未实现离线Blowfish。空白全零地形色盘不再冒充有效色盘，允许磁盘色盘回退；地块类型数组允许到原生65536容量上限。

合成`game_assets.cpp`证明没有live节点也可从RA2MD→ISOTEMMD及RA2→CACHE读取嵌套资源；覆盖优先级与加密索引边界保持通过。TMP格式继续保持52字节帧头、900字节菱形颜色、900字节Z平面的校验，未无依据放宽。格式与[OpenRA TMP读取器](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs)相符。实际游戏目录是否存在未支持的加密/自定义嵌套、MOD帧布局或额外图像变体，仍待用户实机日志和画面核对。
