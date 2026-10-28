# 飞机派出后的单对象原生取消选择（2026-10-28）

## 问题、样本与先前资料

单目标120%策略只派出所需飞机；成功提交的飞机取消选中，剩余对象的选中状态与原任务必须保持。不能用“全部取消，再选择剩余对象”代替单对象操作。

调查前检索 `INDEX.md`、既有选择/队形记录及 Library 中同一份 `RA2-Reverse-Engineering-Notes.sqlite`（读取版本74）；没有找到已验证的单对象 Deselect 入口。源码已有 `selection=0x6FBFA0` 和全体 `unselect=0x48DC90`，后者不满足这次要求。

只读私有样本由所有者提供，均不加入仓库或发布包：

- LBW SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 战网 SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。

公共参考：[Phobos DeselectObject.cpp](https://github.com/Phobos-developers/Phobos/blob/af2ab96a5ddd1fc4d0f56ae6cd5fd8536a7f2101/src/Commands/DeselectObject.cpp) 的命令实现对 `CurrentObjects.GetItem(0)` 调用 `Deselect()`；[YRpp ObjectClass.h](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/ObjectClass.h) 声明该虚函数。公共源用于定位，下面的地址与实际语义另在两份支持样本中核实。

## 新证据

| 位置 | ABI / 状态 | 已核实行为 |
| --- | --- | --- |
| Aircraft `0x7E22A4`、Unit `0x7F5C70`、Infantry `0x7EB058`、Building `0x7E3EBC` 的虚表 `+0x150` | 四类均为 `0x5F44A0`；thiscall，ECX=对象，无栈参数，`ret` | 同一单对象取消选择实现。 |
| `0x5F44A0..0x5F451A` | 对象 `+0x83` 是选中位 | 位为零时直接返回；否则从当前选区查找本对象，删除其一项并保持余项相对顺序，再清本对象选中位。未调用任务/目标设定器。 |
| `0xA8ECB8` | 动态 vector；`+4` items，`+8` capacity，`+0x10` count | 原始动态 vector 虚表 `0x7E4F64` 的 `+0x10` 为 `0x40CF00` FindIndex；Deselect 通过它查索引，然后自行减 count 和左移后项。 |
| `0x4AEB10` / `0x4AEB30` | thiscall；后者有一个栈参数并 `ret 4` | 读/设 DisplayClass 的当前对象：`0x87F7E8+0x119C` 有效位、`+0x11A0` 指针。Deselect 只在当前对象等于自己时调用 setter 清空。 |
| `0x48DC90..0x48DCB1` | 既有全体取消函数 | 循环读取当前选区首项并调用虚表 `+0x150`，直到 count 为零；因此不能直接用于仅取消派出飞机。 |

两份样本的上述四个实际执行函数完全一致。仓库只保存长度与 FNV-1a 校验值，不保存原始函数字节：Deselect 123字节 `1064603B`、FindIndex 38字节 `E88F130E`、当前对象getter 23字节 `0EF2E454`、setter 24字节 `E51B873C`。

## 实现与验证

独立 `assist/source/aircraft_selection_bridge.h` 提供 `dispatchedAircraftSelectionReady` 与 `deselectDispatchedAircraft`。只允许有效 Assist 游戏线程入口中的己方存活 Aircraft/Unit，且该对象确实选中、在当前选区恰出现一次。校验虚表、选区边界与完整原生调用链后，调用原生 `0x5F44A0`，核对选中位清除和 count 减一；失败不调用全体取消或重新选择。调用者负责只在原生 Attack 事件确实入队且对象身份仍相同后执行取消选择。可以在提交前先检查 Ready，避免已知不支持的选择入口。

- `assist/tests/aircraft_deselect_native.py`：两样本136项，直接执行完整 Deselect、FindIndex、getter/setter，不替换其中任何一个。四种对象虚表、1/3/512项选区、首/中/末项、重复调用、未选中、不在选区及三种当前对象关系。断言其他对象各0x800字节不变、剩余选区顺序不变、写入仅限选区/本对象选中位/相同当前对象/调用栈，并核对 ESP 与 EBX/ESI/EDI/EBP。
- `assist/tests/aircraft_selection_fixture.cpp` + `aircraft_selection_runtime.py`：Zig 编译生产 helper，使用同两样本原生选择代码，38项通过。验证 Aircraft/Unit 正例及四处原函数修改、方法指针、错误所属方/类型/生命/选中位、选区成员重复/缺失、非法计数/容量、读取失败与非游戏入口负例；拒绝时对象和选区保持原样。
- 可提交的结构化证据：`research/evidence/2026-10-28-aircraft-selection.json`。仅含样本/夹具哈希、地址、ABI、用例类别与结果，不含私有路径、EXE、函数原字节或转储。逐例日志保留于私人工作目录，可用上述测试的 `--output` 重建。

排除方案：直接写 `+0x83` 会遗漏选区 vector；调用全体取消再重选会变更未派出对象的选择流程；在收集所有所选飞机时提前取消自动任务会误伤未派出成员。helper 本身只处理选择，不处理任务、手动版本、仲裁 lease 或事件回执。队列失败不得调用它；成功后旧任务清理也不得取消新事件的 lease/回执。

证据等级为私有支持样本静态核对与受控 x86 原生执行。尚未证明完整 Windows 点击手势、实机选中画面/音频、MOD 外来 detour 或真实双机联机；完整飞机分配/原输入接管由专用功能回归另测。函数签名或选区不支持时应拒绝本次派出，不能因 helper 失败重新放行已明确接管的原点击、让整组选中飞机收到攻击。
