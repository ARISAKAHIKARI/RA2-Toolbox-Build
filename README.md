# RA2 Toolbox 1.0.38-pre.2

副相机新增 VXL 法线／VPL 光照、SHP／TMP 逐像素深度、建筑遮挡、阴影和实际建筑附加动画；小地图支持示意／实景。统一覆盖层层级，敌方标识按两格聚类，建造和统计图标共用游戏已解析字段、内存 INI 与 MIX 资源解析。

[下载 1.0.38-pre.2](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.38-pre.2) · [发布说明](packages/v1.0.38-pre.2/RELEASE_NOTES.md) · [完整特性树](docs/FEATURES-1.0.38-pre.2.md) · [渲染研究](source/research/2026-10-24-camera-pipeline-138-pre2.md)

EXE 与 Runtime 文件版本 **1.0.38.2**，Assist **72**。关闭游戏与工具箱后同时替换 EXE/DLL；已有 INI 保留，首次所有功能开关默认关闭。提供更新包与对应源码包。

渲染仍属预览：施工、闸门、废墟等特殊建筑主体帧和部分 MOD 效果尚未完整复刻；自动回归及 Windows 窗口测试不代表已完成游戏画面、性能或联机验证。gamemd-ares.exe 原已在共用识别名单中，本版补充真实 Windows 进程发现测试；命令桥仍需通过实际映像特征检查。

## 当前正式版：1.0.37

[正式版下载](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37) · [新增／修复与完整特性树](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/v1.0.37/packages/v1.0.37/RELEASE_NOTES.md)

修复冷冻时间、危险载员等设置保存与恢复，改进敌方标识和载员提示跟随，增加 MIX 图标读取；保留直接模式、科技加强与仅光标高刷。配置和快捷键采用用户预设，首次功能开关全关，更新保留已有配置。

## 历史预览版本

# RA2 Toolbox 1.0.38-pre.1

地图全开和随处建造新增“直接模式”，原方案命名为“受控模式”；科技生产保留经典兼容，新增独立“科技加强（实验性）”。仅保留光标高刷可用，拖屏/全部/强制高刷及视角缩放暂时置灰。步兵强行进入建筑保持原状。

[下载 1.0.38-pre.1](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.38-pre.1) · [发布说明](packages/v1.0.38-pre.1/RELEASE_NOTES.md) · [验证范围](source/REVIEW-1.0.38-pre.1.md)

更新包与源码包分别提供。退出游戏与工具后同时替换 EXE/DLL，保留已有 INI。编译与受控回归通过，完整 Windows 实机和双机联机效果待复测。

# RA2 Toolbox 1.0.37-pre.14（图标锚点与副摄像机显示）

铁幕改用最内圈圆心；蜘蛛寄生标识放大；属性图标取消地图格钳制并连续跟随单位。副摄像机遮挡主覆盖标识，刷新跟随覆盖层，地形先画、单位后画；队形预览取消独立位置限频。

[下载 pre.14](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.14) · [说明](packages/v1.0.37-pre.14/RELEASE_NOTES.md) · [研究与验证](source/research/2026-10-24-display-anchor-137-pre14.md)

# RA2 Toolbox 1.0.37-pre.13（三窗跟踪、方框队形与叠加层恢复）

最多三个跟踪窗支持同键逐窗添加、点击跳转、组内死亡接替；新增正方形与空心框队形，同键逐次切换。补齐每格高度光照与实时Z投影，直接点击提交零拖拽预览；叠加层在战场中定时恢复并避让菜单、读条。磁电三项分别支持游戏内/工具箱解算，实验功能增加联机 Sticky 进入快捷键（默认 Ctrl+Alt+S；联机退出尚未实现），自动/按键移动攻击移至实验功能。

[下载 pre.13](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.13) · [说明](packages/v1.0.37-pre.13/RELEASE_NOTES.md) · [研究与验证范围](source/research/2026-10-24-height-overlay-sticky-137-pre13.md)

# RA2 Toolbox 1.0.37-pre.12（地图光照与防空提前接敌）

观察窗和队形中的 SHP/VXL 跟随当前地图亮度、RGB 色调与临时光照。火控提前分配目标：建筑预留火力，机动防空用原版攻击在距离和时间限制内接近，并优先补足近处火力；手动指令与队形优先。

[下载 pre.12](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.12) · [说明](packages/v1.0.37-pre.12/RELEASE_NOTES.md) · [研究与验证范围](source/research/2026-10-23-lighting-advance-137-pre12.md)

# RA2 Toolbox 1.0.37-pre.11（磁电自动摔落实验修正）

修正“己方磁电吸己方坦克”在检查落点前被敌方过滤条件挡住的问题。已吸起目标不再受阵营限制，开关移至“实验功能”，保留原设置和默认关闭。检测、原生停止命令排队与边界回归通过；完整摔落物理和联机效果仍待实机验证。

[下载 pre.11](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.11) · [说明](packages/v1.0.37-pre.11/RELEASE_NOTES.md) · [研究与验证范围](source/research/2026-10-23-friendly-magnetron-drop-137-pre11.md)

# RA2 Toolbox 1.0.37-pre.9（快捷键修复）

修复 pre.8 主程序/DLL 通信名称不一致导致所有快捷键失效，以及未连接时误报“队列已满”。两端通信名现统一生成，增加成对映射验证与等待回执期间的过期队列清理。请退出游戏和工具，同时更新 EXE/DLL。

[下载 pre.9](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.9) · [说明](packages/v1.0.37-pre.9/RELEASE_NOTES.md) · [验证](source/REVIEW-1.0.37-pre.9.md)

# RA2 Toolbox 1.0.37-pre.8（测试分支）

队形与观察窗共用真实 SHP/VXL 加载和色盘，参考 FA2 补齐剧场与覆盖物规则；新增单机显式 Sticky 驻守，磁电自动摔落独立覆盖手动吸取及水陆/悬崖判断。216个 Runtime 受控用例通过；具体 MOD 画面和完整物理仍需实机复测。

[测试发布](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.8) · [发布说明](packages/v1.0.37-pre.8/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.8.md)

# RA2 Toolbox 1.0.37-pre.7（测试分支）

修复磁电/高级移动发送 Sticky 后，后续普通攻击无法切换任务的问题，改用可中断的普通待命。修正资源 ImageFile 偏移导致的前四字符丢失，并改善地形解码兼容与诊断。已卡住的旧对局需更新后开新局；完整实机地形显示仍待复测。

[测试发布](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.7) · [发布说明](packages/v1.0.37-pre.7/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.7.md)

# RA2 Toolbox 1.0.37-pre.6（测试分支）

基于 pre.5。修正右拖取消队形、增强对象攻击接受0个、SHP格式2解码及步兵预览朝向，增加资源缺失和磁电首次攻击后不追击的只读诊断。持续追击问题尚未确认修复；不将事件入队测试当成实机寻路验证。

[测试发布](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.37-pre.6) · [发布说明](packages/v1.0.37-pre.6/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.6.md)

# RA2 Toolbox 1.0.37-pre.4（测试分支）

修复全局无限吸抢回普通左键攻击、坦克碉堡进驻、条件留枪与队形连续移动；新增队形左/右键设置和修改大师科技模式，修复部分载矿返厂排队、SHP图标提醒、切屏覆盖层与小地图双光标。载员气泡跟随覆盖层刷新；跟踪窗口全体单位加载实际图像、车体与炮塔分别跟随朝向，并补充剧场MIX发现。

[测试更新包](packages/v1.0.37-pre.4/RA2-Toolbox-v1.0.37-pre.4-Update.zip) · [发布说明](packages/v1.0.37-pre.4/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.4.md)

# RA2 Toolbox 1.0.37-pre.3（测试分支）

本次修复磁电追击被取消、最小射程目标遗漏和生产提醒误闪；增加统一双击快捷键、小地图点击拖动导航、游戏目录 MIX 美术读取及纯矩形观察窗，选择空闲飞机不再跳转视野。科技全开 probe 已完成分析，未照搬补丁。

[测试更新包](packages/v1.0.37-pre.3/RA2-Toolbox-v1.0.37-pre.3-Update.zip) · [发布说明](packages/v1.0.37-pre.3/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.3.md)

# RA2 Toolbox 1.0.37-pre.2（测试分支）

新增可在联机选择的第四种「强制高刷」、CnC-DDraw / TS-DDraw 高刷配置与恢复，以及左中 11×11 格局部观察窗。观察窗读取真实 TMP 地表、SHP 树木/覆盖物和 VXL 模型，自动跟随危险载员的敌方载具，也可用 Ctrl+Alt+W 固定悬停或选中的单位。修正队形拖动朝向及预览缓存。

保留 pre.1 的火控后排预留/平均或按血量分配、磁电地面障碍检测/全局无限吸、Alt 拖动队形和同键循环切换。小窗暂为固定朝向与简化光照；高刷数值是绘制调用频率，真实画面和联机仍需实机验证。

[测试更新包](packages/v1.0.37-pre.2/RA2-Toolbox-v1.0.37-pre.2-Update.zip) · [发布说明](packages/v1.0.37-pre.2/RELEASE_NOTES.md) · [操作与验证](source/REVIEW-1.0.37-pre.2.md)

## 历史正式版：1.0.36

[下载 Windows 完整包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/download/v1.0.36/RA2-Toolbox-v1.0.36-Windows.zip) · [Release / 更新包 / 源码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.36) · [研究索引](source/research/INDEX.md)

五大面板：画面设置、单位统计、战术控制、战场信息、实验功能。新增敌方载员气泡、危险乘员常驻和磁电自动摔落；修复路径点、磁电轮换和坦克碉堡，改进小地图、状态图标与队列提醒。详见 [发布说明](packages/v1.0.36/RELEASE_NOTES.md)。

完整包包含程序、光标资源、三套配置和离线预计算环境，解压后运行 `RA2Toolbox.exe`；已有安装可选 Update。先退出工具箱和游戏，再同时更新 EXE/DLL。文件版本 **1.0.36.0** / Assist **61**。

已编译并完成受控事件、路径点、设置和布局回归；[验证说明](source/REVIEW-1.0.36.md)。实机画面、完整运动/摔落物理和真实联机仍需平台复测。

## 历史版本说明

# RA2 Toolbox 1.0.32

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.32) · [实现与验证](source/REVIEW-1.0.32.md) · [研究索引](source/research/INDEX.md)

恢复「操控其他玩家单位」与「出售其他玩家建筑」两个实验性入口，首次升级默认打开，之后分别保存选择。Alt＋左键选择、Alt＋右键下令；出售模式按增强出售键点击其他玩家建筑。继续使用统一入口和原版事件队列，不改变对象所属方或当前玩家。

切换玩家仍然禁用，并在执行层拒绝切换请求。1.0.31 的科技全开两种实现保留。

文件版本 **1.0.32.0**，Assist **60**、Runtime **4**、入口管理器 **3**。两份样本共 **90** 项受控回归及设置、补丁策略、架构和 PE/ABI 检查通过；**尚未完成 Windows 实机或双机联机验证**。见[验证摘要](source/verification/review-132/summary.json)。退出游戏和工具箱后成套更新；已有安装选 Update，首次安装选 Windows。

## 历史版本说明

# RA2 Toolbox 1.0.31

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.31) · [实现与验证](source/REVIEW-1.0.31.md) · [研究索引](source/research/INDEX.md)

科技全开保留现有方案（默认），新增“实现方式 → 经典兼容”。兼容候选恢复已找回早期源码的工厂/建造上限检查及每秒补齐；工厂忙碌时可能暂时拒绝工具箱追加生产。

用户确认比较目标为 **0.3.2**，并回忆 0.3.0 好像也不掉线；目前仍缺可核实的该版本原包；已找到的 0.3.0 归档不能代替它。本版用于对照，**尚未验证能解决战网掉线**。细节见[研究笔记](source/research/2026-10-22-tech-131.md)。

文件版本 **1.0.31.0**，Assist **60**、Runtime **4**、入口管理器 **3**。退出游戏和工具箱后成套更新 EXE/DLL。已有安装选 Update，首次安装选 Windows。验证范围见[摘要](source/verification/review-131/summary.json)。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.13

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.13) · [实现与验证](source/REVIEW-1.0.30-pre.13.md) · [研究索引](source/research/INDEX.md)

「步兵进入与 C4（实验性）」改为直接下令，不再追加或提交路径点。普通步兵继续执行 C4，工程师占领、间谍潜入；默认 Alt+C＋中键点击建筑。

文件版本 **1.0.30.12**，Assist **59**、Runtime **4**、入口管理器 **3**。退出游戏和工具箱后成套更新 EXE/DLL。已有安装选 Update，首次安装选 Windows。

[验证摘要](source/verification/pre13/summary.json)：两份游戏样本共 298 项受控回归及编译、UI、设置、PE/ABI 检查通过。尚未验证 Windows 实机完整寻路与双机联机；C4 本机能力补丁仍可能引起不同步。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.12

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.12) · [实现与验证](source/REVIEW-1.0.30-pre.12.md) · [研究索引](source/research/INDEX.md)

删除刷弹入口并停用旧设置；「步兵进入与 C4（实验性）」补齐普通步兵的移动和安放判定，由原版执行炸药计时和到期伤害。工程师、间谍保持原来的占领和潜入任务。默认 Alt+C＋中键点击建筑。

文件版本 **1.0.30.11**，Assist **59**、Runtime **4**、入口管理器 **3**。退出游戏和工具箱后成套更新 EXE/DLL。已有安装选 Update，首次安装选 Windows。

[验证摘要](source/verification/pre12/summary.json)记录最终二进制及正常回归。受控原生函数测试不等于完整游戏寻路、Windows 实机或双机联机测试；C4 补丁改变本机判定，单方启用仍可能不同步。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.9

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.9) · [实现与验证](source/REVIEW-1.0.30-pre.9.md) · [研究索引](source/research/INDEX.md)

步兵 C4 调整为**步兵强制进建筑（实验性）**；修正高级移动攻击的旧停步状态，补充手动增强命令的原版单位应答语音。预定面板支持建筑就绪时继续预定及无可用单位时快捷键切页。整合地图与飞机路径点设置，每功能独立配置单位清单；统一 DPI 缩放和悬停说明，修正滚动重绘。

文件版本 **1.0.30.8**，Assist **58**、Runtime **4**、入口管理器 **3**。退出游戏和工具箱后成套更新 EXE/DLL。已有安装选 Update，首次安装选 Windows。

[验证报告](source/verification/pre9/summary.json)记录最终二进制与受控回归。普通步兵不获得 C4/占领/潜入能力，到达建筑后可能消失。实际游戏路径、语音、IFV、Windows DPI 画面及联机仍需实测。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.8

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.8) · [实现与验证](source/REVIEW-1.0.30-pre.8.md)

- 高刷三档共用光标页的目标刷新率，最高 **500 FPS**，自动模式跟随显示器；移除单独的画面目标设置。
- 名称统一为「载员自动逃生」「敌方索敌显示」，同步修正说明。
- 原风险和测试标签统一为「实验性」；地图半透明、透明、自动全图逐项标注。
- 简化飞机路径点、生产预定等按钮的括号说明，保留悬停帮助。

文件版本 **1.0.30.7**，Assist **58**、Runtime **4**、入口管理器 **3**。已有安装选 Update，首次安装选 Windows 完整包，退出游戏和工具箱后成套更新 EXE/DLL。

[验证报告](source/verification/pre8/summary.json)：设置与编译检查通过，两份游戏样本各 46 项实际 DLL 调度回归通过。500 FPS 为目标，实际显示效果仍需 Windows 实机复测。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.7

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.7) · [实现与验证](source/REVIEW-1.0.30-pre.7.md)

- 高刷范围 **仅光标 / 光标＋拖屏 / 全部** 移到「高刷光标」普通页面，画面目标刷新率与实际补充绘制状态一并显示。
- 「单位统计」集中提供敌方单位提示、载具容量、铁幕 / 力场 / 冷冻时间、蜘蛛寄生和火力 / 防御 / 速度强化开关；状态文字缩放、信息刷新率也放在此处。
- 预定面板移除常驻操作说明和普通增删提示，保留建造图标与队列序号；使用暗蓝卡片、青色选中态、悬停描边和容量点。缩短提示区域，维持绘制与点击的共同缩放。
- 沿用已有配置与自动保存，不需重新设置。

文件版本 **1.0.30.6**，Assist **58**、Runtime **4**、入口管理器 **3**。退出游戏和工具箱后成套更新 EXE/DLL。已有安装选 Update，首次安装选 Windows 完整包。

验证报告见 [verification/pre7](source/verification/pre7/summary.json)。已完成编译、设置消息与面板绘制模型回归；尚未进行 Windows 实机画面验证。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.6

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.6) · [实现与验证](source/REVIEW-1.0.30-pre.6.md)

- 新增 **蜘蛛寄生标志**：14×14 游戏像素，放在单位当前位置，颜色跟随当前寄生者所属方。
- 新增 **火力 / 防御 / 速度强化标志**：每个 7×7 游戏像素，放在单位稍下方；三个图标总宽约 25 像素，整组限制在所在的 60×30 菱形格内。
- 两个显示开关默认开启、分别保存；图标跟随已有高刷覆盖层和窗口缩放，独立于状态文字缩放。
- 读取独立强化倍率，不把普通三星加成显示成吃箱子；MOD 若通过同一倍率字段强化，也会显示。

文件版本 **1.0.30.5**，Assist **58**、Runtime **4**、入口管理器 **3**。已有安装下载 Update，首次使用下载 Windows 完整包，退出游戏和工具箱后成套更新 EXE/DLL。

字段、布局、设置、编译与入口验证见 [verification/pre6](source/verification/pre6/summary.json)。尚未进行 Windows 完整游戏画面及双机联机验证。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.5

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.5) · [实现与验证](source/REVIEW-1.0.30-pre.5.md)

- 自动躲弹改发原版 **Scatter 散开事件**，由原版处理落点和移动。删除自选两格落点及反复发送 Move 的逻辑。
- 一轮散开处于等待执行、车体转向或移动中时，后续弹体只记录，不重新选方向；原版已在移动的单位也不会被这次自动躲弹打断。
- 删除“威胁消失 6 帧后恢复攻击/警戒”的命令；观察到散开执行并停稳后才结束本轮，避免尚未起步就被另一条命令打断。
- 保留已发射正伤害空袭弹体、己方/盟军试打、地面目标格和敌方碾压的检测；治疗弹排除、类型过滤、内外计算和手动取消继续生效。
- 同步 Scatter 使用原版默认坐标；飞机原生散开直接带飞机坐标，且无需等待同步事件。因此本版不能等同于原版三星即时散开的方向与时机，也不保证所有弹种可躲。

文件版本 **1.0.30.4**，Assist **58**、Runtime **4**、入口管理器 **3**。先退出游戏和工具箱，成套更新 EXE/DLL。已有完整安装下载 Update，首次使用下载 Windows 完整包。

验证范围与最终二进制哈希见 [verification/pre5](source/verification/pre5/summary.json)。真实完整游戏、弹道物理及双机联机仍待复测。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.4

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.4) · [实现与验证](source/REVIEW-1.0.30-pre.4.md)

- 高刷调度改用 QPC，按微秒保持节拍；120 Hz 为约 8.333 ms。实时读取鼠标客户区位置，解决额外拖屏更新重复旧坐标的问题。三档策略保持「仅光标 / 光标＋拖屏 / 全部」。
- 高刷页增加实际补充绘制频率与最长间隔，便于复测；这些统计不是显示器实际 FPS。全部模式仍不进行运动插帧或提高逻辑帧率。
- 空袭躲弹改为看到已发射的正伤害弹体后触发，按弹道横向避让；新一发绕过上一发的 12 帧等待，同一发不反复触发新发条件。支持己方/盟军试打与地面目标格，排除治疗弹。碾压分支保留。
- 原版在飞机开火流程中直接触发目标格散开，本功能仍使用正常同步移动指令，受联机指令延迟限制，不能保证完全复现原版精英或躲过所有弹种。
- 科技全开、地图显示继续保留，没有新增平台或联机禁用。

文件版本 **1.0.30.3**，Assist **57**、Runtime **4**、入口管理器 **3**。先退出游戏和工具箱，成套替换 EXE/DLL。已有完整安装下载 Update，首次使用下载 Windows 完整包。

已进行受控 x86、宿主调度和 PE/ABI 验证，报告见 [verification/pre4](source/verification/pre4/summary.json)。真实 Windows 画面观感、完整弹道物理与双机联机仍待实际游戏复测。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.3

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.3) · [稳定版 1.0.29](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.29) · [源码](source/) · [实现与验证](source/REVIEW-1.0.30-pre.3.md)

- 新增实验性高刷策略：**仅光标 / 光标＋拖屏 / 全部**，默认仅光标。后两档画面目标 60–240 Hz，默认 120；“全部”在不拖屏时也持续调用原版画面合成与呈现。光标沿用光标页设置。不会提高逻辑帧率或进行运动插帧，单位动作仍受原版逻辑更新限制，实际帧率需要实机确认。
- 高刷拖屏覆盖右键拖屏、屏幕边缘滚屏。按时间折算速度型滚屏，物理位移型拖屏保留原来的位移量；切回仅光标、离开前台、聊天和菜单期间恢复原版画面路径。
- 小地图在视口内放不下正常滚动区间时固定该轴中心，修正上下限反转造成的来回抖动。已有其他补丁占用接口时保留对方补丁。
- 智能卸载支持载员已经走出后的短时接续，缓解 Ctrl+D 的原版 D 抢先卸载；工程师/间谍可以在走出时接续占领/潜入，部署未被观察到时在原命令有效期后重试。
- 刷弹恢复攻击改为下一逻辑帧发出，不再等待观察到可能一闪而过的 Stop 状态；新一次开火可直接开始下一轮。继续排除航母/无畏等母体及子单位。
- 自动躲弹纳入实际锁定己方单位的己方/盟友飞机攻击，便于强制攻击自测；仍只预判空袭与碾压，不是全弹种躲避。
- 科技全开与地图显示功能保留，没有新增平台或联机禁用。战网掉线原因尚未确认，新增状态诊断便于继续定位。

实验性预发布：受控测试执行真实 x86 Runtime，但输入、世界对象和绘制器使用测试替身；不能替代 Windows 实际帧率、完整游戏或双机联机测试。文件版本 **1.0.30.2**，Assist **56**、Runtime **4**、入口管理器 **3**。先退出游戏和工具箱，成套覆盖 EXE/DLL，保留配置、资源和 cache。

## 历史版本说明

# RA2 Toolbox 1.0.30-pre.2

[预发布完整包与更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.30-pre.2) · [稳定版 1.0.29](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/tag/v1.0.29) · [源码](source/) · [本次实现与验证](source/REVIEW-1.0.30-pre.2.md)

以 v1.0.30-pre.1（02614f99bde1bc8022e0699ae9e5133f975a2c0e）为基线。

- 特殊下令光标：高级移动攻击使用原版移动攻击图案的内绿外黄配色；飞机分配投弹使用原版攻击图案的内绿外红配色。按键 CS 复用移动攻击图案；磁电、进驻、飞碟主武器也有独立颜色。
- 按住已配置的特殊键、选中对应己方可用单位时显示；松键、切出游戏、进入聊天/菜单、提示过期、关闭功能或换局恢复。侧栏、雷达、边缘滚屏及原生维修/出售/超武等光标保留。
- 提示复用现有高刷光标，包括外置图层、透明 SHA 和游戏内硬件光标模式。需要同时开启高刷光标及战术助手；无光标诊断模式或暂停光标时不显示提示。没有设置按住键的纯鼠标操作不会常驻特殊图案。提示只表明当前模式，不代表鼠标下每个目标都可接受该命令。
- 自动刷弹排除普通飞机、飞行中的载具、航母/无畏等生成子单位的母体、航母小飞机、无畏导弹及 MOD 同类子单位。以 SpawnOwner、Spawned、Spawns/SpawnsNumber、MissileSpawn 属性判断，改名不绕过。母体/子单位也不能被按键刷弹重新纳入；普通飞机仅保留显式按键刷弹通道。
- 光标状态仅在 EXE 的现有模块间传递；没有新增游戏 Hook、注入入口或下令事件。选区提示按 32ms 节流并使用独立只读页缓存，松键在显示时再次检查。刷弹在两个计算后端的统一采集和提交校验处排除这些单位。
- 文件版本 1.0.30.1；Assist 55、Runtime 4、入口管理器 3 不变。先退出游戏和工具箱，成套覆盖 EXE/DLL，保留配置、资源和 cache。

验证：本次 136 项真实 x86 DLL 受控用例通过，覆盖单机/联机事件队列、外部/Runtime 计算、刷弹排除以及原有刷弹/躲避恢复；宿主模式、类型过滤、输入及共享模块检查通过，Windows PE/ABI/版本/依赖静态校验通过。预览来自真实像素换色函数。尚未进行 Windows 完整游戏或双机联机实测。

## 继承 pre.1 的功能与限制

# RA2 Toolbox v1.0.30-pre.1 预发布版

- 高级移动攻击：先选当前实际射程内的目标，再按价值或伤害比例排序。读取原版当前武器选择、精英武器、对空武器和实际射程，涵盖高度、AirRangeBonus 与最小射程。
- 射程外只用普通移动接近；进入射程后发送目的地为空的攻击事件，避免 IFV 提前交给原版追击后继续贴近。旧移动方案返回时若目标已入射程，立即作废并重新计算。
- 新增强制刷弹（测试）：支持按类型名单自动启用，以及按住快捷键对选中单位启用。当前实际武器 Burst≥2 且连射未完成时，尝试停止再恢复原目标；普通与三星武器分别按实际状态判断。
- 新增自动躲弹（测试）：对己方可移动地面单位预判敌机攻击、近距离碾压并短距避让。不限星级，可配置类型名单；暂缓同一单位的刷弹和高级攻击，威胁解除后恢复仍有效的任务。
- 两项新功能默认关闭，均支持工具箱计算或 GameRuntime 计算。共用统一游戏线程、事件入口、五类规划配额和单位仲裁；不新增游戏 hook，不修改星级、能力、ROF 或连射计数。


本版为 pre-release，供实际游戏验证。高级移动攻击、刷弹和躲避的受控回归会随源码保存；这些测试执行真实 x86 DLL、原版武器/射程函数及事件构造，但对象、输入、地图与任务观测使用人工场景，不等同于完整游戏、弹道物理或双机同步测试。
刷弹是尝试利用原版中断连射行为，效果受引擎修改、剩余连射时间和网络事件延迟影响，不保证每次成功。自动躲弹当前预判锁定己方的敌机攻击与近距离碾压，不是全弹种弹道预测；建筑、飞行中的己方单位、无可用落点、追踪弹与瞬时伤害不保证规避。大规模场景按预算轮换处理。
兰博玩上传的磁盘 EXE 将连射重置位置转入未包含在文件中的启动代码；该平台刷弹实际效果未确认，测试只覆盖工具的下令与状态恢复。


受控回归共 360 项通过，结果与限制随源码保存。Windows.zip 为完整运行资源包，Update.zip 用于已有完整安装。先退出游戏和工具箱，成套更新 EXE/DLL，保留配置、资源与 cache。Release 附件仅含运行文件，源码见仓库 source/。

使用 Zig 0.13.0 运行 source/build-zig.sh 或 source/BUILD_ZIG.cmd。文件版本 1.0.30.0，Assist 55，Runtime 4，管理器 3。
