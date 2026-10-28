# 指令线与建筑摆放红绿格：只读补齐和原生反馈保留

用户在 pre.6 修复测试包实机报告：渲染升级遮住原生命令线、建筑放置红绿格。之前只重绘选中框、血条、框选和软件光标，完整不透明场景自然遮住未重绘的原生 UI。

调查前检索研究索引、原生反馈笔记和持久 notebook v79 的 DrawActionLines、CurrentFoundation、放置预览关键词。没有把既有反馈增量的未完成清单当作功能已经支持。

## 证据身份与边界

| 授权引擎 | SHA-256 |
| --- | --- |
| sample3 | `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` |
| sample4 | `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` |

不发布私有 EXE、原始指令字节或内存 dump。受控执行中的坐标、色盘、选择集合和绘制终点为合成边界，不冒充实际 Windows 图像或完整 native 任务/放置判定。

## 指令线

原版 Tactical `0x6D4735..0x6D4756` 要求 source `+0x83 IsSelected` 与 `TechnoClass::ActionLinesEnabled @0x843108` 为真，再经虚表 `+0x438` 调 Foot `0x4DC060`。Building 对应槽为 `0x459E60`，是空实现。

Foot 路径只在 target `+0x2B4` 或 destination `+0x5A4` 非空时考虑绘制；非强制调用还检查全局 timer `0xB0EA80`（start）和 `0xB0EA88`（remaining duration）。target 优先于 move；move 有 NavQueue 时从 `+0x58C` 指针数组读取 `+0x598` count 的末项。最后调用 `DrawActionLineSegment @0x7049C0`，thiscall、36 栈字节。使用实际 `[0x87F6C4]` converter `+0x174` LUT，攻击索引 8、移动索引 3，并按实际 RGB565/RGB555 位布局解码，不猜固定红绿颜色。

新增 `feedback_orders.h::actionLines` 读取当前选中己方集合及上述状态，进行双读一致性、身份/Owner、容量和范围检查；按当前视口、当前物体位置生成线段，和选中反馈放在同一图层及原子组中。读失败沿用已有有界完整组宽限。

**功能性重绘的明确限制**：线起点使用当前物体位置，不完整重播 weapon FLH/枪口矩阵；目标 Cell 使用 Level/桥层投影，不完整复现每种 slope 高度插值；不宣称已覆盖 PlanningToken 编辑、全部模组特殊指令线或盟友/跨所属方控制显示。普通己方移动、攻击、最后导航目的和原生命令线开关/显示时限已接入。

## 建筑红绿格不应凭 membership 猜有效性

Display 是 Map 对象 `0x87F7E8`，不是指针槽。

| 偏移 | 真实状态 |
| --- | --- |
| `+0x1174/+0x1178/+0x117C` | 当前 foundation center、top-left offset、格子偏移数组 |
| `+0x1182/+0x1186/+0x118C` | copy center、offset、数组；center 实际按 2 字节对齐 |
| `+0x11A4/+0x11A8` | 当前建筑/类型 |
| `+0x1190/+0x1194` | copy 建筑/类型 |
| Cell `+0x12C & 0x02/0x04` | active/copy membership，**不是可放置位** |

数组以 `(0x7FFF,0x7FFF)` 结束，原生 active/copy 存储限制分别为 120/50 格。MarkFoundation `0x4A95A0` 使用 mask **0x02**，copy `0x4A9650` 使用 **0x04**；避免把十进制 mask 2/4 误记成 bit 索引再左移。

原版 `Cell::DrawPlacement @0x47EC90` 在内部实时计算合法性，涉及 `Cell::CanThisExistHere @0x47C620`、当前近邻、黑幕、WaterBound 和其他类型规则。Map `+0x1180` 也不表示完整每格有效性，最终 bool 只在栈上；未找到可靠已存每格 valid 数组。

因此实现 `FoundationStencil` 读取当前 active/copy 范围与 `[0x8A03FC]` 的真实 cellsel SHP：绿色 frame0、copy frame1、红色 `2+slope`，提取相关帧非零 alpha 的 union。用稀疏单行透明 cutout 让这些精确 native UI 像素透出，**让原生引擎继续决定红绿和 MOD 规则，不调用/重算合法性**。只在当前 foundation 的实际 shape mask 开孔，不退回整屏原图；selection/world/stats 层仍可在其上。相同 shape/frame alpha 缓存复用，身份或反馈隐藏重置时清缓存，没有每帧复制整张场景。

原版 anchor 为 `((cellX-cellY)*30-cameraX, (cellX+cellY-Level)*15-cameraY-1)`；Level 有符号。flags `0x20600/0x10600` 含 Centered，实际 `0x4AF002..0x4AF038` 减完整 canvas 两轴的一半，随后 `0x4AF064..0x4AF07C` 加 frame crop；与现有 `voxel::sprite` 的 x/y 定义一致。坡面改变 red frame 和 depth 参数，不另移动 anchor Y。不能以固定 60×30 推断全部 mask 边界。

该方案保留真实红绿提示，是有意的原生反馈兼容区域；不是将全部建筑预览/ghost body/模组 UI 独立重建。

## 验证

- 原指令 actions：两样本各 8 案，合计 **16 PASS**；覆盖未选中、选项关闭、移动、攻击、timer 过期/暂停、NavQueue 末项及空目标。FLH/端点 getter 是明确替身。
- 原指令 placement：sample3 **8 draw PASS**；sample4 copy/inactive **2 PASS**。sample4 active 的 **6 案 BLOCKED**，因为已有 `0x47ED25→0xB7E623` 磁盘/映射续行 32 字节全零；没有恢复别的样本指令。两样本另外合计 **6 Centered/crop PASS、8 Mark/Unmark PASS**。
- production `native_order_feedback.cpp`：实际 helper 的 timer/目标/颜色、身份与 torn read；active/copy 非矩形 SHP alpha union、缩放、相机位移、缓存和取消全部通过。没有伪造每格 valid。
- production `render_feedback_host.cpp` 旧光标/采样宽限回归继续通过。两 C++ 测试各自优化及 ASan/UBSan 通过，环境 leak sanitizer 关闭，不主张泄漏检查通过。
- `validate_native_feedback.py` 只读/接入契约通过。

可重跑脚本：[native_feedback_audit](../tools/native_feedback_audit/README.md)。精简证据：[render-fix2-native-feedback.json](evidence/render-fix2-native-feedback.json)。实机红绿提示位置、各平台显示模式和全部 MOD 原生反馈仍需用户新构建确认。
