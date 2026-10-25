# 建筑维修扳手：独立状态绘制、原版帧与位置

## 问题、前置查阅和样本

用户回报副相机仍不绘制建筑维修扳手。本轮先读 AGENTS、研究 INDEX，以及持久 SQLite 中已有的建筑独立部件、AnimClass 与物件光照结论；这些记录没有证明维修扳手来自 AnimClass，不能继续把缺图归咎于建筑动画枚举。

支援 EXE SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。只读私人采集 `RA2Master-Capture-20261025-003340-3568` 的 `01-off / 02-on / 03-off`。采集里实际维修中的建筑数量为零，因此下文维修开关与遮罩组合是受控执行，不是用户新版本实机画面的复现或验收。未发布 EXE、dump、WRENCH.SHP、色盘或 framebuffer。

## 根因：它不是 AnimClass

原版 `TechnoClass::DrawExtras` 入口为 `0x6F5190`，建筑虚表 `0x7E3EBC + 0x110` 指向此入口。维修扳手由其 `0x6F5253..0x6F5345` 分支直接调用 DSurface::DrawSHP，完全不经过 AnimClass::Array。

此前 renderer 只枚举建筑 body、独立部件与已创建 AnimClass，自然没有维修扳手这一条状态绘制路径。它也不是 `RepairProgress`——后者服务于医院／维修厂内部进度，不能拿来选择悬浮扳手帧。

| 地址／偏移 | 原版用途与证据 |
| --- | --- |
| Building `+0x6E8` | IsBeingRepaired；`0x6F527F` 读取并决定是否进入维修图标分支 |
| Techno `+0x3CD` | IsSinking；入口 `0x6F519B` 为真时跳过这些额外状态图形 |
| `[0x89DDC8]` | 全局 WRENCH.SHP 引用；`0x6F533A` 读入绘制参数 |
| `0x819404` | 原版资源名 `WRENCH.SHP`；`0x45E98F..0x45E99E` 用 FileSystem loader 载入并写入全局 |
| `[0x87F6C8]` | MOUSE_PAL 转换器；`0x6F532D` 把它放入 EDX |
| Building virtual `+0xAC = 0x459EF0` | 返回 `Location - (128,128,0)`，作为悬浮图标的世界坐标中心 |
| `0x6D2140` | Tactical::CoordsToClient；上述世界中心转换为最终 draw point |
| `[0xA8ED84]` | 游戏 CurrentFrame，用于扳手动画相位 |
| `0xA8EB60` / `0x5FB2E0` | GameOptions / GetAnimSpeed(14)，由当前游戏速度得动画周期 |
| `0x6F5340 -> 0x4AED70` | ECX=DSurface、EDX=Mouse converter；14个栈参数，`ret 0x38` |

参考一手结构：[YRpp FileSystem.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FileSystem.h#L30)、[BuildingClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingClass.h#L335)。字段与计算结论均另外核对原始指令，未仅凭 header 推测。

## 七帧动画按游戏速度计算

原版 `GetAnimSpeed(14)` 得到 `112 / (GameSpeed + 1)`。维修分支再做向零取整的除4、至少2，然后按 CurrentFrame 取相位：

```text
period = max(2, (112 / (GameSpeed + 1)) / 4)
frame  = (CurrentFrame % period) * 6 / (period - 1)
```

当前可选 GameSpeed 0..6 对应 period `28,14,9,7,5,4,4`，可选择的帧编号范围是0..6；较短周期会跳过某些中间帧，这是原版公式本身的结果，不能擅自改成 `CurrentFrame % 7` 或每个实例从第0帧起播。

## 位置、色盘和层级

原版维修中心是建筑 Location 的两个地图轴各减128 leptons，Z不变。它不是 building body 的 SHP 绘点，也不应再加通用 `camera::anchorY(kind=3)`。由外部相机把此中心按当前投影和相机位置转换后，按 SHP 的中心原点绘制即可。

原版 draw flags 为 `0xE00`，亮度参数1000；没有启用 ZRead / ZReadWrite。捕获的 MOUSE_PAL 是普通 ConvertClass（vtable `0x7E5358`、BytesPerPixel=2、ShadeCount=1）。它没有 LightConvert 的 UsedPalette1 `+0x188`，应读取 `PaletteData +0x174` 指向的一行256项 RGB565，再按位扩展为RGB8；索引0保持透明。一个 shade 的鼠标色盘不受地图环境亮度或建筑所属方染色影响。

因此维修扳手作为最后的状态图形绘制：不写场景depth，不被屋顶或前景建筑 body 的深度遮住。不能混入建筑本体深度图，也不能先套单位光照再作为“动画”排序。

## 新接口与实现边界

- `external_reader/source/repair_visuals.h` 提供 `Sprite` 与 `sample(read, building, id, out)`。只读验证当前建筑身份、维修／下沉状态、位置、全局资源指针、游戏帧和速度；身份／维修状态或全局资源在采样中变化则拒绝。调用方应从已经通过场景可见性与存活筛选的建筑中收集图标；现有预览黑幕选项仍由场景控制，不在 helper 内重造原版复杂 shroud 投影。
- `camera_repair.h` 提供独立的 `repairvisuals::Assets`。优先解码游戏已载入的全局 SHP；当它是尚未展开的 SHPReference 时，回退读取原资源名 `WRENCH.SHP`。使用原 MOUSE_PAL 的RGB565行，保留裁剪帧相对完整画布中心的位置，alpha=255。7个姿态共享缓存；每个 simulation frame／资源版本只检查一次色盘，不为每座维修建筑重复读取512字节。
- 图像设置 NaN depth / writeDepth=false，由调用方在 world actor pass 之后以 neutral light 绘制。helper 不调用游戏函数，不创建 hook 或游戏命令，也不修改原资源。
- 模板为不提供 read/files 的旧 renderer fixture 后端返回 nullptr，避免测试替身因新增状态图形而被迫实现整个文件系统。

## 原版受控验证

[native_repair_visuals_capture.py](../assist/tests/native_repair_visuals_capture.py) 在三份采集中各选三种建筑，执行原版 `0x6F5190` 的相关分支，保留原 WhatAmI、GetCell、Building center、GameOptions速度和Tactical投影。仅在 IsShrouded 与最终 DrawSHP 边界提供受控结果／记录参数，没有替代动画或位置计算。

- 所有7档游戏速度，各跑两个周期；共1,278例原版 frame／中心／DrawSHP ABI 与 production C++ helper 对照，通过。
- Repair flag、IsSinking、IsShrouded 各真假交叉，三份各三建筑，共72例分支门控，通过。
- 每次原版 draw 均验证 WRENCH 全局资源、MOUSE转换器、flags0xE00、亮度1000、其余预期零参数，以及当前完整frame编号。
- 三个阶段的 WRENCH 均为未展开的 `WRENCH.SHP` 引用，而非捕获中已可用的模型bytes。没有把合成解码测试误写成该capture包含真实扳手像素。

数值与代码区间hash保存于 [native-repair-visuals-capture.json](../verification/native-repair-visuals-capture.json)。原版视觉gate验证的是相关slice，不是执行完整viewport绘制或所有状态图形。

[repair_visuals.cpp](../assist/tests/repair_visuals.cpp) 使用合成7帧SHP和RGB565色盘验证：开关／身份／失读／地址溢出／采样中ID变化、中心与速度公式、7姿态缓存、每tick色盘共享、内存到磁盘回退、失败后恢复、资源版本和色盘变化，以及最终图像覆盖任意world depth而不修改depth。测试未使用私有游戏美术。

```sh
g++ -std=c++17 -O2 source/assist/tests/repair_visuals.cpp -o /tmp/repair-visuals
/tmp/repair-visuals
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_repair_visuals_capture.py \
  /path/to/private-capture --output source/verification/native-repair-visuals-capture.json
```

## 未决点

等待使用者实机确认扳手与原版画面的实际位置、大小及遮挡效果。当前MOUSE转换器读取按已经验证的RGB565／1 shade实现；其他像素格式尚未获得原版样本。没有扩展停电标识、选择框、血条等其它DrawExtras图形，也不把维修扳手的证据外推为所有额外图形已复刻。
