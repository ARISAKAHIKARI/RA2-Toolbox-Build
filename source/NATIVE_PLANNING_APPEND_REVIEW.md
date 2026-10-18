# 原生“追加进路径”研究记录

更新：2026-10-18。当前可执行发布版仍为 1.0.11；本轮只有研究、测试与说明，没有交付新的混合路径快捷键，也没有修改 GameRuntime。

用户报告的顺序是：先给谭雅下达针对建筑的任务，再把普通步兵追加到该规划路径。工程师、间谍组合还可能使普通步兵走到建筑位置。这里区分“任务被复制并分派”和“抵达目标后实际爆破、占领或潜入”。

## 本轮修正及结论

此前 review-111 夹具把攻击目标和目的地同时设为建筑。本轮执行参考 EXE 的 FootClass::ClickedAction，发现已解析的 Sabotage、Capture、Enter 动作都把建筑作为目的地，攻击目标为空。因此此前的结果不能视作实际 UI 输入的完整复现。

| 输入动作 | 任务 | 攻击目标 | 目的地 | 后续目标 |
| --- | --- | --- | --- | --- |
| Sabotage（16） | Sabotage（17） | 空 | 建筑 | 空 |
| Capture（9） | Capture（8） | 空 | 建筑 | 空 |
| Enter（3） | Enter（7） | 空 | 建筑 | 空 |

这个测试从**已解析动作**开始，尚未执行真实鼠标悬停、地图点击和动作选择。因此不能把 Enter 测例单独当成已确认的间谍 UI 行为。原生 FootClass::ClickedAction 及其虚调用实际执行，在 TechnoClass::ClickedMission 入口记录参数后返回；只有本地房屋控制查询使用夹具环境。

按上述字段修正初始事件后，原生 PlanConnect → 复制 → PlanCommit → 下一节点分派的三个候选任务再次通过。普通步兵收到保留原任务与建筑目的地的事件，没有改写该步兵的 C4/工程师类型属性。

这仍不能证明最终动作成功：在参考程序的最终 Sabotage 分支，单位自身 InfantryType.C4 为假时拒绝继续；为真时通过。无规划对象、未提交规划和已提交规划的六个局部正负对照结果一致。该测试仅执行 0x51A4BF 起的判断块，到通过或拒绝分支立即停止，不代表整段 UpdatePosition、路径运动、其他执行分支或爆炸结算都已验证。

## 完整点击链的实际阻碍

ClickedMission 位于 0x6FFBE0。规划模式下，在构造源对象引用前，0x6FFC6C 跳向 0xB7E614。当前参考文件对应目标的 16 个字节全为零；仿真在进入该地址时停止，记录为 `BLOCKED_MISSING_REFERENCE_TRAMPOLINE`。

没有用猜测代码补齐跳转目标，没有把这条链报告为通过。离线文件不足以说明真实启动后是否有加载器填充这段区域；也不足以判断用户使用的原版或平台程序是否相同。

参考文件为现有 CnCNet `gamemd-spawn.exe`，SHA256：
`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。
这里“未打补丁”仅指没有安装本工具补丁，**不指未经平台修改的原版 EXE**。

## 已确认的原生追加机制

YRpp 将相关事件命名为 PlanConnect（0x2A）和 PlanCommit（0x2B）。PlanConnect 中的目标引用用于按位置找到既有规划节点，不是能力转移指针。

参考程序通过 0x637E00 → 0x639800 → 0x6367B0 复制既有节点的 111 字节任务，只替换源单位引用，保留任务、目标、目的地、后续目标和规划标记；此阶段没有重新检查追加步兵的 C4、工程师或间谍能力。

PlanCommit 后，通过 0x6385C0 → 0x636590 把下一节点的事件加入游戏内部待执行列表，并把规划标记设为 2。这是游戏自身依据规划事件派生任务，不是工具私自向 OutList 补写。

## 可复现检查

依赖 Python 的 pefile、unicorn；EXE 由测试者提供，仓库不包含游戏 EXE。使用本项目 1.0.11 GameRuntime.dll 只为复用夹具 ABI，不启动 Runtime 或安装补丁。

```bash
python assist/tests/planning_boundary_probe.py gamemd-spawn.exe GameRuntime.dll --output planning-boundary.json
python assist/tests/planning_connect_probe.py gamemd-spawn.exe GameRuntime.dll --output planning-connect-destination.json
```

本轮结果保存于 `verification/planning-followup/`：3 个动作边界、3 个修正载荷后的追加分派、6 个最终 C4 判断块对照，共 12 个通过的有界检查；完整点击链另有 1 个明确的阻塞结果。旧 `verification/review-111/planning-connect.json` 保留为历史候选载荷结果，不能替代本轮结果。

合成环境包括对象登记、内存分配、建筑坐标、本地控制、键盘状态和静止移动查询。追加测试不模拟真实地图寻路及完整任务结算；判断块测试不模拟其他到达建筑的执行路径。没有真实 Windows 游戏、LBW 或双机联机验证。

## 尚未交付及后续所需证据

- 尚未复现“追加普通步兵后最终爆破建筑”。已看到的类型校验与用户报告之间仍有未解释差异。
- 工程师/间谍组合的“移动到建筑格”不能当作“占领或潜入成功”。
- 下一步需要在用户确实复现的游戏版本上核对：准确 EXE/平台版本、是否加载 Ares/Phobos，以及先下令后追加的录像或可复现存档。若启动器动态填充跳板，需要运行时对应代码/日志才能补全此处离线证据。
- 1.0.10 起的 C4 Beta 是独立实验性判断修改，不能称作原生混合路径实现。本轮未改变其行为。
- 后续接入原生规划事件时仍须扩展 GameRuntime 的统一构造/提交接口，不允许功能私自安装 Hook 或写 OutList。
