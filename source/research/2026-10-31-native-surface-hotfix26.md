# Hotfix26：原生 Z/A 与目标面读取（生产路径已接入）

问题：补齐 Hotfix25 记录的 unsigned16 Z/A、位移隐形、坡面与特殊效果缺口。先检索 INDEX、Hotfix21/23/25 与同一 SQLite v101；本文件只记录本轮新核查。

固定参考：Vera20K `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`、YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2`。Vera GPL-3.0 的 native_z、palette_light、foot_depth、terrain_packed、combat_light、building_light、wave_geometry、surface_line 与 native_surface_format 是移植依据，保留仓库 GPL 及上游署名。

私有受支持 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。仅静态反汇编，未执行原生指令；不提交 EXE、原始反汇编或游戏素材。

## 新核查的事实

| 地址/字段 | 静态证据与结论 | 排除的错误理解 |
| --- | --- | --- |
| ZBuffer `887644`；`4373B0/437A10/547CF0` | 空存储 FFFF 与行种子 8000 独立。SHP 先有符号比较再存低16位；TMP 允许相等。Flat/Moderate/Vertical 的量化、余数与裁剪后重种分开处理 | 反转一个浮点 scene-Z 不等于原生 Z |
| `4DAFC0..4DB09E`；Type `DC0/DC4/DC8/DCC` | cliff/column/tunnel/bridge 项取有符号最大值，再加地形修正和 locomotor 项；已有原生 locomotor 槽38返回0。边界 Cell 查找使用线性 `y*512+x` 与共享 dummy `ABDC50` 的坐标语义 | 将四项相加、仅用高度或把每个缺失 Cell 当作独立对象均不等价 |
| `703F79..703FE7`；`AA0E28` | 桥柱选择使用 tileIndex - BridgeSetBase + 1 的7..16窗口。S/E任一贡献1，SE再加1 | 桥体旗标本身不能代替桥柱项 |
| `484F20`、Cell `116/EC`、Tube vector `8B4138` | 低桥按原生 tunnel 指针及 LandType=10 判断 | 不按普通桥影推断 |
| `73B1A8..73B205`；Type `E16`、Unit `5A4`、radio `E4/E8`、Factory Type `16BD` | TooBigToFitUnderBridge 的整幅缓存上段与最后16行分开，工厂导航分支也参与；SHP无炮塔直绘另有-16/Flat规则 | 不能给每个体素子件各自做一次分段 |
| `70BE50`；Abstract `10`、Techno float `24C` | ID与ftol值按有符号32位回绕后 `%400` 得到线性像素字偏移。只有角色4的VXL使用位移；负偏移可读取同次 blit 已写像素 | 随机扰动、逐帧累加、二维边缘 clamp、统一透明度均不等价 |
| AlphaShape `420F40..421340`；LUT `420E90..420F05` | 修改 A 的原生 word，LUT 为 `min(255, index * oldA / 127)`；源0也有效。Draw 从记录 Rect 减相机；旧捕获重复加相机是错误 | RGB后处理、忽略0、除128及把Rect当当前屏幕位置均不等价 |
| Spotlight `5FF850`；record `+C/+10/+14`、表 `83358C`、Rules `7A8` | +C为stage，+10为base size，+14为flags；先查90项stage表，普通表值按size/64缩放，80..89直达mask64..73。遮罩固定256×128，flags1为减暗，2/4/8禁用增亮通道 | +10不是半径，也不能直接当最终遮罩编号；stage0及减暗不能丢掉 |
| Wave `75FA29..75FA71`、`7609E0`；`B0/148/14C/150/1D0` | 模式1/2共用红/蓝加亮分支，LaserEC决定量，严格u16 Z；模式0/3读原生多边形及距离/正弦/亮度表，不读A、不写Z | 不能跳过1/2，也不能把全部Wave画成相同的正弦发亮折射 |
| Wave immutable tables `B4669C/B45E68/B46648`；初始化 `75F020` | 距离为300×300 u16；mag496后有6个zero shorts及sonic500。线性读取域被保留，不周期拟合也不钳成最后一项 | host hypot/sin与原生表不是同一数值机制 |
| RadBeam `659CA0/659FF0`；`90/8C/80` | 喷发2使用当前保留step/count/amplitude，按180度段参数生成端点，不推进计时/RNG。直线1以floor分母、ceil段数，最后可超过目标 | 不能丢弃2，不能把最后一段硬收缩到目标 |
| `4CAC40/4CACB0`；`8650BC/84F084` | sqrt使用原生mantissa表和f32收窄；sin使用原生float表、带符号半步索引 | 标准库sqrt/sin不能作为完全等价声明的依据 |

## 后续核查

| 地址/字段 | 静态证据与结论 | 排除的错误理解 |
| --- | --- | --- |
| Convert `+16C/+170/+174`；`48E91D`、`55621E/556300` | FullColorData为完整色阶，16位每级512字节；PaletteData只指向中间级。每游戏帧每converter读取一次，内容未变复用不可变表；变化进入CPU/GPU缓存键 | 从中间565色反推全部色阶会丢失原始量化与特殊索引 |
| Terrain Draw `71C304/71C34E`；Cell Overlay `47F6A0` | 树木正文Vertical/-12，影Flat/-3；覆盖物按资源/legacy7E、wall、drawFlat、rock选择Flat/Vertical和-2/-17，再扣15×Level。Smudge、Frozen正文/影/bib/anim分开携带元数据 | 不能仅移植TMP和主单位而让其他层继续走旧浮点Z |
| `54530E..545458`；`4AF038..4AF0FB/437AAB` | SLOP01Z..04Z读取整个原始文件中的encoded数据，源文件头和后续帧保留线性读域。raw帧只裁剪、extended帧才消费signed差值；legacy7E偏移(30,-2) | RLE解码后的索引或BUILDNGZ减65不能用于SLOP |
| Laser `550260/5509F0`；`4CB3D0/8610B4` | 显式双色与House使用各自8向偏移表；原生atan表和65534朝向量化决定方向。低detail显式中心先画；高detail侧线先画，按保留float fade缩放 | 不能以屏幕法线统一加宽、统一混色/淡出 |
| `4BC750`；`4BCC0D/4BD556`、`4BD646/68F`、`4BD85B/8C0` | outerRGB全零选AA。保留257项不对称cubic表、Z变化分支、主点共用Z/A、读两点后成对写回。X-major从另一旧目标word混合，Y-major从自身；GPU单invocation按序写 | 普通alpha线、每像素并行或每对先写后读都会改变结果 |
| BuildingLight `8B4190`、`435BE0`；`+9C/DC/E4` | 子对象当前位置、mode、parent决定临时灯光。owner center447AC0按foundation偏移；IsPowerOnline4555D0只读等价门控，67C提升/504暂停/电力/停电/供电/任务18/19分别核对 | AC1678不能覆盖绘制期间才构造的临时Spotlight |
| `435D63..435DE4`、`436E80`；Rules `788/78C/7A8` | 按3D距离和有符号bucket选stage80..89。边缘旋转读原生asin/sin/cos表，使用75−6×bucket的无A增亮线及独立首尾Z。5865E0在受支持私有SHA中直接返回false | 无法凭“灯光表支持64..73”就声称建筑灯已经接入；不把私有补丁门控外推到其他EXE |
| ParticleSystem `A80208`；`62E280`；Type `2A4/304/30C`、object `CC/F4` | 活跃子粒子数/上限比例夹0.4..1，乘LightSize，F4作当前stage；临时Light即时画后销毁，不推进其状态 | 持久灯光向量不包含这批光照，也不能把F4当寿命 |

## 生产接入

- 共享`native_surface`、`native_foot_depth`、`native_effect_math`、`native_combat_light`拥有原生行Z、A、packed格式和特效算子；世界GPU与CPU回退共同消费。
- 主视图、局部/盯防与小地图的TMP、树木/覆盖物/污迹、保存建筑及动画附层携带独立原生Z。BUILDNGZ/SLOP、SHP合成车身炮塔、桥上最后16行分段及水线裁剪进入相同行种子。
- A在palette选择之前生效；SHROUD仅跳FE，FOG只接受0..127并累积减暗；AlphaShape保留0..255完整域和源0。精度增强档保留连续RGB与重建，并使用同一Z/A准入。
- 小地图稀疏128页先原尺寸合成原生Z/A再面积缩图。CPU无sprite页的底色也更新A；GPU的native-Z atlas与分页lookup保持独立。形状/色阶/A身份全部参与内容复用。
- 位移隐形按线性word-offset读已绘制目标。GPU分bulk/serial/bulk并屏障，CPU跨页按全局scanline顺序，并保留负偏移的同次blit反馈。相关页变化会重放完整有依赖事务。
- Laser显式/阵营色/AA、RadBeam0/1/2、Wave0/1/2/3、普通CombatLight、建筑探照灯和粒子系统临时Light均有生产入口。新AA保持成对原子次序，无sprite回读或原生绘制调用。
- 内部绘制metadata64 words，push128 bytes；跨进程Assist81/Runtime4/入口3/通知1不变。CPU页池按RGBA+float-Z+u16-Z+A共11 bytes/pixel重算。

## 验收边界

按用户约定只进行生产交叉编译、GLSL/SPIR-V和PE/ABI静态检查；最终版本、源码commit、shader摘要和二进制哈希见同版随包验收记录。不执行CPU测试套件、原生指令、GPU/Windows/游戏、联机或FPS测量，因此这不是实机逐像素/性能认证。

这里完成的是上述世界surface/效果生产分支的补接。增强倍率过滤和小地图效果在输出采样域内计算，不能宣称与“先原尺寸完成全部效果再积分”逐像素可交换；host浮点中间舍入也未证明与每一步x87 chop53同位。既有本地UI范围射线、EBolt strand近似和任意SHP/VXL混合零件/反冲不在本次已完成列表；侧栏主体、放置合法性仍由游戏原生绘制。外部未知MOD/vtable、越过文件分配域的SLOP读取及原生性能降级门槛不推断为已支持。完整Vera引擎等价仍不得宣称。
