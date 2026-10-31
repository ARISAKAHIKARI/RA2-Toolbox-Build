# Hotfix26：原生 Z/A 与目标面读取（实施中）

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

## 实施与验证状态

新增共享 native_surface/native_foot_depth/native_effect_math/native_combat_light；CPU与GPU携带独立原生深度、形状通道、palette-row元数据。主/局部视口保留TMP索引来源，在当前A及相机下重放。位移draw切开GPU dispatch，按顺序读写已有目标；无每sprite回读。迷雾SHROUD只跳FE，FOG只接0..127并叠加减暗，保留A完整字节域。新元数据必须进入缓存相等键及资源预算。

此为源码检查点，尚不是完整交付或像素等价结论。当前已通过一轮 Windows 交叉编译及场景/特效/迷雾 SPIR-V 静态校验；后续变更必须重新冻结编译。仍在接通完整消费路径、稀疏页/缩小图、AA与特殊混合件；此处不把未接通路径标成完成。按用户要求不运行 CPU 测试、GPU/Windows/游戏、原生指令或FPS测试。
