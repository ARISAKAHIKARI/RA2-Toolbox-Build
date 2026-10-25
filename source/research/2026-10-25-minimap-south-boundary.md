# 实景小地图下边界：有符号高度、原生像素相位与输出范围

> pre.12 更正：下文以生成雷達尺寸／倍率反推的「精確跨度」不是原版 RGB 底圖的整數准入邊界。原版 `Map+0x1240/+0x1244` 直接保存 raw 寬高，`FillTerrainColors @0x654EA0` 先以未扣 Level 的平面 2×1 格子和 raw 矩形求交。這會排除原先被 Level 抬入南緣的格；顯式無黑幕時另限制揭露至 live usable 範圍。完整調查與測試見 [pre.12 南緣更正](2026-10-25-minimap-bottom-pixels-pre12.md)。pre.6 的 physical-legal 繪圖入口是 Tactical，不能代替 Radar 初始底色准入。

## 问题和证据范围

用户截图中，实景小地图水池附近的下边界露出交替三角形深色区域。调查基于 `v1.0.38-pre.8` 提交 `070f19e91cee76629cbaa4a45f66ee310da3e7b5` 的现行代码；先查阅研究 INDEX、持久 SQLite v44，以及 pre.5 地图边界、pre.6 可绘制边缘更正等条目。不能再把玩法 usable 区或相机限位当作地形绘图遮罩。

支持的原版映像 SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。新证据包括生产采样器/画布回归、私人 capture 原指令的坐标/高度算术重放和原版 TMP 行偏移表检查。没有执行完整原版地形绘图、Windows 呈现或多人联机；用户截图也不是此 capture 的同场景截图。

## 发现与处理

### 1. 高度字段存在明确的有符号错误

原版 `Cell::DrawIsoTile` 的 `0x480416` 使用 `movsx` 读取 Cell `+0x11B`，`0x480426..0x48042E` 计算并减去 `Level*15`。观察窗的 Tile.level 已为有符号值，但实景小地图 `radar::CellArt.level` 仍是 `uint8_t`，采样时直接保存原字节。若原值为 -1，旧实现把它当成 255，将地块错误向上抬高 3,825 像素，而原版应向下移动 15 像素。

现改为 `int8_t` 并在采样和示意图 terrainColor 输入处明确符号扩展。`radar_boundary.cpp` 在南部取景边缘构造 Level=-1 的合法地格，执行生产 `Sampler`，再按真实 TMP 菱形覆盖形状绘入画布，验证地块出现在底部，保留 900 像素菱形的正确矩形裁切。

这是一条已复现的代码错误；现有旧 capture 的南缘地格主要为 Level=2/6，没有用户新截图同场景的负高度数据，因此不能称它是该截图的唯一原因。

### 2. 原有半格相位正确，不应把整个地形再向下平移

原版 Tactical 先将整数格坐标转成世界坐标并投影，再在地形调用点减去 30 像素 X，最后由 DrawIsoTile 减去有符号高度。因此 TMP 左上角为：

```text
left = 30*(cellX-cellY)-30
top  = 15*(cellX+cellY-Level)
```

它等价于现有绘图使用格中心 `(x+.5,y+.5)`，配合 TMP 图像原点 `(-30,-15)`。不能为了盖住下沿三角形再给所有地块加一行或半格偏移。

新增 `mapbounds::Bounds::tileOrigin` 和 `radar::Geometry::tileOrigin` 共用这一原版相位，后者再加原生雷达平面 offset。helper 只接受物理合法格，不检查 usable；非 usable 但合法的边缘地格仍返回有效绘点。

| 原版地址/范围 | 本次验证 |
| --- | --- |
| `0x6D1F10..0x6D1F9B` | Tactical 世界坐标到屏幕 XY 的整数部分，在高度/FPU 分支前停止 |
| `0x6D51F6..0x6D5226` | 真正地形调用者减相机位置并将 X 减 30，在 CoordinatesLegal 调用前停止 |
| `0x480409..0x480430` | 原版读取绘点、符号扩展 Level，并减去 Level*15，在 GetImage 虚调用前停止 |
| `0x7EC450` 的 30 项偏移表 | 原版 TMP 29 个非空行；相邻差等于 `4*min(y+1,29-y)`，共 900 像素 |

`native_radar_boundary.py` 在三份 capture stage 各执行 120 组，合计 **360 组**，覆盖五个合法格、八个正/负/极值高度和三个相机平移，与真正编译的生产 helper 零差异。输出只含计数、范围 SHA 和结论，不含原指令字节、素材或原始记忆体；见 `verification/native-radar-boundary.json`。

### 3. backing 大小与实际投影范围需要分开

原生雷达的尺寸/倍率描述的是未减地格高度的平面取景范围：

```text
exactWidth  = radarWidth * 30 / factor
exactHeight = radarHeight * 15 / factor
```

整数 backing 用 ceil 分配，但最后一次呈现的采样范围应继续使用 exactWidth/exactHeight。例如旧 capture 的高度是约 4,140.000041 像素，ceil 后需要 4,141 行 backing；这不意味着源平面额外多了一整行背景。新增 `mapbounds::NativeViewport`/`Geometry::nativeViewport()` 同时提供精确跨度和 backing 尺寸，交给原始尺寸合成/统一缩放流程使用。

源坐标和原生 Geometry 的正/逆变换仍相同，不改可导航范围、相机限制或当前 VisibleRect。画布裁切只裁实际取景矩形；不会依据所有列里最浅的一条南缘统一截掉其他列的合法地形或单位。

### 4. 真实地表外缘可以不齐

在既有 capture 的有效合法地格数据中，仅按有符号高度与原版 900 像素名义菱形轮廓估算，南缘不同列的投影底部不在同一行；部分列可位于雷达平面取景底线上方，其他列仍延伸更远。这个诊断不包含 TMP extra、颜色索引透明度、建筑/单位，也不是完整原版 rasterizer。

因此交替外缘本身不能证明地块准入或半格相位有误；也不能通过延伸非法格、复制最后一排图块、使用 usable 遮罩或按最浅列全局裁切来填平。当前修复保留真实外缘，由原始像素合成后统一滤波处理缩小时的采样边缘。该统一合成/滤波集成由同版渲染实现记录；本条不把名义轮廓当作像素遮罩。

## 代码和验证

修改 `external_reader/source/map_bounds.h`、`radar_map.h`；新增 `assist/tests/radar_boundary.cpp`、`native_radar_boundary.py` 和对应 sanitized JSON。

已运行通过：

- `radar_boundary.cpp`：负高度南缘像素、物理合法但 non-usable 边缘保留、非法格拒绝、原生 TMP 相位/900 像素轮廓、精确跨度与 ceil backing、不同高度不修改导航 domain。
- `minimap_overlay.cpp`：原有投影/导航、实时缩扩界、原子地形 bank、黑幕策略和 13,824 种布局组合。
- `map_bounds.cpp`：物理合法性/usable 分离、奇偶相机视口、小图中点、失读/混合矩形拒绝。
- `art_variants.cpp`：现行采样器的资源池、坡面、TMP 变体和损坏回退。
- `native_radar_boundary.py`：前述 360 组原版算术重放和三份原版 TMP 行偏移表。

```sh
g++ -std=c++17 -O2 source/assist/tests/radar_boundary.cpp -o /tmp/radar-boundary && /tmp/radar-boundary
PYTHONPATH=/path/to/native-deps:source/assist/tests python3 source/assist/tests/native_radar_boundary.py /path/to/private-capture --output /tmp/native-radar-boundary.json
```

未确认用户新截图每一个水池边缘像素的实际高度、TMP extra 和资源是否已就绪；完整 Windows 实景和各种 MOD 的画面仍需实机验收。没有把该部分的不确定性写成必须扩大裁切的理由。
