# 民房进驻后的建筑外观帧

## 问题、基线与证据范围

用户回报民房驻军后，观察窗仍显示未进驻外观。本次基线是 pre.8 提交 `070f19e91cee76629cbaa4a45f66ee310da3e7b5`。调查前查阅研究 INDEX、同一 SQLite 的建筑独立部件、GetCurrentFrame、动画及驻军空间宿主记录。这里处理建筑外观；不改变入驻单位的跟踪身份、摄像机或游戏指令。

支持的游戏 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。只读私人 `RA2Master-Capture-20261025-003340-3568` 的 `01-off`、`02-on`、`03-off`。三份捕获均有未驻军的 CAFARM01（人数 0、Health 400、TechLevel -1）；**没有捕获到真实驻军后的连续画面**。以下驻军人数及血量组合是合成字段配合原指令执行，不冒充实机进驻验收。原始 EXE、内存、SHP 和色盘不进入仓库。

## 原因：漏了 GetCurrentFrame 的独立驻军分支

原版 `Building::GetCurrentFrame 0x43EF90` 为 thiscall，ECX=Building，返回 EAX，无栈参数。代码在 `0x43F02C` 测试 type 的 `CanBeOccupied +0x157B`，之后通过 virtual `+0x408` 取当前驻军人数。Building 的此虚函数是 `0x4581F0`：直接返回 object `+0x694`。

旧 `Context::buildingFrame` 只有电力／存储显示、建造／售卖反向及普通受损分支；对已经完成的建筑仅返回 `Stage + damage`，完全遗漏 CanBeOccupied 分支。因此即使已读到建筑及驻军内容，外观仍停在普通空房图。

| 地址／字段 | 已确认用途 |
| --- | --- |
| Building `+0x520` | BuildingType 指针 |
| Building `+0x534` | BState；0 走原有建造／售卖帧路径 |
| BuildingType `+0x157B` | CanBeOccupied；非零启用独立驻军帧分支 |
| Building virtual `+0x408` / `0x4581F0` | 返回 Building `+0x694` 的当前驻军人数 |
| Building `+0x6C` / type `+0xA0` | 当前血量／Strength |
| Building virtual `+0x84` | 经 `0x6F3270` 转发到实际类型 getter |
| Type `+0x634` | TechLevel |
| `[0x8871E0] +0x1700` | double ConditionYellow；捕获值 0.5 |
| `[0x8871E0] +0x1708` | double ConditionRed；捕获值 0.25 |
| `0x5F5C60` | 原版 health/strength x87 比值；本轮重播实际执行 |

在非零 BState 且 CanBeOccupied 的路径中：

```text
frame = occupants > 0 ? 2 : 0
if health/strength <= ConditionRed:
    frame += 1
else if TechLevel > 0 and health/strength <= ConditionYellow:
    frame += 1
if TechLevel == -1 and frame == 3:
    frame = 1
```

等号属于受损分支。民房 TechLevel=-1 在 Yellow 与 Red 之间仍使用正常驻军帧 2；不能把普通建筑的 ConditionYellow 直接外推。苏军碉堡等可建造驻军建筑 TechLevel>0 可使用单独的受损驻军帧 3。

| 类型／状态 | 空房 | 有驻军 |
| --- | ---: | ---: |
| 民房，血量高于 ConditionRed | 0 | 2 |
| 民房，血量不高于 ConditionRed | 1 | 1 |
| 可建造驻军建筑，血量高于 ConditionYellow 且 ConditionRed | 0 | 2 |
| 可建造驻军建筑，满足上方任一受损条件 | 1 | 3 |

`0x4513D0 Building::GetImage` 仍只根据 BState／ActuallyPlacedOnMap 选择 body 或 make；驻军不换另一张 SHP。原来的 owner/custom palette 路径与主图半帧上限继续适用。OccupyHeight 是其他占用／开火位置配置，不能拿它猜外观帧或改变建筑图片位置。

## 修复

* `building_components.h::occupiedFrame` 实现上述原版帧选择，显式读取人数、Health、Strength 和 TechLevel。字段失读或不合法时返回 -1，使当前姿态无法冒充已知空房；不修改游戏内存。
* `camera_assets.h::buildingFrame` 在原有建造／售卖路径之后、普通受损路径之前接入 CanBeOccupied 分支。原有特殊显示字段与 make 状态优先级保持。
* 新增 live `ConditionRed +0x1708` 读取；不可用时从有效 Rules INI 读取 `[AudioVisual] ConditionRed`，支持百分数，缺失／无效时用原版常见缺省 0.25。ConditionYellow 沿用现有实现。
* 不新建 AnimClass、不人工播放驻军动画、不改 body 素材、色盘和相机高度。两个渲染器原有 `buildingPose` 调用及包含 `pose.frame` 的图像缓存键，直接接收每轮真实的 0/1/2/3 变化；同一份源素材可以复用。

公开字段名称交叉来源：[YRpp BuildingTypeClass](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingTypeClass.h)。偏移、条件及帧值以本轮原始指令和受控执行为证据，不仅依赖该头文件。

## 验证

`assist/tests/garrison_frame_native.py` 对三份捕获逐一执行完整 `0x43EF90`，包含原有 virtual occupant-count/type getter 和 `0x5F5C60`，只在对象／类型与 Rules 字段输入合成值。人数 0/1/3/-1、十种血量、Strength 100/200、TechLevel -1/0/1/10、三组 Red/Yellow（包括非缺省及倒序阈值）、Stage 0/7，共 **5,760 例**，与编译后 production helper **零差异**。另执行 `0x4513D0` 的 body/make 选择入口，空房／驻军共 9 例全部仍分派到普通 body。输出 `verification/garrison-frame-native.json` 仅保存数量、非敏感配置与原指令区段 SHA。

`assist/tests/garrison_frame.cpp` 覆盖进入／撤出、民房 Red 和可建造建筑 Yellow 的等号边界、独立阈值变化、非法 Strength、字段逐项失读，全部通过。

`assist/tests/game_assets_host_test.py` 直接运行 production `captureActor`、`buildingPose`、`renderActor` 和真实 SHP decoder：只捕获一次有逐帧不同索引的公开合成房屋 SHP，驻军人数 0→1→3→0 时检查最终像素索引按 frame 0→2→2→0 改变；检查民房受损 frame 1、可建造建筑受损 frame 3，以及 live ConditionRed 从 0.25→0.4→0.25 的变化。源文件名、源素材及 palette 保持同一份。原有建筑部件与资源管线回归通过。

同一新增 host fixture 在隔离临时源码副本中移除本次驻军分支，恢复 pre.8 的 frame 行为，会在 `civicPose.frame==(count?2:0)` 断言失败；修复后通过。没有临时改写共享生产文件或削弱旧测试。

`building_components.cpp` 的原有 Bib、工厂正常／屋顶生产、门计时和受损边界也通过。

```sh
g++ -std=c++17 -O2 source/assist/tests/garrison_frame.cpp -o /tmp/garrison-frame
/tmp/garrison-frame
python3 source/assist/tests/game_assets_host_test.py
PYTHONPATH=/path/to/native-deps:source/assist/tests python3 source/assist/tests/garrison_frame_native.py /path/to/private-capture --output /tmp/garrison-frame-native.json
```

这证明所列原版帧选择与当前生产合成管线修复；尚不代表真实 Windows 民房进驻录屏、所有 MOD 的驻军扩展或全场景像素一致。未调整跟踪身份、黑幕、地形变体、光照或事件队列。
