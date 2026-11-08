# 1.0.46 修订：四类原生生产时间提示

调查前已检索研究 INDEX、Hotfix27 原生建造提示及速度上限记录，并查询同一个 SQLite 笔记 version 165。功能基线为 `e5e9abd69cdf2963f7bef3dd8fbc3e12c4537a63`；所有改动按用户要求并入原 1.0.46 正式版。

公开证据目录：[release-146-revision-20261108](evidence/release-146-revision-20261108/)，生产测试位于 `source/assist/tests/build_time_{native.py,tooltip.cpp,tooltip.py}`。

## 问题与修复

已确认原先只允许页签 0/1、cameo kind 7、BuildingType 数组、Building scratch receiver 与 Building 工厂产物，因而坦克、步兵与飞机的合法侧栏项在进入计算前即被排除。只放开页签不能修复其余硬编码，更不能把 Unit/Infantry/Aircraft 的 Type 填到 Building 接收对象中调用。

生产改动仅为 `source/assist/source/build_time_tooltip.h` 与新增 `build_time_types.h`。保留现有用户开关和默认值。允许四个侧栏页签，按当前 cameo 的原生类型选择正确的 Type 数组与接收对象布局，并让当前工厂剩余时间使用同一类别的实例布局。超武、特殊项和未知类型继续返回原文。

| 类别 | cameo Type ID | 原生实例 ID / 工具箱 kind | Type 数组 | 实例 vtable | Type vtable | 实例 Type 偏移 | 原生 Size |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 建筑 / 防御 | 7 | 6 / 3 | A83C68 | 7E3EBC | 7E4570 | 520 | 720 |
| 坦克 / 舰船 | 40 | 1 / 2 | A83CE0 | 7F5C70 | 7F6218 | 6C4 | 8E8 |
| 步兵 | 16 | 15 / 1 | A8E348 | 7EB058 | 7EB610 | 6C0 | 6F0 |
| 飞机 | 3 | 2 / 0 | A8B218 | 7E22A4 | 7E2868 | 6C4 | 6D8 |

地址与大小均为十六进制。四类共同 Owner 偏移为 21C；Type getter 分别为 459EE0 / 741490 / 51FAF0 / 41C200。对象 vtable+84 指向 6F3270，转发到各自 vtable+88；对象/类型 WhatAmI 位于 vtable+2C。原生侧栏调用的 48DCD0 类型解析器也执行验证了四个 Type 数组。

## 原始证据与安全边界

私有样本 SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。原始 EXE、完整映像和反汇编不加入仓库或交付包。固定 YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2` 类头及 Phobos `f06797ec39f2b60b9fa5b6da69288c36d189b712` 的只读查询方式作为对照，路径和 blob SHA 记录于 `production-time-upstream.json`；既有 GPL-3.0 署名保留。

`build_time_native.py` 在 Unicorn 中执行完整 `6F47A0..6F4955`，所有被调用函数同样为原始指令，**没有替换 native service**。47 个受控场景均正常返回，四类接收对象的实际读取集合严格为 vtable、Owner+21C 和各自的 Type 字段；没有接收对象写入，也没有栈外原生写入。6C8 是已经验证足够的只读前缀，**并非四类真实实例的 sizeof**。真实 Size 通过各自原生虚方法另行执行确认。

Unit 专有的 Naval(+CCE) 选择 House+5380 陆军 / +5388 海军工厂计数；飞机 +5378、步兵 +537C、建筑 +5384。国家倍率通过 50C0A0 按 Type 类别区分，防御 BuildCat=5 使用独立 +144 倍率；Building 专有 Wall(+1571) 分支保持不变。低电、上下限、零速回退、多厂倍率、关闭多厂倍率与逐次截断均实际执行。

新增门槛核对所选实例/类型虚表绑定与 getter/WhatAmI 指令，核对完整 TimeToBuild 函数和 TypeBase 指令哈希。未知虚表、未知 getter、第三方改写这些查询方法均返回原文；Debug 的参考签名跳过不能作为部分接收对象的安全证明，不能绕过此结构门。没有扩大到任意 MOD/平台扩展算法兼容。

## 保留的行为和一致性

总时间仍为 `54 × clamp(trunc(nativeTime / 54), 1, 255) + 1`；原生调用保留价格、BuildSpeed、国家、Type、供电和逐次多厂截断。`+1` 延续现有就绪显示口径，并未因此证明每种实际入口次序都额外耗费一个完整游戏帧。

剩余时间仍使用工厂当前阶段的 duration/start，加上后续阶段的新 stored rate，保留电量变化时当前阶段的原始截止时刻。54 优先显示已就绪，其次暂停、缺钱；非法阶段或计时字段只保留总时间。当前工厂与产物的 Owner、实例类型和 Type 必须匹配。

返回前复核 house/match/frame、tab、row、count、cameo index/kind、Type 数组中的指针、Type vtable、factory 指针和已绑定产物身份。页签或侧栏源发生变化就返回原文；来源已失效时不再读取旧产物。

名义单机 / 联机 FPS、当前自定义 FPS、原生同框文案“建造”、255 UTF-16 单位上限、原文内存不改写均保持。`guard.cpp`、`native_tooltip_runtime.h` 和 `runtime_fp_scope.h` 与基线 e5e9abd 的字节完全一致，既有 FP 保存恢复、SEH、runtime lease 和共享原生框所有权路线未改。

容量检查从 `count >=75` 改为 `count >75`，Assist 在有原文时接受最后一个有效槽位。但原生 Strip.Help 的 6A9304/6A9307 也拒绝 count>=75，可能先返回 null；共享 hook 仍保留 null，**本项并非原生 75 项提示框的完整修复**。

## 实际验证结果

- `production-time-entry.json`：实际生产 tooltip 入口与 bridge 的 vec/item/kind，207 场景、13,334 项检查通过。159 次 native thiscall 使用明确测试替身检查真实 scratch receiver 字节，并返回上述 47 例完整原指令的结果。
- `production-time-ubsan.json`：同一 207 场景、13,334 项检查通过，UBSan 无错误。
- `production-time-native.json`：47 例完整原始 TimeToBuild，另 4 个原生 Size 和 4 个原生侧栏 Type resolver；零 native service 替身。
- `production-time-debug-consumers.json`：既有 355 项 / 11 组原生消费者签名策略回归通过。
- 所有收据记录源 SHA-256 且测试前后源未变化。生产 diff 无空白错误。

入口夹具是 Linux 受控执行：进程内存、Win32 所有权/时钟和 thiscall 是明确替身；64 位测试栈的低 32 位参数恢复只存在于夹具。使用 2 字节 wchar_t 验证 UTF-16 单位与代理对，没有运行 MSVC FP/SEH 边界本身。没有 Windows 游戏、真实工具提示视觉、实际生产完成时刻、联机或 FPS 测试，不能据此作这些声明。

生产修改与专项检查已冻结；完整产品构建、PE、打包、同版本 Release 附件替换与源码标签对应关系由最终发布收据记录。Windows 文件版本使用 1.0.46.3 区分修订附件，界面与标签仍为 1.0.46。
