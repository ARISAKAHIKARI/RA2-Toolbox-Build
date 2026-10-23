# v1.0.37-pre.10：SHP、覆盖图、朝向、摔落准入与侧栏

调查前检索 INDEX、pre.3/pre.4/pre.6/pre.7/pre.8 笔记和持久 SQLite v27。基准为 GitHub pre.9 分支提交 `39bc1ae3b2860cac44f6d3556b8b0d56819789ff`。用户最新要求：队形续走不强制朝向；点击的碉堡须检查是否空置；只闪 TAB 图标的金/红图案；修改大师科技不要额外 AddCameo，也不要主动通知刷新。

## 样本、范围与未决

- LBW EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网 EXE SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
- 使用上述原始 PE 的 x86 指令、编译后的 DLL、受控 Win32/地图/查询边界以及真实公开 SHP 文件。不是完整 Windows 游戏或双机网络测试。没有用户的新实机捕获能证明其全部 MOD 资源、落地伤害、寻路与最终视觉都已正确。
- 不发布私有 EXE、内存转储、第三方完整游戏美术或 FA2 源码副本。测试日志、输入哈希与范围见 `verification/review-137-pre10/`。

## FA2 SHP 解码与方向

参考 EA 开源 [CNC_TS_and_RA2_Mission_Editor](https://github.com/electronicarts/CNC_TS_and_RA2_Mission_Editor) 的 `shp_decode.cpp`、Loading、IsoView 与 VXL 矩阵代码。参考 decoder 文件 SHA-256 为 `264ca03a53afa422b08027c66cd46f0d7ccf53406a88073a3f98de959832a23f`。

1. `flags & 2` 的帧（包括 2、3、6）逐行使用 uint16 行长度和零值 RLE；不是只有格式3才 RLE。**这修正 pre.6 对格式2“单一前缀后的固定步长”解释。**
2. 真实 `yurig.shp` 第5帧宽13，末尾透明 run 可以到14。旧实现拒绝整帧；FA2 裁掉超出宽度的透明部分。新实现只允许透明 run 裁剪，仍拒绝实色越界、截断数据和非法长度。零长 run 消耗输入，不会死循环。
3. 裁剪图像用 SHP 画布尺寸确定中心锚点；允许合法裁剪超出画布。上限2048、4M像素/8MiB读取，空帧偏移0不画。SHPReference 的实际 SHP 在 +12。
4. 对20个真实 SHP、707帧逐帧与 FA2 比较索引像素及锚点，全通过。Yuri 文件 SHA `1c73fa3a7bdce534f4c4d3a6470a974490dccad18ee2da7bca01177aa1098ef9`；其他逐文件哈希在 `fa2-shp-compare.log`。参考代码只由测试参数读取，未复制进产品。
5. FA2 的 `Z(45*i+90)`、`X(300)`、存图索引 `7-i`，对等距投影等价于本产品 `radians=(body-8)*2π/32`。以前负号使旋转方向反了。预览世界 heading 转 body 为 `(8+heading)&31`；原生0/8/16/24对应屏幕 NE/SE/SW/NW。独立矩阵用非对称3D点逐32方向比较；车体与炮塔保持独立。

资源层：优先实际 SHPReference 文件名；随后 live ImageFile+1F8、RULES Image、ART Image；补 GENERIC/GENERICMD/CAMEO/CAMEOMD 内层 MIX。覆盖图按当前剧场扩展名寻找，即使类型未设 Theater；每种覆盖图读取其适用色盘，矿石按 FA2 RA2 分支用 TEMPERAT.PAL。TerrainClass 不读 TechnoType +DD0 字段。队形与观察窗仍共用 captureActor/renderActor。失败信息包含实际文件、帧号和解码原因，覆盖图单独记录成功/需求数量。

证据：`fa2_shp_compare.py`（ASan/UBSan，参考非对齐 WORD 读取以 memcpy 作可移植等价）、`sprite_formats.cpp`、`game_assets_host_test.py`（真实Yuri文件经完整MIX/INI/色盘/渲染路径）、`visual_pose.cpp`、`formation_host.cpp`、`watch_host.cpp`。这些不证明用户目录的所有 MIX 已被正确发现。

## 点中碉堡的自身预留与空堡要求

原生点击可能先在碉堡 RadioClass 联系表 +E4/+E8 为某个选中坦克建联，坦克已是 Move/Enter 且 +500/+5A4 指向碉堡。旧逻辑把这份联系计为 used，再给同一车加一个 slots，错判满员；分配阶段同样把点中堡排除。

保留已有有效联系的预留者，不能让其他车抢它的容量；运行时核对 `entryDestination(source)==target && radioLinked(target,source)` 后去掉自身重复计数，再补原生 Enter=7。仅有联系但目的地不匹配不算自身预留。**target+2E4 已有真实进驻者，或 +114 有乘员时，无论是否残留自身联系都拒绝。** 最终准入重新检查，覆盖规划后被抢占。

原生 Enter、radio2/14 和到达链接地址沿用 pre.4/pre.5：4DFF40、742CAF..742D10、458E50。测试 `tank_bunker.cpp`、`tank_bunker_runtime.py` 新增空堡自身预留、无目的联系、预留但已占用/已有乘员；`tank_bunker_arrival_runtime.py` 执行原版最终双向 +2E4 写入，地图与移动状态有替身，未模拟全程寻路。

## 磁电：已识别危险但发令被租约挡住；补原版悬崖判定

- 抬升关系仍是 source+2AC / victim+2B0。目标世界位置 +9C/+A0/+A4 对应原生 GetCoords（虚表+48，5F65A0）；地板高度用 578080(thiscall Map,Coord*)。没有证据要求改这些偏移，不能说此前读不到目标或坐标全错。
- 可复现：已存在危险抬升，同时新手动 Attack 仍待观察，Generic 发令租约未释放。普通 Magnetron owner 优先级不足，Idle 被拒绝。旧DLL的 `drop-lease-before.log` 在“检测到危险”之后断言失败。若测试直接把 Attack 写成已交付，observer会释放租约，旧版也通过；这个区别必须保留。
- 明确开启全局自动摔时，只有已经实时复核的 MagnetronDropOrder 使用专用 MagnetronDrop owner，优先于普通/手动待确认指令，仍低于 Sell，同帧仲裁和同次抬升去重保留。没有改变事件种类或网络封包。
- LandType 及速度倍率不能涵盖全部悬崖。补 `Cell::Tile_Is_Cliff 4863D0`，thiscall、无栈参数、返回bool。此函数只读剧场 tile 范围和 tile 子块；包括 AA1020、AA073C 等范围，斜坡端点的子块0/4不当悬崖。没有把相邻高差直接判成悬崖。
- 已有水陆/FloatBeach/两栖/悬浮规则继续；树、矿柱、地面载具取受吸目标正下方地图格内容，排除空气高度、飞行/悬浮、正被吸/下落的单位。桥、地面未抬升等条件仍排除。
- 新日志在原44字段后追加 victim ID、XYZ、+425/+427，方便区分目标与被吸对象及是否已进入下落。仍为只读选中磁电诊断，不能据日志采样范围宣称覆盖全局每辆磁电。

原版停止命令 `536B80 -> 730EA0` 在730EE7发送事件6；事件构造器4C65E0、接收器4C74CB与现有Idle一致，不能仅凭“不摔”把事件6改成另一个猜测编号。接收器清目标/目的地；Unit IdleMode 虚表+484=738970 在7389B1..7389BF调用释放函数70FEE0(thiscall,bool)。释放函数清双向关系，悬空时写victim+425/+427，并记录+428来源、+42C所属方，调用Crash虚表+3A0。**目前测试证明危险准入、排队及这些静态原生路径，尚未证明事件交付到完整AI/下落伤害闭环；尤其不能把手工模型的“解除链接”当成真实摔落。**

## 队形续走

第一轮显式有方向拖拽仍使用后方接近点→目标点；`formationCommit(remember=false)` 的后续同组移动只发送各自目标点，不再为朝向多走一步。相对偏移、成员身份和右键拖屏保持规则不变；自动续走不重复应答语音。`formation_runtime.py` 增加连续两次续走，检查直接落点、任务数量和无第二段 Move。

## TAB 图标闪烁只覆盖金色/红色笔画

原生 ShapeButton Draw `69DEB0`：+1E禁用时 frame2；+40开关态用 +2D 选0/1并叠加闪烁3；普通态 +2C 选0/1。TAB00..03 在6A593E加载，按钮B07C48、stride60h；色盘converter+174为RGB565表。

旧代码比较四个按钮的frame0，会把共同笔画误当底板；还要求四栏全部可读，导致有未开启栏时其他栏也不闪。新代码各栏独立读取当前帧与**自己的禁用frame2**，仅保留从底板变成金/红暖色的像素。同色底板/边框和未启用栏不覆盖；仍由生产状态与每生产线设置决定是否闪。色温条件是产品针对原版红/金图案的筛选，MOD若完全改成其他色系不会强行染底板。

同时修正该独立SHP读取器的格式2 RLE。`sidebar_shapes.cpp` 包含共享笔画、暖色底板、选中金/普通红/禁用底板、独立栏失败与格式回归。尚未在用户当前主题上截图验证。

## 修改大师科技：移除工具额外填图，不主动刷新

采集五处补丁见 pre.3/pre.4，未增加生产条件补丁。此次复查原生 `Building::UpdateConstructionOptions 4456D0`（thiscall，无参数；Building虚表+4E0）：先检查Owner==CurrentPlayer、非Limbo(+81)、+41B、+660，再按BuildingType+EB8的工厂种类选择四种枚举。这些外层条件不在采集补丁的跳过范围。

旧 `refreshMasterCameos` 自行枚举全部四个类型数组并 AddCameo，越过外层工厂条件，确实多加了“有图标不代表能生产”的项目。现在大师模式只装卸原已验证五处补丁，周期逻辑只读统计已显示图标；Produce辅助也不补缺失图标。**不调用AddCameo、不设置House::RecheckTechTree、不主动调用刷新函数**；按用户要求，等原版建筑完成等自然触发。

新确认 House+1FC 为 RecheckTechTree：4F9265读取，4F927C清零，枚举House+68建筑列表并对存活、未出售、同属方建筑调用+4E0，4F92E2更新侧栏。记录地址供理解原版，不意味着产品会写它。

`master_tech_runtime.py` 验证开启、周期、Produce不额外添加和不写+1FC；再由夹具模拟稍后原版置标志，执行原生House→Building→已补丁枚举，覆盖无工厂、Limbo、未激活、未建成、出售、外属与TechLevel=-1。AddCameo最终函数是测试接收端，非实际UI。默认/经典模式由 `tech_compat_runtime.py` 回归。仍不能保证采集模式本身列出的每个MOD项目都可生产；没有另行强改工厂或清空现有队列。
