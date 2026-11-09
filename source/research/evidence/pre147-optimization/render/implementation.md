# 渲染、地形存储和空闲飞机显示修复交接

基线：1.0.46.4。代码已在共享工作树落地，未提交、未改版本，后续发布由根代理统一处理为 1.0.47。此文件是本轮实现和受控验证证据，不代表 Windows 游戏实测或发布验收。

## 结果

| 项目 | 修复前受控结果 | 当前结果 |
|---|---|---|
| VXL 部件失败后恢复 | 151 检查中 69 失败；8 场景累计 4,704 像素不同 | 151 检查全部通过；同一 renderer 恢复后 0 像素差；真实 voxel 标记、恢复间隔 20ms |
| 健康 VXL 输出 | digest `8f320cbad81486c3` | 相同 digest |
| 暂停空闲飞机 | 旧代码只允许 500ms 样本显示 | 精确确认的同 match/frame/serial 可持续显示；时间戳不刷新；命令仍限 500ms |
| 补读失败后的重复扫描 | 8192 槽受控表：首次 294,898 次封存读取，下一轮再 294,898 次 | 首次相同，下一轮 0 次；新来源或完整 tactical sample 可再尝试 |
| Terrain 新 publication 分配，池已热 | 15,345,816 字节 / 22 次 / 4 个大分配 | 141,288 字节 / 18 次 / 0 个大分配，分配字节减少 99.0793% |
| Terrain 同 publication 第二消费者 | 0 字节 / 0 次 | 保持 0 字节 / 0 次 |

以上计数均为受控输入，未换算 FPS。8192 槽计数使用 8191 个健康记录引用和最后一个独立缺页对象，证明按表项扫描的重复成本；不声称实际地图存在 8192 架飞机。Terrain 计数使用实际 SharedSceneProvider 和封存页，只有一个物理 Cell；新原始 token 的构造在计数区间之外。

## 改动文件与行为

本代理改动以下 7 个生产文件。`main.cpp` 的导弹 workspace 部分由根代理修改，本代理只动飞机显示采样调用链，已保留根代理的改动及 CRLF 行尾。

- `source/external_reader/source/camera_assets.h`：约 518 行开始的 VXL capture 现在区分 Absent / Ready / Unavailable。读取失败不会把不完整 body-only 模型或错误 SHP fallback 提交为 ready。需要的 scale、turret offset、variant 元数据和非空部件必须完整。VXL 槽已确认空值是合法缺部件，不要求读取无用 HVA；非空 VXL 的 HVA 失败则拒绝整个候选。磁盘资源仍优先，损坏磁盘资源仍可从健康 live asset 恢复。
- `source/external_reader/source/radar_scene.h`：约 565 行，已确认的必需 VXL 尚未 ready 时，失败重试沿用建筑的 16ms 路径。此前正确拒绝坏候选后会等普通素材的 2 秒重试，真实 voxel marker 会因此暂时阻止新场景发布。解码预算保留，其他可选素材的退避保留。
- `source/external_reader/source/idle_aircraft.h`：新增飞机子样本 stamp，包含 match、frame、serial、原 tick 和已发布计数。显示接受普通 500ms 窗口或精确确认的同一 publication。
- `source/external_reader/source/tactical_sampler.h`：飞机计数、ID 和 stamp 按候选/已发布事务提交，失败恢复上一已发布 cohort。refreshAircraft 只调用绑定的封存 Read。旧 aircraft 子样本落后于暂停来源时，可从已封存来源派生补读。补读失败按 match/frame/serial 记忆，避免对无法变化的缺页来源反复遍历；正常 sample、epoch/reset 清除失败记忆。
- `source/assist/source/aircraft_indicator.h`：按钮绘制和可见性使用已发布飞机 stamp 和计数，避免候选失败、时间流逝或一般 tactical stamp 冒充飞机新鲜度。
- `source/assist/source/main.cpp`：约 1310 行，同来源快速路径在飞机子样本已经过期且 serial 不匹配时派生补读；只使用已 pin 的 WorldScope，失败不 commit，成功保持原 source tick。正常 sample 传入 source serial。没有新增 live/RPM 后备或第二次原始世界捕获。
- `source/external_reader/source/radar_map.h`：约 147 行新增 TerrainStorage 原始存储复用，最多保留 3 个空闲存储。每次发布创建新 shared_ptr 控制块，最终强引用释放后才能入池。旧 weak_ptr 必须过期，即使之后复用相同 raw 地址。保留所有 Cell 读取、预算和 cadence；读者仍持有时正常分配新缓冲，不丢帧、不覆盖。

Terrain 每个存储只重置上次实际写过的下标，空洞默认值与新建 Terrain 一致；real/diagram 切换也恢复正确默认。完整地图的计算和 Cell 读取没有删除。它优化新 publication 的大数组反复分配，不重复声称此前已修好的“同一 publication 多消费者共用 Terrain”收益。

空闲池最多保留约 43.5MiB 的四组固定数组（每份 14.5MiB），另有 touched 索引容量和元数据；仍在使用的 publication 另计。首次捕获、读者长期持有导致池空、模式容量扩展时，仍允许正常分配。受控初始/后备分配记录为 15,345,868 字节 / 24 次 / 4 个大分配，属于保留正确性的后备路径。

## 新增定向测试

- `source/assist/tests/voxel_part_capture.cpp`
- `source/assist/tests/voxel_part_capture.py`
- `source/assist/tests/idle_aircraft_display.cpp`
- `source/assist/tests/idle_aircraft_selection_lease.cpp`
- `source/assist/tests/terrain_storage_pool.cpp`
- `source/assist/tests/terrain_storage_performance.cpp`

### 实际成功执行的主检查

工作目录均为 `/workspace/scratch/40121bc881b8/ra2-ui-146`；精确 argv、退出码、原始日志路径及 SHA256 见 `delivery_checks.json`。

```text
python source/assist/tests/voxel_part_capture.py --output /workspace/scratch/5a310fe852f8/opt147/render/voxel
/workspace/scratch/5a310fe852f8/opt147/render/idle_aircraft_display
/workspace/scratch/5a310fe852f8/opt147/render/terrain_storage_pool
```

VXL 对照使用保存的原 baseline camera_assets.h，其余同一现行调用链和测试。对照 fixture 预期返回 1，驱动的 `--expect-failure` 验证通过；当前 fixture 返回 0。测试覆盖普通/Extra turret 和 barrel，VXL header/tail/body、HVA header/matrix 失败；合法无炮塔/无炮管；空 VXL 搭配非空或不可读的无用 HVA；非空 VXL 缺 HVA；无 VXL 的合法 SHP；无效磁盘资源的健康 live fallback；liveVoxels 两种配置；同 renderer 恢复和健康像素等价。

飞机显示当前编译命令也保存于 `idle_aircraft_display.receipt.json`：

```text
g++ -std=c++17 -O2 -pthread -I source/external_reader/source source/assist/tests/idle_aircraft_display.cpp -o /workspace/scratch/5a310fe852f8/opt147/render/idle_aircraft_display
```

飞机显示测试使用实际 TacticalSampler 和生产 Snapshot，独立 live seam 计数为 0。覆盖子样本落后 serial、确认丢失、500/501ms、失败计数/ID/tick 回滚、空列表、下一真实帧、epoch/frame/serial 隔离、tick wrap。最后对象缺页测试对比本次修复前保存的 tactical_sampler.h：旧版第二轮重复 294,898 次读取并触发预期断言；当前第二轮 0 次，配置重置及下一 publication 恢复均通过。这个对照不是原始 1.0.46.4；它专门检验本轮新补读路径的失败记忆修正。

Terrain 正确性覆盖：同地址复用且旧 weak_ptr 过期；12 个旧 publication 同时持有并校验不变；三线程释放与新捕获；与 fresh Sampler 的数组/CellArt 对照；real/diagram 切换；有部分写入后的候选失败；缺页失败和恢复；budget=1 的多步捕获；reset；Sampler 销毁后迟到读者释放。

### 附加实际执行

```text
/workspace/scratch/5a310fe852f8/opt147/render/idle_aircraft_selection_lease
/workspace/scratch/5a310fe852f8/opt147/render/terrain_storage_performance
ASAN_OPTIONS=detect_leaks=0 /workspace/scratch/5a310fe852f8/opt147/render/terrain_storage_pool_asan
/workspace/scratch/5a310fe852f8/opt147/render/shared_scene_sampling
/workspace/scratch/5a310fe852f8/opt147/render/radar_visibility_publication
python source/assist/tests/building_depth_capture.py --output /workspace/scratch/5a310fe852f8/opt147/render/building-depth
git -c core.whitespace=cr-at-eol diff --check
```

Selection 测试使用未改动的生产 idle_aircraft_selection.h，确认 500ms 可选、501ms 不发指令，暂停显示 lease 不授权过期命令，焦点/连接门槛保留。Terrain ASan+UBSan 运行成功；LeakSanitizer 默认运行因当前 ptrace 环境不可用而失败，故另行明确 `detect_leaks=0`。不能把它写成 LSan 通过。

Existing SharedScene 测试通过：三个并发消费者、一次捕获、无 live fallback、暂停时间戳、缺页恢复等。radar_visibility_publication 测试通过：22 个不完整 terrain 批次拒绝，旧 front 保留并恢复。building_depth_capture 在 VXL 事务修改后通过 209 检查、36 恢复场景、0 像素差；它在最终 required-VXL retry 分支修改之前执行，建筑已有 16ms 分支未变，最终 retry 分支由当前 VXL 151 检查覆盖。Vulkan 运行不可用，未宣称 GPU 实测。

## 限制与未修改项

- 没有 Windows Host、实际游戏、联机或物理 GPU 验收；根代理负责最终交叉构建。分配字节和合成像素计数不代表实际帧率。
- 现存 `enemy_markers.cpp` 独立测试在 `freshReads` 断言失败；已用保存的 baseline TacticalSampler 复现同一断言，属于既有 fixture 问题。
- 现存 `hotkey_host_test.py` 缺少当前 middle_input_host 所需的多个 Win32/UI seams，未能编译；没有为了本任务改动该无关 fixture。实际飞机选择规则由独立生产 header 定向测试确认。
- 本代理未修改 shared_scene.h、版本文件、Notebook 或 INDEX，未提交和发布。
- `delivery_checks.json` 为要求的三个主检查给出实际执行记录，生产 hash 含被改动源及相关 Host 调用文件。调用文件 hash 表示审阅绑定，不能解读成已执行完整 Host。VXL receipt 内所有实际编译依赖的 hash 已与最终源码逐项确认一致。
