# Hotfix22：接通真正的 Z 路径点

## 问题和结论

用户反馈“路径点目前也不会渲染”。Hotfix15–21 的 `localui::Engine::paths` 实际只读取 `Foot+0x588` 导航队列，并附加字体数字；原生 Z 规划使用另一套 PlanMgr 节点和 PlanningToken。导航队列为空不代表没有规划路径。这是源码确认的接线缺失，不把它描述成已经完成后偶发的画面问题。

本轮替换这一错误数据源。`native_planning.h` 读取完整节点/令牌表，按成员的剩余路径重建共享前驱分组、巡逻闭环和单位入线；`local_ui_renderer.h` 适配 Vera 的外边、彩色虚线、节点方块、任务徽标及悬停/保留高亮。路径图不再依赖主画面对象裁剪清单、导航队列或字体。远处单位的路径穿过当前视口也能进入绘制清单。

## 调查前记录与固定来源

已查询 `INDEX.md`、Hotfix15/21 记录、`NATIVE_PLANNING_APPEND_REVIEW.md`、现有 `assist/tests/planning_connect_probe.py`、`planning_boundary_probe.py`，以及同一持久 SQLite v96。这里读取旧夹具的布局和证据，没有重新运行它们。

上游：[Vera20K 3d42426](https://github.com/ARISAKAHIKARI/vera20k/tree/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f)，YuriPlanet 及贡献者，GPL-3.0：

- `src/sim/world/planning.rs`：规划节点、成员/前驱分组、loop、pending、颜色选择、current/mega/connection 绘制图。
- `src/app/presentation/render/planning_lines.rs`：原生绘制阶段、线宽/虚线、节点、hover/highlight 和鼠标徽标。
- `src/sim/mission/mega_mission.rs`、`src/sim/movement/{navcom,ground_pose}.rs`：当前任务和进攻移动目标的只读投影。
- `src/render/{surface_line,native_surface_format}.rs`：严格误差步进、4开4关图案和 RGB 向零除法插值。
- `tools/spatial_oracle/planning_mode.py` / `.meta.json`：上游已有的布局、入口和受控证据范围。该 meta 的绘制案例是输入指定的线/标记，不是整个 PlanMgr 端到端验证。
- Ares-Developers/YRpp 的 `PlanningTokenClass.h`、`TechnoClass.h`、`FootClass.h`、`MissionClass.h`、`NetworkEvents.h` 用作交叉核对；未据其未命名字段猜测坐标。

历史支持样本 SHA-256 沿用 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 与 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。新增字段采用固定上游和旧原生夹具布局交叉核对；**本轮没有对这两份 EXE 新执行、反汇编或实机验证**，不能把上游 oracle 当成本轮结果。

## 地址和只读契约

| 项目 | 读取/语义 |
| --- | --- |
| `0xAC4CF4` | 原生规划激活 byte；非激活返回空图 |
| `0xAC4B30`、`0xAC4C78` | 全局节点向量、令牌向量；向量 items/capacity/count 为 +4/+8/+10(hex) |
| Techno `+0x514`，token `+0` | 相互验证的规划令牌与所属对象 |
| token `+4`，`+0x8C/+0x90/+0x94/+0x98` | 节点向量、pending、循环长度、循环起点、循环改变 byte；允许路径重复引用同一节点 |
| node `+0` 向量，member `+0/+4` | 有序成员记录；对象指针、111字节事件指针。只读绘制所需前30字节 |
| event `+7/+0xB/+0xC` | 源对象 ID、类型 tag、任务；任务11/29 对应鼠标433/434帧 |
| event `+0xE/+0x18/+0x13` | TarCom → NavComAux → NavCom 的5字节 NetID；按首个非空 tag 选择，失效目标不换成另一个目的地 |
| Cell tag `0x0B`、Techno tag `0x34` | cell为 y×1000+x；对象按完整 Techno 表的 ID 解析，保留对象 Z 和建筑中心 |
| Foot `+0x5A0/+0x5A4` | 当前辅助/主目的地；Attack 使用 `+0x2B4`，无目标使用原始 Location |
| Foot `+0x5C4/+0x5C8/+0x5CC/+0x5D1` | MegaMission、MegaDestination、MegaTarget、已取得进攻移动目标 latch |
| `0xAC4CCC`、`0xAC4BF0` | 原生 hover 和保留 highlight 节点；不自行调用命中测试或改选区 |
| `0x633BC0/0x63B050` | 规划事件坐标/目标选择的上游标识；不是本轮新增调用入口 |
| `0x63B0A0/0x63B150/0x63B2F0` | 外边、内部图案、后置高亮阶段；这里只读重建 |
| `0x63D910/0x63D960/0x63D400/0x63D480` | 线外边/内部、单位标记外边/内部的上游 thiscall 绘制叶；**不调用其 ABI** |
| `0x6612C0` | RGB插值低byte amount，带符号整数除256向零截断；dark80、bright100 |

`Snapshot` 缓存本次图的每个已读块，再通过 **readFresh** 复核全部块。旧页缓存不能充当第二次读取。检查向量容量、节点归属、token反向关联、事件源ID、循环索引、布尔值和有限坐标；限制8192节点/令牌、65536成员/路径引用及8MiB读集。无效或改变的图不发布部分线。原有相机/会话/150ms新鲜度检查继续由任务外层负责。

不修改任务、事件、选区、RNG或游戏计时器，不新增Hook、游戏调用或协议字段。普通飞机兼容路径仍由 `airway::lines()` 和现有 World 覆盖层负责，本次没有更改其发令/续航逻辑。

## 绘制顺序

1. 所有节点、路径和单位连线的黑/白外边。
2. 节点路径的阵营色4开4关虚线、任务徽标和6×6标记，随后补填节点标记。
3. 当前行动段、已取得目标的进攻移动段、规划入线；Limbo保留相应端点标记。
4. 独立第五UI层：16×16 hover、24×24 retained highlight 的三层白框，rank205在普通action lines rank200之后，仍低于状态/消息/光标。

所有新增层包含在发布、短暂持有和清除组中，避免退出规划后残留独立高亮纹理。任务徽标素材失败只缺该图标，不抹掉整张路径图；取消错误的字体数字依赖。

## 边界与验证

本轮实现针对已知零售ABI的有效地图格、Techno/Cell目标和当前阵营/盟军规划。Cell高度复用当前地表/桥面解析器，尚未逐像素证明所有坡面与原生 `Cell+48/+58` 一致；徽标37px门限采用普通hypot，没有移植完整x87近似开方；任意mod目标tag、战役特殊控制阵营映射和被补丁改写的布局不声称全覆盖。没有把 node+0xAC 等缓存字段误当 XYZ。

按用户要求只做 Zig 0.13.0 混合架构交叉编译，以及 PE/导入/版本/manifest/ABI 静态检查。没有运行 CPU 夹具套件、Vulkan、D3D、Windows loader 或游戏，不宣称已实机确认显示或性能。最终产物/哈希见 `evidence/render-hotfix22-final.json`。
