# 原版抛射体视觉：独立绘制、帧、双轴朝向、色盘和阴影

## 问题与证据边界

用户要求副相机补齐抛射体画面。本轮先查 AGENTS、研究 INDEX 和持久 SQLite 中已有弹体方向笔记。此前已有 `Velocity +E8/+F0` 用于躲弹的证据，但没有 Bullet 绘制来源、色盘或 VXL 俯仰证据，不能把单位图像读取直接套到弹体。

支援原版映像 SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`，image base `0x400000`。

输入是私人 `RA2Master-Capture-20261025-003340-3568` 的 `01-off`、`02-on`、`03-off`。三份 **BulletClass::Array 均为空**，本轮没有捕获中真实飞行弹体的画面验收。探针使用合成 Bullet 布局及复制的已解析 BulletType，执行原版 x86，保留原数学、MapClass 普通地面查询和 virtual GetImage/GetAnimFrame/GetHeight/GetZ，截停最终 SHP／VXL 绘制边界。场景的遮蔽标志作为可控输入关闭，不能把此探针写成全部黑幕条件、真实弹道或完整 rasterizer 已验证。

公开文件只有探针、计数、选择和代码 hash，不含 EXE、dump、模型、色盘或 framebuffer。

## 入口与已确认字段

| 地址／字段 | 原版证据与含义 |
| --- | --- |
| `0xA8ED40` | BulletClass 独立 DynamicVector；不在 TechnoClass 列表中 |
| `0x7E46E4` | Bullet 主虚表，构造器 `0x466425` 写入 |
| vtable `+0x114` → `0x468090` | Bullet::Draw_It，thiscall，stack `{Point2D*, Rectangle*}`，`ret 8` |
| vtable `+0x1E8` → `0x468000` | GetAnimFrame，thiscall，无 stack 参数 |
| Bullet `+0xAC` | BulletType 指针；virtual GetType `0x46B5B0` 返回此字段 |
| Bullet `+0xB0` | 发射者 TechnoClass 指针；不同于单位 Type 字段 |
| Bullet `+0x6C` | **伤害值，不是存活 HP**。Construct `0x4664F6` 把 damage 参数写入此处 |
| Bullet `+0x9C/+0xA0/+0xA4` | 世界坐标 x/y/z |
| Bullet `+0xE8/+0xF0/+0xF8` | double 速度 x/y/z |
| Bullet `+0x114` | InheritedColor 色盘编号；不是 Techno Draw_It 虚表或正常单位字段 |
| Bullet `+0x12C` | 当前运行时动画帧，BYTE |
| Bullet `+0x158` | SpawnNextAnim；为非零时原 Bullet 绘制被抑制 |
| Type `+0xA4` | 已解析 SHP 指针；Object::GetImage `0x5F3E30` → Type virtual `+0x9C` / `0x41CFA0` 返回它 |
| Type `+0xB0/+0xB4` | VXL / HVA pair |
| Type `+0x236` | Voxel 分支标志 |
| Type `+0x29A/+0x29E/+0x2A1` | Shadow / Inviso / NoRotate |
| Type `+0x2A8/+0x2A9` | AnimPalette / FirersPalette |
| Type `+0x2D4` | VXL **ColorScheme 编号**；不是指针 |
| Type `+0x2F4/+0x2F5/+0x2F6/+0x2F7` | AnimLow / AnimHigh / AnimRate / Flat |

### 实际层级不等于 CachedLayer

Bullet 的 InWhichLayer 是 `0x468B90`，原指令只读 `Type+0x2F7`，返回 `Flat ? 1 : 3`；它不读取 Object `+0x94` 的缓存层。对 `+0x94` 取0/1/2/3/4/FFFFFFFF/7FFFFFFF，各交叉Flat字节0..255，三份capture共5,376例均一致。因此外部弹体不得因 `+0x94` 不在0..4就丢弃，也不得直接采用该旧缓存字段作为绘制层。

公开 [YRpp BulletClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BulletClass.h) 和 [BulletTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BulletTypeClass.h) 用于符号交叉核对；其中 `BulletType::Color` 的 pointer 声明与支援映像实际整数索引指令不符，以原版 `0x46B1EC..0x46B212` 为准。

### 可见性与存活不能照搬单位

Draw_It `0x4680E2..0x46810C` 先拒绝 Type.Inviso 和 Bullet.SpawnNextAnim，然后按 Voxel 分支。它不把 `+0x6C <= 0` 判作不可绘制；探针对负伤害、零伤害、正伤害，以及 `+0x74/+0x90` 两个基础状态字节的组合执行了36例，均仍可到达绘制边界。后两字节不是这个函数自身的 gate，不能据此声称所有离图对象应该绘制。

外部读取仍需独立校验列表、对象虚表、ID、位置、InLimbo 和生命周期一致性，但**不得使用 GameView::live 的 Health>0 条件过滤弹体**。Object::DrawIfVisible `0x5F4B10` 的渲染入口另有 dirty / InLimbo / 世界可视范围处理，本轮没有复刻完整入口策略。

## SHP 帧：速度朝向与动画帧是互斥来源

`0x468000..0x468088`：

1. 若 NoRotate=true，初始帧为0。
2. 否则取 `atan2(-vy,vx)`，减 `pi/2`，乘 `-65536/tau`，原 `_ftol` 朝零截断，再按下式转32方向。
3. 用 `ObjectClass::BodyShape` 的32项表 `0x7F4890` 得到 SHP 帧。该表已检查等于 `(28-direction)&31`。
4. 只要 AnimLow 或 AnimHigh 任一非零，直接改用 Bullet `+0x12C` 当前帧，不再使用朝向帧。不在外部按墙钟另推一套动画。

```text
q = trunc((atan2(-vy,vx) - pi/2) * (-65536/tau))
d = ((((uint16(q) >> 10) + 1) >> 1) & 31)
frame = BodyShape[d]
```

原函数覆盖 NoRotate、AnimLow、AnimHigh、运行时帧0/11/255和32个水平朝向，共2,304例（三份 capture 总计）。Production 应拒绝非有限速度或不可读的字段，而不是从失读值猜朝向。

## SHP 色盘、亮度、阴影和深度

主绘制选择 `0x468379..0x46841D`：

| 条件 | converter |
| --- | --- |
| AnimPalette=true | `[0x87F6C0]`，优先级高于 FirersPalette |
| 否则 FirersPalette=true 且 InheritedColor != -1 | 全局 ColorScheme 数组的该编号 `+0x30C` |
| 否则 FirersPalette=true 且 InheritedColor == -1 | 当前玩家 House `+0x16054` 的 ColorScheme converter |
| 其余 | `[0x87F6C4]` |

原版启动 `0x52BE63..0x52BF12` 读取 `ANIM.PAL`，`0x52BF1A..0x52BFC6` 读取 `PALETTE.PAL`；两条都将6位通道乘4，构造53色阶 **plain ConvertClass**，分别存入 `87F6C0`、`87F6C4`。该启动来源是本轮反汇编证据，没有重新执行真实文件加载。捕获中它们的虚表是 `0x7E5358`。它们没有 LightConvert 的 `+0x188` 原始色盘指针，读取该处会越过188字节对象，误把相邻值当指针。

原版主 SHP 绘制为 `flags=0x2E00`、brightness=1000、输入屏幕位置，Z参数为 `-30-screenHeight(worldZ)`。Bullet.Bright 字节没有改变此处固定1000。

[YRpp Surface.h 的 CC_Draw_Shape](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Surface.h) 对应的stack参数从0编号：`args[5]=Remap`、`args[6]=ZAdjust`、`args[7]=ZGradientDescIndex`、`args[8]=Brightness`。本体与阴影的Remap及ZGradient均固定0，探针逐例断言并保存至JSON。[ZGradient枚举](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GeneralDefinitions.h) 的0为Ground；不能将args[5]误认梯度，也不能依据Type.Flat切换SHP本体为standing梯度。沿现有compositor Ground近似使用固定ground深度模式；完整逐像素梯度尚未重新验证。

Type.Shadow=true 且 GetHeight()>0 时，`0x468308..0x468374` 先绘制阴影：

- **同一 SHP、同一当前帧**，不是 `frame + FrameCount/2`。
- converter 固定 `[0x87F6C4]`，flags=`0x2601`，brightness=1000。
- 屏幕 y 加 `screenHeight(距地高度)`；Z参数为 `-10-screenHeight(地面高度)`。

合成位置 worldZ416 / 原版查得 ground208、输入 `(100,100)` 时，阴影位置 `(100,130)`、Z=-40，随后本体位置 `(100,100)`、Z=-90。色盘、FirersPalette、InheritedColor、Shadow、Bright 共144组原版绘制调用边界验证。

这里的 flags/Z 是原版绘制参数，不等于外部 renderer 可以忽略每像素深度、直接画最终覆盖层。阴影本体可由同帧不透明遮罩派生，但应保持相应地面深度。

## VXL：必须同时考虑 yaw、pitch 和另一套法线着色

### 矩阵和 HVA 帧

Voxel 分支 `0x468112..0x46824A` 使用速度进行两次定点32方向量化：

```text
yaw   = (direction(vx,vy)-8) * (-tau/32)
pitch = (direction(hypot(vx,vy),-vz)-8) * (+tau/32)
```

注意 pitch 所用 atan2 等价于 `atan2(vz,hypot(vx,vy))`。实际调用 `0x5AF1A0` RotateZ(yaw) 再 `0x5AF080` RotateY(pitch)，得到行主序、列向量矩阵 `Rz(yaw)*Ry(pitch)`；然后乘原版投影矩阵 `0xB44318`。

`0x46B0C0` 是 thiscall，7个 stack 参数，`ret 0x1C`。调用参数为 `{Type+0xB0, drawMatrix, point, rect, HVAframe=0, flags=0x800, brightness=1000}`。内部确实用第5项做 HVA 帧索引，所以这里固定0，不随 Bullet.AnimFrame 推进 HVA。

现有 `capturePart` 已在本地 HVA 之后镜像Y。它的 preview点应先做 `Ry(pitch)`，再 `Rz(-yaw)`：

```text
q = (cos(pitch)*x + sin(pitch)*z, y, -sin(pitch)*x + cos(pitch)*z)
previewWorld = Rz(-yaw) * q
```

原生32个水平朝向×5个竖直速度×3份capture，共480组，检查实际传入 yaw/pitch、原始旋转矩阵和镜像前后四个测试点。不是凭朝向直觉推断的符号。

### VPL 法线页与单位不同

Bullet VXL wrapper调用 **`0x753C80`**，它经转置的完整 drawMatrix 变换 view light `0x887470`，在 `0x753CEE` 调 **`0x758670`** 计算 normal→VPL page：

```text
dot = normal · localLight
page = dot < 0 ? 0 : uint8(trunc(dot * 16))
normal index 253/254/255 -> page16
```

**没有车辆 `0x753D00` → `0x7586F0` 的 specular / halfway 项。** 因此直接复用现有车辆 `nativeVoxelPage` 会产生错误的炮弹亮面。探针用已捕获普通载具 library 提供有效 mode4 normal布局，但执行的是原 Bullet setup `753C80` 和纯diffuse `758670`；三份共480次setup、119,040个normal页检查。该部分证明的是原生索引着色，不是捕获到了某个实际 VXL 炮弹的画面。

探针还动态编译当前production `voxel_lighting.h`，将相同32水平朝向×5竖直速度送入 `nativeVoxelLight(previewYaw,pitch)` 和 `nativeProjectilePage`，对每个有效mode4 normal及253/254/255，与上述原指令结果逐项比较，同样119,040项、零差异。原始矩阵与四点映射也继续通过。这只覆盖mode4，未把其他法线模式列为已执行。

### VXL 色盘与 Flat

`0x46B1EC..0x46B212` 以 Type `+0x2D4` 作为 **ColorScheme编号**，到全局scheme数组取converter。它不跟随 Unit owner / CustomPalette，也不使用 SHP 的 AnimPalette/FirersPalette 分支。

Type.Flat `+0x2F7` 为真时，selector flags从 `0x800` 变 `0x2800`，mode从2变0，并带全局remap值；brightness保持1000。探针对3个scheme编号×2个Flat状态×3份capture执行原 `0x46B18C..0x46B21F` selector片段，共18例。此项验证的是参数选择，尚未声称外部Flat逐像素深度语义已完全等于原版。

### Z测试与Z写入另行核实

仅根据flags名称推断不足，本轮实际调用原版selector和单像素blitter，保持候选Z=100，分别令现有Z buffer为0/99/100/101/65535：

| 原绘制分支 | selector / pixel函数 | 实测Z行为 |
| --- | --- | --- |
| 非Flat VXL，flags800 | `490B90` → `493DF0` | 五种Z都绘制；不测试Z、不写Z |
| Flat VXL，flags2800 | `490B90` → `494B60` | 仅现有Z>100绘制；严格只读Z、不写Z |
| SHP，flags2E00 | `490E50` → `497FD0` | 仅现有Z>100绘制；严格只读Z、不写Z |

三份共45个实际像素绘制调用。每次同时检查Z buffer原样保留。因而非Flat弹体的外部image必须关闭Z-test，而不是只把writeDepth关掉；现有Surface用NaN逐像素深度支持no-Z路径。Flat VXL／SHP保留严格Z-test且不写入深度。此检查未覆盖所有SHP压缩形式或完整场景遮挡。

## 尾迹、激光和束不是额外画一个 Bullet

已有 [YRpp BulletTypeClass](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BulletTypeClass.h) 的 Trailer 是 AnimType，实际出现的 AnimClass 应由独立动画集合渲染；Inviso弹体不能因此被画成可见弹丸。本轮没有验证尾迹生成时序。

其他视觉有各自集合和生命周期：

- [EBolt::Array](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/EBolt.h) `0x8A0E88`；电弧的公开源码说明和字段不能当作SHP弹体。
- [RadBeam::Array](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/RadBeam.h) `0xB04A60`。
- 原 LaserDraw 构造 `0x54FF50..0x54FFA1` 把this追加到 DynamicVector `0xABC878`，Items=`ABC87C`、Count=`ABC888`。这是构造注册反汇编证据，尚未受控执行完整光束绘制。

因此实现若只接Bullet SHP/VXL，应清楚限定范围；不能宣称仅凭这个集合就覆盖了所有激光、辐射束、电弧或尾迹。

## 测试与复现

新增 [native_projectile_capture.py](../assist/tests/native_projectile_capture.py) 和 [sanitized结果](../verification/native-projectile-capture.json)。本轮不改游戏逻辑、不安装Hook、不写同步状态，也不发布私人输入。

```sh
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_projectile_capture.py \
  /path/to/private-capture --output source/verification/native-projectile-capture.json
```

尚未验证的部分包括当前用户游戏画面的实际弹体 inventory、Windows合成、完整多物体遮挡、VXL所有法线模式、场景黑幕入口、光束完整绘制及联机。原版受控执行不能替代这些实机验收。
