# 覆盖图不透明度与 VXL 原生坡面姿态

## 范围与依据

用户实机反馈矿石及多种覆盖图呈半透明，并补充 VXL 缺少坡面角度。基线为 pre.6 Fix1，远端源码 `d8c7633cfb07a070388d8f018656f41bcc2c8bcc`，本地同产品修改提交 `c277eb99913ada9f6c58f440154a9aaf1f66be87`。调查前检索研究索引、资源池、矿柱色盘、VXL 光照/投影笔记与持久 SQLite v79。

不透明度问题由宿主源码与生产资产/合成夹具证实，不需要新增引擎地址。坡面行为另以固定 YRpp `b22f1860dd7b100cad57123d4ca5d459b9020d88` 交叉检查，并执行授权映像的原生函数：

- 样本 3 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。
- 样本 4 SHA-256：`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`。

不发布原 EXE、内存或素材；原函数重播使用合成对象、类型、ramp matrix/quaternion 表。它不等于完整游戏或实机画面验收。

## 共享覆盖图误用了半透明预览默认值

`cameraassets::Context::overlay()` 的磁盘 SHP 与内存 SHP 两条调用都漏传 alpha，继承 `voxel::sprite()` 的默认值 **124**。所有非零索引因此只留下约 48.6% 不透明度。矿石是静态 ground overlay，直接进入地形背景，没有动态 actor 路径的 `camera::opaque` 补偿。普通覆盖图也会经过同一入口，所以不能只按资源标志给矿石单独修正。

此前亮度笔记确认的 `renderActor(alpha=255)` 与动态 `camera::opaque` 结论仍成立，但不能扩展为所有静态覆盖图也已不透明。旧资产测试甚至以 `premultiply(...,1)` 的同一默认 124 作为预期，因而没有揭示这一漏项。

修正共享 `overlay()` 入口为明确 alpha=255；索引零仍保持透明孔。没有改变通用 decoder 的默认值或队形预览的透明度。直接以原始 palette RGB 解码，避免先 124 预乘再反除造成颜色损失。例如灰色 88 在旧图像中预乘为 42；恢复为 opaque 也只能得到 86，而直接解码仍是 88。

`game_assets_host_test.py` 新增资源/普通覆盖图 × 归档/内存 SHP 四种生产路径。每一个实心像素在黑/白背景上都必须精确等于其原 palette RGB，透明孔必须保留各自背景；另断言刻意使用默认值的预览仍为 alpha124。既有矿柱、动态色盘、动画、工厂部件、弹体等整套资产回归通过。隔离旧基线、直接运行同一新版不透明度测试（加入后续姿态用例之前的测试版本），在第一项 opaque 资源像素断言失败，没有修改夹具来迁就旧实现。负控报告保留该测试版本 SHA，避免与后来扩展了姿态用例的脚本混为一谈。

## VXL 坡面来自移动器绘图状态，不是 Cell Level

旧 `visual::Pose` 仅采集水平朝向与 SHP 帧；车辆传入 `voxel::render()` 的 pitch 恒为零。只有弹体拥有俯仰角。因此已有 VXL/HVA 几何、现场光照和高分栅格本身不能补出车辆的坡面姿态。

| 原生字段/入口 | 已验证用途 |
| --- | --- |
| Foot `+0x674` | `ILocomotion*`；不是 LocomotionClass 主接口指针 |
| Drive ILocomotion vtable `0x7E7EB0`，槽 `+0x24` | `Draw_Matrix @0x4AFF60`，stdcall，stack 为 interface/out matrix/index，`ret 0xC` |
| Ship ILocomotion vtable `0x7F2D8C`，槽 `+0x24` | `Draw_Matrix @0x69F670`，相同调用约定和布局 |
| ILocomotion `+8` | LinkedTo 对象，必须与被绘制车辆一致 |
| ILocomotion `+0x18/+0x1C` | 当前/之前 ramp index |
| ILocomotion `+0x20/+0x28/+0x2C` | SlopeTimer 起始帧、剩余时长字段、rate；原函数据当前逻辑帧计算过渡比例 |
| `0xB45188` | 21 个实际 live ramp Matrix3D，48 bytes/项 |
| `0xB43188` | 同一 ramp 的 quaternion，16 bytes/项 |
| `0x7559B0 / 0x755A40` | 复制当前 ramp matrix / previous→current quaternion Slerp 并生成 matrix |
| Techno `+0x328/+0x32C` | 当前 sideways/forwards rocking，单位为弧度 |
| Type `+0x360/+0x368` | 原函数计算 rocking 支点平移所用的 double scale，先转换为 float |

`Techno::PitchAngle` 不是该坡面来源；固定 YRpp 明确它是 dropship 内部状态且不影响绘制，不能凭名称直接套用。

原函数组合为 `Tz(height) * Ramp * Yaw * Rock`。Rock 包含局部侧/前倾及支点 X/Y 位移，不是围绕任意模型中心转动。坡面过渡使用原逻辑帧的 SlopeTimer；没有改变游戏时钟或写回姿态。新增 `voxel_posture.h` 读取已确认的 Drive/Ship 状态和 live 表，重建同样的数学步骤；角度/asin 查表按数学公式重建，不带出游戏原表字节。

模型在 HVA 后已镜像 native Y，故世界姿态以 `S * NativeWorld * S` 作用于当前 preview 点。Rock 依车体朝向共轭为世界变换，使独立转向的炮塔及其车体相对支点也随坡面倾斜。VPL 的原始 normal index 保留；新的 world matrix 进入光源的转置变换，未以 HVA 变形法线代替原始索引。

Pose 保存这份观测姿态；既有 VXL/HVA source model 保持缓存，修改的是本轮顶点/法线栅格输入，不因爬坡重新解码 VXL/HVA。`voxelposture::signature` 包括 matrix、rocking、scale 和 available 标志，供呈现图像缓存区分同朝向的不同坡面；source geometry 的身份不含姿态。

## 验证与限制

- `voxel_posture.cpp` 运行 production 采样与顶点投影：Drive/Ship、平地/高度不等于坡面、转换中点、双轴 rocking、颜色不透明、模型内存保留、失读/字段变化/坏表拒绝和未知 locomotor 隔离。优化版及 ASan+UBSan 通过；受环境限制关闭 LSan，不声称验证泄漏。
- `voxel_posture_native.py` 对每份授权 EXE 执行完整 Drive/Ship `Draw_Matrix`、实际 timer、Slerp 和矩阵函数，仅替身化 `GetTechnoType` 边界。公开数值/代码区段哈希证据见 [voxel-posture-native.json](evidence/voxel-posture-native.json)。每份 1,344 例，合计 **2,688 例**；与实际编译的生产 helper 最大矩阵系数误差 **1.196×10⁻⁷**。输入覆盖 21 个索引、方向、过渡开始/中点/结束、steady、rocking 和 scale；表值为公开可生成的合成旋转，不把它说成 21 种原版实际坡面的资产重播。
- 既有 `voxel_preview.cpp` 的原生非对称 HVA/body/turret/barrel anchors 通过。
- 整套 `game_assets_host_test.py` 新增 production Context 接收 live Drive Pose→输出不同坡面像素；模型 faces 指针、数量及磁盘读取次数不变，整套通过。

可复现：

```sh
g++ -std=c++17 -O2 source/assist/tests/voxel_posture.cpp -o /tmp/voxel-posture
/tmp/voxel-posture
python3 source/assist/tests/voxel_posture_native.py /path/to/authorized-game.exe \
  --portable /tmp/voxel-posture --output /tmp/voxel-posture-native.json
python3 source/assist/tests/game_assets_host_test.py
```

边界：未知/其他 locomotor 不猜布局，保持既有路径；当前未完整复刻 Hover/Fly/Jumpjet/Tunnel 等移动器的专用矩阵。原生 `Shadow_Matrix` 尚未认证，本次不以完整坡面阴影为完成项。采样一致性沿用 ReadSession page epoch 与场景前后检查；双读在直接 fresh Reader 下可拒绝变化，但不能把缓存页内的重复读取说成跨进程完全原子快照。未声称实机帧率、全部 MOD 姿态、Windows/DWM 画面或联机已验收。

固定一手参考：[Drive](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/DriveLocomotionClass.h)、[Ship](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/ShipLocomotionClass.h)、[Foot](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/FootClass.h)、[Techno](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/TechnoClass.h)、[Matrix3D](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/Matrix3D.h)。
