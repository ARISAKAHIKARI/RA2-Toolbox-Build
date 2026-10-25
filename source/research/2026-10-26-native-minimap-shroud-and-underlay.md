# 1.0.38 小地图黑幕边缘与雷达底色

## 问题与证据

用户提供的两张画面分别显示：实景小地图的探索边缘为整格锯齿，与游戏中的平滑黑幕边缘错位；全图显示时，画布右侧和下侧残留黑色齿状空洞。起点为已发布 `v1.0.38-pre.12` 的源码。调查前查阅研究索引、持久 SQLite v48 中的黑幕策略、原尺寸合成、南缘和雷达 raw 边界记录。

支持的原版映像 SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。下述函数与字节来自原版 x86 反汇编及私人捕获 `RA2Master-Capture-20261025-003340-3568` 的只读观察；用户截图是实际反馈，但没有对应的同帧 Cell 数据或原生雷达完整像素。原始 EXE、内存和 SHP 字节不入库。

## 原版黑幕与雾

`Cell::DrawShroud @0x4801F0` 为 thiscall，ECX 为 Cell，栈上为 `{Point2D* drawPoint, Rectangle* clip}`，`ret 8`。它通过 `TacticalClass @0x6D8700` 取得逐格边缘帧：该函数以 ECX=`[0x887324]`、栈 `{CellStruct* (Cell+0x24), mode}`、`ret 8`，AL 为有符号帧索引。mode 0 的结果写入 Cell `+0x120`，mode 1 写入 `+0x121`。mode 0 使用邻格 Cell `+0x12C` 的 `0x18` 位与当前 `0x08` 位，mode 1 使用 `+0x140` 的 `0x03` 位与当前 `0x02` 位；`0x7F4194` 的 256 字节带符号查找表可返回 `-2`、`-1` 或正值 1～46。`-2` 映射 SHP 帧 15，`-1` 映射帧 0；其他索引直接作为帧。SHROUD.SHP 与 FOG.SHP 字符串位于 `0x81CE18/0x81CE10`，加载的 SHROUD 指针见 `0x89E794`，FOG 见 `0x89E790/0x89E760`。上述两个 Cell 帧字段是绘制时写入的缓存；地图未进入 Tactical 视窗时是否会由其他路径更新，仍待确认，因此工具箱按实时邻格位及游戏映像的查找表独立求帧。

`Cell::DrawShroud` 调用 `0x47EFE0` 和 `0x47F250`，均为 thiscall，ECX 为 Cell，栈 `{Point2D* drawPoint, Rectangle* clip, int frame}`，`ret 0x0C`。原版 SHROUD.SHP 捕获为 60×30、96 帧；帧 0 的菱形内 900 个像素值为 127（清晰），帧 15 菱形内 900 个像素值为 2（遮蔽），菱形外值 `0xFE` 为不写入哨兵。部分边缘帧包含 2～127 的过渡值。`0x47EFE0` 在哨兵外写本帧字节到 16 位 alpha；雾路径 `0x47F250` 跳过大于 127 的值，并按原生加减规则合成。FOG 绘制还受 `[0xA8B230]` 的选项字节 +1 位 `0x10` 与当前 House `+0x1F5` 控制。

地形 TMP 的原生高度偏移不可用作黑幕位置。`0x6D3660` 通过 `0x480A30/0x6D1F10` 投影 Cell 中心，`0x6D36F0/0x6D36FD` 将 SHP 左上角置于 `projectedX−cameraX+viewportX−30, projectedY−cameraY+viewportY−15`，不减 Cell Level。在工具箱的雷达原像素画布上，左上角对应 `((x−y+offsetX)*30−30, (x+y−offsetY)*15)`；TMP 自身仍按 Level 上下移动。

旧小地图及观察窗采样都仅把 `Cell+0x120 == -1` 当作可画地格，直接丢弃部分遮蔽帧的地形和边缘素材。画面因而跟随露出的 TMP 菱形轮廓，而不是原版屏幕空间的 SHP 黑幕。原版帧在地形和物件完整合成后应用，才可避免较高地块或建筑越过遮蔽边缘；半透明、透明、原生揭示与直接全图选项保持无小地图黑幕的既定策略。

只读捕获复核脚本 [native_shroud_capture.py](../assist/tests/native_shroud_capture.py) 按原版 `0x6D8700` 指令重构邻格选择器，并把期望帧和 Cell 缓存逐格比较；去标识结果见 [native-shroud-capture.json](../verification/native-shroud-capture.json)。三个阶段各有 37,008 个具备完整八邻格的样本；SHROUD 全部相符，FOG 的 `02-on` 与 `03-off` 各有 3 个边缘帧不同。该脚本没有执行原版选择器，缓存一致也不能证明画面外或以后每一帧都有及时更新，因此正式采样从实时邻格标志和游戏已加载的 256 字节表重算。越过有效 Cell 或读不到邻格时按遮蔽邻格处理；读不到查找表则不发布新画面。

## 右侧与下侧的底色

原版 `RadarClass::FillTerrainColors @0x654EA0..0x655041` 在 raw RGB 底图中，先让每个平面地格矩形 `[x−y+offsetX, x−y+offsetX+2) × [x+y−offsetY, x+y−offsetY+1)` 与 `[0,rawWidth) × [0,rawHeight)` 求交，再调用 `Cell::RadarColor @0x47C060` 写底色。`pre.12` 已按该准入裁切地格，但实景合成只绘 TMP 菱形；菱形之间及 raw 画布边端没有对应的平面底色，表现为黑色三角。独立私人 capture 的 raw 画布为 248×276，33,700 个可用且准入地格的最大 flatX=246、flatY=275，确实抵达 raw 右／下界。

修复为同一批已准入地格先在原像素画布填充 2×1 平面颜色，再绘 TMP 与动态物件，最后根据原生 SHP 坐标对目标像素的源区间采样，并在完整画面上应用独立的黑幕／雾帧。显式全图选项在 legal/raw 内而 live usable 外的玩法边缘只补雷达底色，不绘 TMP／覆盖图／物件；这使右下 raw 底图抵达边界，同时不把用户可玩范围往外扩。底色取自工具箱原有 `terrainColor` 静态调色，保留未暗化的 `baseColors` 避免 FOG 再次压暗；它复现原版准入与画布覆盖，而非逐像素解算 `Cell::RadarColor` 的颜色。这个顺序不移动已通过 360 组原版算术重放的 TMP 相位，也不把玩法 usable 边界误当成全帧雷达着色遮罩。

## 验证与未决

采样回归 [minimap_overlay.cpp](../assist/tests/minimap_overlay.cpp) 与 [radar_boundary.cpp](../assist/tests/radar_boundary.cpp) 穷举 256 种邻格位／双模式、旧缓存值与实时位矛盾、局部帧 4/46、Level2 下的独立黑幕锚点和失读收敛。边端底色的聚焦测试为 [radar_scene.cpp](../assist/tests/radar_scene.cpp)：合成 raw8×8、Level2 边格验证 TMP 止于 x210/y105 时底色抵达 x239/y119、不会覆盖已绘 TMP，缺失 tile 索引也有底色且不绘推测的菱形。[radar_shroud_mask.cpp](../assist/tests/radar_shroud_mask.cpp) 用合成 60×30 SHP 帧测试非对称锚点、Level2 与遮罩分离、原尺寸/缩小取样、FOG 算术、揭示模式、物件越界及素材失败关闭。生产渲染器对地形、单位、扳手等完成合成后应用独立黑幕，并缓存同一来源的遮罩。观察窗也通过共用 [radar_shroud_selector.h](../external_reader/source/radar_shroud_selector.h) 按实时邻格计算帧，[watch_shroud_mask.cpp](../assist/tests/watch_shroud_mask.cpp)、[watch_view.cpp](../assist/tests/watch_view.cpp) 与 [watch_host.cpp](../assist/tests/watch_host.cpp) 覆盖局部相机坐标、边缘格保留、旧缓存矛盾及动态物件的后合成遮挡。正式版 Windows 编译与 UI 门限由验证流程运行；原版黑幕函数与帧字节是反汇编和私人只读观察，尚无完整游戏逐像素对照；右下边界测试是合成画布，不能当作该用户画面的同帧 raw/Cell 对照。仍需在相同地图、相同黑幕选项和缩放下直接比较游戏雷达与新版小地图。
