# YRpp 原生绘制参考：查表、专用 blitter、姿态缓存及可见/脏区

用户认为33ms仍太慢，询问YRpp是否提供游戏本身的绘制方式。前述32.954ms是工具箱受控CPU回放，不是原版Render耗时，也不是用户实机FPS。调查前查阅研究索引、原生渲染阶段/脏区笔记、HF26/28/29及同一SQLite笔记。本文仅为固定上游源码审查，未执行游戏或GPU。

固定上游：[CnCNet/YRpp d9795546950fcc75a94325ffc83b44d16ec34ce2](https://github.com/CnCNet/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)。YRpp既有引擎接口/跳转地址，也有 `Blitters/` 中实际的C++像素算法；完整高层绘制仍需通过对应游戏入口及原版证据核对。

## 优先参考的机制

| 机制 | 固定头文件中的证据 | 对当前工具箱的意义 |
| --- | --- | --- |
| 先选择专用blitter | `ConvertClass`的 `SelectPlainBlitter` / `SelectRLEBlitter`，并持有50个Plain与39个RLE函数对象槽；`Blitters/`按Z读写、A、透明度/warp等分开 | 在绘制调用/行之前选择像素内核，减少通用 `Surface::blit` 逐像素重复判断；特殊目标读取仍独立处理 |
| 光照/A映射表 | `AlphaLightingRemapClass::Table[256][256]`；RLE Alpha-Z路径每次先取当前亮度对应的一行，再以A及源索引查预生成色表 | CPU既有A=127/大图fast path不等于完成整个机制。优先研究完整A域的偏移查表及最终packed色查表；保持当前FullColorData、remap、555/565与不透明/半透明规则 |
| 透明跨度跳过 | `RLEBlitter::Process_Pixel_Datas`遇到透明标记时，按长度一起前进目标、Z、A和Z-adjust，而非逐透明像素处理；UseZ/UseA为模板参数 | 素材缓存中保留或建立不可变行跨度，只遍历可见跨度。不能把外部RLE解码器/裁剪约定未经验证直接替换 |
| VXL按状态缓存栅格结果 | `ObjectTypeClass`的 Main/TurretWeapon/Shadow/TurretBarrel四类cache；`Drawing.h`列出VoxelIndexKey的车身/炮塔/HVA/移动器姿态规则 | 平移不应使全部体素几何重算；但本工程已经有相关缓存，应先量命中率与失效原因，不能重复声称首次加入 |
| 可见内容和脏区 | `TacticalClass`的VisibleCellCount、VisibleCells、RegisterDirtyArea；`Drawing::DirtyAreas`；`Display::Submit/Remove`与五层ObjectsInLayers | 主视口准备限制在实际可见内容与必需依赖；不变地形保留。每次交付完整图，与每次重新计算全图是不同工作 |
| 原生表面与packed缓冲 | `Surface.h`的Tile/Sidebar/Primary/Hidden/Alternate/Temp/Composite和BSurface的2-byte色；`Drawing.h`的独立ZBuffer/ABuffer、区域操作及回绕指针 | 研究按原生packed像素合成、最后一次输出转换，减少目前ARGB32+float-Z+u16-Z+A路径中的重复读写；这是候选架构方向，尚未实测可替换 |

## 接口与地址

以下来自固定YRpp头文件，本轮未验证它们适用于所有spawn/Ares或MOD映像，也没有调用这些原生函数。既有私有样本关联SHA-256为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`；本轮没有重新反汇编该样本或增加其执行证明。

| 入口/对象 | YRpp地址/ABI或布局 | 证据 |
| --- | --- | --- |
| GScreen::Render | `0x4F4480`，thiscall | GScreenClass.h；高层仍是JMP_THIS |
| GScreen::DoBlit / UpdatePrimarySurface | `0x4F4780`，fastcall，surface与可选rect | GScreenClass.h |
| Tactical::Render | `0x6D3D10`，thiscall，DSurface/bool/int | TacticalClass.h；mode注释不是已验证的独立关闭开关 |
| Tactical::RegisterDirtyArea | `0x6D2790`，thiscall，RectangleStruct/bool | TacticalClass.h |
| Drawing::DirtyAreas | `0xB0CE78`，DynamicVector<DirtyAreaStruct> | Drawing.h |
| Plain/RLE blitter选择 | `0x490B90` / `0x490E50`，thiscall | ConvertClass.h |
| AlphaLightingRemap FindOrAllocate | `0x420140`，stdcall(intensityCount)；表256×256 WORD | AlphaLightingRemapClass.h |
| CC_Draw_Shape | `0x4AED70`，fastcall，多参数含Flags/ZAdjust/ZGradient/Brightness/ZShape | Surface.h |
| LayerClass::AddObject / Sort | `0x5519B0` / `0x551A30`，thiscall；ObjectsInLayers位于`0x8A0360`共五个容器 | MapClass.h |
| Display::Submit / Remove | `0x4A9720` / `0x4A9770`，thiscall(ObjectClass*) | DisplayClass.h |

原生阶段0→1→UI→2、动态/脏区/滚屏准入沿用此前私人样本静态与受控证据，本轮没有把它们重新宣称为实机验证。对象阶段仍包含DrawBehind/DrawIt/DrawExtras等；当前建筑base与Extras阶段缺口不会因加入一个快像素循环而自动解决。

## 不能未经核对逐字使用的上游实现

`Blitters/BlitTransXlatAlphaZReadWrite.h`的当前固定版本有两个值得复核的源码现象：源指针只在Z比较通过后前进；Z值读取为局部WORD，函数内未看到向原Z目标写回。其名称不能代替实际语义，不能把该函数当作已验证的ZReadWrite正确实现直接搬入生产。

对照的 `RLEBlitTransXlatAlphaZReadWrite.h`则使用Z引用并在普通分支写回；共享RLE遍历也有透明跨度前进的实现。上述是源码审查现象，不是执行结果或对原游戏二进制的缺陷结论。移植时应按实际源格式、裁剪、A/Z、色盘和混色约定逐项对照独立预期及原版。

## 下一步顺序

1. 先定位常驻地形/素材、VXL姿态缓存与可见范围准备中的失效/重复工作（包括已发现的两个prepare-only未用全图页索引副本）。
2. 以YRpp的A/亮度查表、透明行跨度和调用前专用内核为下一组CPU候选；保留Hotfix29的完成帧和绘制顺序约束，每次独立验证。
3. 原生packed目标与单一GPU呈现后端作为较大独立实验。读取或复用已有原生surface不能自动产生逻辑帧之间移动的高刷单位，仍需处理独立对象绘制/运动数据。

本轮仅新增研究记录，没有修改Hotfix29交付或启用回退的GPU直出。没有性能倍数/FPS承诺。
