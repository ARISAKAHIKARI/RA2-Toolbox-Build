# Hotfix23：第二轮 11 项绘制入口接入

问题：落实[额外缺口审计](2026-10-31-vera-render-additional-gaps.md)的 E1–E11，保留外部只读状态来源，主画面和盯防窗口消费相同世界快照，并使 GPU 与 CPU 回退都获得新增材质。基线为 `05704f90b838851a7495035f60e801ee8ebae6fc`。

参考 [Vera20K `3d42426`](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)，YuriPlanet 及贡献者，GPL-3.0；类布局辅助参考 YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2`。本轮 C++/GLSL 适配和本地静态反汇编，未运行上游 oracle、原生指令或游戏。只对 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的用户样本新增反汇编依据；不能外推成另一份 EXE 或任意补丁布局都已验证。私有 EXE 和原始反汇编不入库。

## 实施对应

| 项目 | 本轮生产路径 | 保留边界 |
| --- | --- | --- |
| E1 FoggedObject | `native_scene_extras.h` 捕获保存记录；`scene_extra_layers.h` 独立提交正文、第二半阴影、Bib、保存动画，不向已死亡建筑取姿态 | 正文类型的静态资源和当前 Cell 光照仍需可读；不包含活对象 pips/炮塔/健康帧。完整原生 A/Z 仍在首轮路线中 |
| E2 隐形、伪装、warp | `native_draw_state.h` 统一观察者、cloak 阶段、探测、伪装外形/所属方及 blink；实例级 25/50/75 选择位进入 CPU、GPU、共享缓存键 | 原生 VXL word-offset 折射/位移尚未实现；当前是观察者准入和透明混合，不称完整 cloak 材质等价。跨种类伪装使用展示帧，任意 MOD 动作不保证 |
| E3 沉船 | 允许仍处于沉没绘制生命周期的零血量 Unit；读取 signed `Techno+3CA`，CPU/GPU按世界水线裁剪正文和附件 | 不为未由原生绘制捕获水线的船另造计时器；零/负行保持未裁。未证明裁剪边界逐像素等价 |
| E4 Smudge | Cell 保存 smudge 类型/序号；逐占格计算复合帧0锚点、Cell Convert及强度，无 Z 读写；先地形后污迹再对象 | 不再假定原点仍存在，覆盖部分清除和不同 Cell 材质的调用；不同坡面与视口裁切尚未实机认证 |
| E5 LineTrail | 读取原生32槽环，逆登记序、原强度和世界坐标；detach 后仍绘制；3轴整数走线，同一XY重复样本保序；读颜色/Z不写Z | 维持现有浮点场景深度；尚未复刻原生 unsigned16 Z/A wrap和所有裁切细节。单快照最多8192个32×32平面，极端规模超预算部分省略 |
| E6 信标 | PBEACON选中半帧组与RDRBEACN四段闪烁周期；双方盟友/败退门控、所属色盘；主/盯防独立后置精灵，实景地图GPU反馈 | 示意地图仍在工作线程CPU组合；无放置信标、消息编辑或游戏调用 |
| E7 RadarEvent | 17类记录读取，只有原生有颜色的0/1/2/3/4/5/11/12画轮廓；使用原半径、角度、退场因子/计时器；严格整数线端点 | 原状态机由游戏推进；未移植 EVA、Space 历史。颜色相位读取f32，未逐像素认证三角函数 |
| E8 overlay/桥影 | 全部body后独立shadow扫掠；对角线降序、X升序；高桥0/9 Latin选帧只用于body；影取原始data+半帧，EW影(-15,+7) | 延续已有depth策略；SLOP01Z..04Z原始深度面尚待首轮Z/A工作。废墟的外层overlay proxy裁切尚未等价 |
| E9 Rubble | Cell+40的保存建筑类型、LeaveRubble；原图body3/半帧+3；自定义Rubble指针body0且不加shadow | 自定义SHP须可读或可由资源名加载；不把错误资源静默替换成建筑正文 |
| E10 PixelFX | 找到原生Cell+FC记录，读取随机位置、base/peak、phase/wait；GPU算三角插值和565颜色；CPU回退同规则 | CPU仍做有界采样/门控/命令准备，非全GPU生成。原生未分配或停止推进的屏外记录不补随机数或宿主时钟 |
| E11 ChronoScreen | 读取Action127阶段/剩余计数；阶段1和3分别保留不可变世界及相机，GPU前序表面快照、查原生sin表、旋转和565变白；CPU回退 | 保留的是世界重组输入，不是包括本地UI的原生整屏截图。f32与x87舍入、resize/load重捕获为兼容策略，不声称原生等价。普通超时空不一定触发此效果 |

## 新增静态地址与布局依据

以下全是读接口；没有新增可调用 ABI、hook、协议或事件注入。

| 主题 | 地址/字段 | 依据与结论 |
| --- | --- | --- |
| FoggedObject | ctor `4D0EF0`、保存 `4D1114`、DrawAll `4D1890`；vector `8B3D10`、vtable `7E8B38`；owner+28、kind+30(6)、Location+34、Level+50、items+60/count+6C、visible+74；记录16字节 | 构造和DrawAll反汇编核对。保存正文/动画各有type/frame/noZShape/zAdjust，不能由活建筑补齐。排序保持原有32位wrap key |
| 保存建筑/动画 | BuildingType InvisibleInGame+1701、TerrainPalette+1702、ExtraLight signed16+1548、NormalZAdjust+1520；AnimType AltPalette+35C、UseNormalLight+35D、DetailLevel+2D4、Flat+369 | `4D1D49..4D217F` 核对：35D才是NormalLight，35C选择所属Convert/ANIM；修正混用这两个相邻标志的假设 |
| Techno显示 | `703860/703A10` visual character、`70ED80` disguise；cloak+220/progress+224、当前玩家标志+41A；Rules+628；Cell sensor+7C/disguiseSensor+AC；warp+270/+271；disguise+1D8/start+1DC/blink+1EC,+1F4/type+518/owner+51C | 反汇编结合Vera/YRpp。建筑DoubleThick+16B1来自固定YRpp布局，未声称本轮重新执行。拒绝按redisguise+1E0/+1E8当blink时钟 |
| 沉船 | signed16 +3CA、sinking+3CD；Vera/YRpp与已有原生 `73BEA4..73BF7B` 证据引用 | 本轮未执行该函数；读取正行裁剪，拒绝用屏幕行或atlas分配框自行生成水线 |
| Smudge | Cell+3C类型索引/+11D序号；SmudgeTypes `A8EC18`；type宽+298/高+29C；`6B55F0`、Cell Convert+34、intensity+10C | 静态核对frame0、(row-col)*30与-(row+col)*15偏移、flagsE00。不是序号对应不同SHP帧 |
| Trail | vector `ABCB78`，0x210字节对象；RGB+0、owner+4、head+C、32×16节点+10 | YRpp与Vera `line_trail.py` 固定源码交叉定位；节点XYZ/strength，owner0不代表应立即删除。未执行556C00/4BEAC0 |
| 信标 | manager `89C3B0` 8×3指针；Point+0、flags+C、houseIndex+110；SHP `89C474/89C478`、帧数 `89C41C/89C428` | 固定YRpp/Vera布局对照；原帧模数和所属方控制，不使用墙钟 |
| RadarEvent | vector `B04DA8`；create `65FA70`、draw `660000/660050`；type+0、xy+4/+8、radius+C、rotation+10、fade+18、计时器+30/+38、rotating+3C | ctor/draw反汇编核对，不把+14误作fade；有些类型仅声音/历史记录，不应画轮廓 |
| 桥身/影 | body `47F6A0`、shadow `47F510`；Cell+140 bit80、raw data+11E；Latin `81CC30` | 静态核对桥分派按Cell位；body跳过A7/B2，shadow入口没有同一排除；Crate先于IsRubble且强制帧0；影无body Latin变体 |
| 废墟 | OverlayType IsRubble+2B4；Cell+40；BuildingType LeaveRubble+1768、Rubble资源+14F4；selectors `45F160/45F1D0` | 自定义Rubble返回frame0，shadow selector返回false；不能无条件使用half+3 |
| PixelFX | gate `6D7A30`；init `631D40`、ctor `631E30`、颜色 `631E50`、wait `631EE0`；Cell+FC、vtable `7EFDA4`、size3C | 首次把显示接到原记录：RGB+4/+8/+C，phase+10，speed+14，peak+18..20，base+24..2C，dx+30，dy+34，wait+38；phase0..8192，offset[-31,32]/[-15,16]。门控含Cell+12C bit10、fog、occupant+E4、bridge+140 bit1000。不是普通ParticleClass |
| ChronoScreen | controller `53B460`、draw `53BBA0/53C0A0`；phase `A9FAB0`、remaining `A9FA98`、duration `A9FAA8`、source `A9FA9C`；sin/cos `4CB150/4CB1A0` | 原生phase1扭曲、2白屏45tick、3回退。读取 `84F084` 的10241个f32表项；角度×2607.594482421875，signed/2与mod8192、奇数修正、cos+2048。表由运行进程读，不在仓库嵌入游戏数据 |

证据清单：[静态发现与源哈希](evidence/render-hotfix23-native.json)。列表中的上游oracle文件只是出处，不是本轮测试结果。

## 管线、缓存和拒绝的实现方式

新增材质位与水线参与 ObjectCanvas、GPU编码和最终帧复用比较；共享素材不烘焙某个对象的透明度。CPU与GPU尾迹均读取前一个颜色，拒绝无序相加以及把重复XY折叠成一次blend。影不叠加对象透明度。读取不稳定或失效的生产者清空自己的记录，不保留失效尾迹。

Smudge/overlay/rubble从静态TMP库分出；移除旧overlay局部重绘及其色盘/帧变化触发的TMP重建。素材缓存64MiB/4096条，保留在用shared_ptr；尾迹内容不变复用平面，不因每次高刷快照分配新像素。所有表/数组均有上限。

主/盯防：地形 → Smudge/overlay正文 → overlay阴影 → Fogged保存图 → 当前对象 → Trail → 迷雾/世界特效 → 电弧 → 信标/Cell闪光 → Chrono → 本地UI。Chrono保留世界时冻结相机和既有mask，但不冻结游戏状态。实景地图在GPU效果序列末追加事件和RDR信标；示意地图将对象点、事件、信标放在工作线程同一有序合成，UI只贴图和画当前视框，双面板缩放同一结果。

相较Vera新增的具体部分是：多格Smudge逐Cell保留材质和局部清除；自定义Rubble指针；PixelFX使用原生随机位置/相位；实景雷达反馈的GPU合成。CPU仍采样、选资源、解码和准备几何，不声称全部GPU化或已测得加速。

## 验证与未完成项

生产目标为x64 RA2Toolbox.exe、x86 GameRuntime.dll和GameRuntimeHost32.exe。验证限定Zig0.13交叉编译、GLSL编译/SPIR-V静态验证、PE架构/版本/导入/manifest/导出ABI检查。最终哈希及结果另见 `evidence/render-hotfix23-final.json`。

按用户约定没有运行CPU夹具套件、原生指令执行、Vulkan/D3D、Windows loader、游戏、联机或FPS测试；编译成功不证明画面已实机无残影。

E1–E11都有了生产入口，但E2的word-offset折射与首轮完整Z/A、坡面资源Z表、tether redraw、完整UI/任意MOD材质仍未完成。Chrono世界重组与原生整屏捕获也有明确差异。不能把本轮写成“已完整照搬整个Vera渲染器”。
