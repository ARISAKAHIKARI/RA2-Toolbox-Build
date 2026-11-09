# 1.0.47-pre.5 交付准备

本目录准备本地可审查的 Full / Update 包。脚本不修改源码、不执行 Windows 程序、不安装依赖、不推送源码，也不创建 GitHub Release。人类版本为 `1.0.47-pre.5`，PE 文件版本为 `1.0.47.5`；Assist 协议为 102，Cursor 155、Runtime 4 保持不变。Shared 从基线的 23,162,528 字节增加到 23,162,544 字节；只接受这次已审核的尾部扩展（目录 hash、火控满表状态及对齐），不允许把任意布局变化视为兼容。

下列构建、最终打包命令需要本轮冻结提交及真实回执就绪后再执行。保留此前二十九组检查（含 engine 变更时必需的 dependency），本次新增输入诊断、输入占用、两组 Host 输入集成验证和冷启动验证，当前合计三十四组。所有门槛必须绑定本轮实际执行与冻结源码，不得沿用上一版通过摘要，也不得把脚本的合成门控测试当作生产验证。这里的组数不是功能点数，也不是测试断言数。

缺陷前后对照保留 `1.0.47-pre.2` 的固定提交 `46420180d9137932ca98fd2e6c131bde4f2a6294`；它是回归比较来源，不改变下述 1.0.46.4 产品/资源基线。

pre.4 的现场快捷键失效尚未解决。pre.5 补齐已配置按键、实际前台身份、注册、服务提前返回和投递阶段的输入日志，并修复受控复现的零值/过期编队占用期限、跨对局预览占用及世界样本生产者冷启动依赖。默认 OwnGround 绑定本身仍产生 Runtime 需求，因此冷启动缺陷有明确配置边界；用户“先用 35 识别再覆盖”的新反馈与该缺陷的因果关系尚未证实。上述受控检查不能写成现场恢复或真实 Windows 输入已验收。

## 固定资源基线

| 输入 | 字节数 | SHA256 |
| --- | ---: | --- |
| `RA2Toolbox-1.0.46-x64-Full.zip` | 4,428,436 | `d8d5d871e0358579b49c1a2dedbfca48c601ea22119bf897e18593ae7233533c` |
| `release-manifest.json` | 61,217 | `1d30a70514dcba77b26a330db130150850fe24c6fd962c513212542db10dc26b` |

基线必须是已发布的 **1.0.46.4**，source commit 为 `f452d1cb95c57f465356b55a3d7b83fb7403a70a`。脚本核对 ZIP CRC、路径、成员集合及每个成员的大小与 SHA256。

Full 保持 53 个成员；原始素材、三套主题、八个许可证和七个 engine 文件共 47 个资源以此为基线。唯一允许替换的资源是明确审核的 `engine/prepare_runtime.py`；其余资源必须原字节保留。Update 包含三程序、loader、README、Start.cmd、七个 engine 和八个许可证，共 21 个成员。Update 不附带原始素材，适用于已有完整安装。两包共同成员必须完全相同。

七个 engine 都必须与本轮冻结源码及构建回执逐字节一致。相对 1.0.46.4 基线保留了 `prepare_runtime.py` 的单文件依赖验证优化，因此回执必须显式列出这个变更并提供 `dependency_probes`，与三十三组固定检查合计三十四组；脚本仍可识别没有 engine 变更的三十三组分支。本轮不捆绑 Python 包、生成插帧、预览、缓存、源码、开发记录或个人设置。光标混合画布修复因自动审核阻断仍未交付，未纳入本版。

依赖优化仅在 NumPy、SciPy、OpenCV 三项 managed 缓存记录全部有效且顺序匹配时，把原有三个验证子进程合并为一个。原 `PROBE_CODE` 字节不变，每项版本、来源及功能断言都继续执行；每项重新建立搜索路径，已加载依赖可在该批次内复用。独立总完成标记避免前一项成功掩盖后项零码早退。缺项、批验证失败或批子进程启动错误回到原逐项检查与修复路径；取消继续传播。下载、安装、PowerShell 和取消主体未改，Python 候选检查、引导和实际预处理进程也没有被消除。

既有依赖专项包含 13 项便携测试；发布门槛要求本轮重新执行。它执行原探测代码与批次脚本，但科学模块及 Popen 为替身；覆盖 8 批验证进程调用由 24 次变为 8 次、24 次模块检查与各功能守卫次数保持的路径。这是调用路径验证，不是实际 Windows 子进程启动耗时、真实科学扩展或物理 GPU 验收。

## 最小门控检查

可在冻结、构建前执行以下轻量策略测试。它只读取真实基线并运行合成正负回执，不构建产品、不执行三十四组本轮生产验证。

```bash
RA2_REPO=/workspace/scratch/c1b87d8550cd/ra2-hotkey-fix
RA2_EVIDENCE=/workspace/scratch/c1b87d8550cd/hotkey147pre5/delivery
RA2_BASE=/workspace/scratch/5a310fe852f8/ui146-delivery-final
python3 -B "$RA2_REPO/packages/v1.0.47/test_package_release.py" \
  --base-full "$RA2_BASE/RA2Toolbox-1.0.46-x64-Full.zip" \
  --base-manifest "$RA2_BASE/release-manifest.json" \
  --report "$RA2_EVIDENCE/package-gates.json"
```

门控覆盖：真实资源与 engine 保留、基线篡改、旧提交/旧版本/错误树、未执行或缺组回执、缺失与变更源码指纹、缺失/变更/空/仅编译日志、失败体素场景、禁入成员、构建产物字节不一致，以及 engine 变更缺少依赖检查或超出审核范围。前一轮四组、此前十六组、保留的两组快捷键和本次五组输入与启动专项额外检查精确计数与场景结果、伪 `passed` 包装、重复运行拼接、缺失场景和失败输出。逐功能与快捷键新增组的 JSON 结果拒绝重复字段、错误类型和额外字段。本轮还检查巡逻缓冲 ASan 编译/运行参数，以及 Shared 必须为已审核的 16 字节增量。门控报告会明确 `synthetic_receipt_fixtures:true` 与 `product_build_performed:false`。

## 冻结构建

先完成本轮源码、版本信息、`BUILD_SOURCE_MANIFEST.json` 与本目录四个文件的审查和冻结。记录完整 40 位提交 SHA，之后所有步骤使用同一个值。未提交代码、遗漏源码输入或 HEAD 变化都会阻止交付。

沿用 `packages/v1.0.43/build_release.py`；不复制或修改旧构建器。工具链要求 Zig 0.13.0、已准备的 WebView2 SDK 1.0.4314.35。下面三个工具路径由实际已配置环境提供，不下载依赖。

```bash
RA2_COMMIT=填写本轮冻结的完整40位提交SHA
RA2_ZIG=/实际路径/zig
RA2_SDK=/实际路径/webview2-sdk
RA2_CACHE=/实际路径/zig-cache
python3 -B "$RA2_REPO/packages/v1.0.43/build_release.py" \
  --repo "$RA2_REPO" --expected-commit "$RA2_COMMIT" \
  --output "$RA2_EVIDENCE/binaries" --logs "$RA2_EVIDENCE/build" \
  --zig "$RA2_ZIG" --sdk "$RA2_SDK" --cache "$RA2_CACHE" --cpus 6
```

构建输出目录必须为空。构建器记录全部 source blob 的 Git SHA / SHA256，核对前后输入、HEAD、三程序及 loader，并执行静态 PE 校验。最终打包会再次校验实际 PE、架构、版本、协议、Shared 大小及每个产物的构建摘要。

## 本轮真实回执

`validation.json` 使用以下结构。示例中的占位符不是可通过门控的证据；所有记录必须来自本轮实际命令。保留原始日志，不追加或改写 PASS 标记。本轮 `engine_changes` 必须为实际差异的精确列表：`["engine/prepare_runtime.py"]`。

```json
{
  "schema": "ra2.optimization-validation.v1",
  "commit": "本轮冻结提交",
  "source_tree_sha": "该提交的source树SHA",
  "version": "1.0.47-pre.5",
  "file_version": "1.0.47.5",
  "engine_changes": ["engine/prepare_runtime.py"],
  "required_checks_passed": true,
  "windows_executed": false,
  "game_tested": false,
  "gpu_tested": false,
  "multiplayer_tested": false,
  "checks": [
    {
      "name": "missile_allocator",
      "command": ["实际执行程序", "实际参数"],
      "exit_code": 0,
      "executed_this_revision": true,
      "log_path": "../compute/equivalence-and-timing.jsonl",
      "log_sha256": "原始日志SHA256",
      "test_source_hashes": {"source/assist/tests/missile_allocation_equivalence.cpp": "SHA256"},
      "production_source_hashes": {"source/assist/source/missile_compute.h": "SHA256"}
    }
  ]
}
```

本轮 `checks` 必须恰好包含下述十个既有组名、后述十六个逐功能组名、保留的两个快捷键回归组名、本次五个输入与启动专项组名和 `dependency_probes`，共三十四组。各组至少包含下表指纹，可以增加真实依赖。所有路径都相对于仓库，所有哈希都必须与构建回执及冻结 Git blob 的实际字节一致。`log_path` 可以绝对指定，或相对于 validation.json；日志不进入产品 ZIP。

| 组名 | 必需测试（均在 `source/assist/tests/`） | 必需生产文件 |
| --- | --- | --- |
| `missile_allocator` | `missile_allocation_equivalence.cpp` | `source/assist/source/` 下的 `missile_compute.h`、`missile_bridge.h`、`main.cpp` |
| `snapshot_reuse` | `world_frame_reuse.cpp`、`world_frame_reuse.py` | `source/shared/source/` 下的 `world_frame.h`、`world_frame_pool.h`、`world_frame_transport.h` |
| `tech_capture` | `tech_enhancement_capture.cpp`、`tech_enhancement_capture.py` | `source/assist/source/` 下的 `tech_ui_breakpoint_bridge.h`、`tech_enhancement.h`、`tech_parameter_lookup.h` |
| `voxel_recovery` | `voxel_part_capture.cpp`、`voxel_part_capture.py` | `source/external_reader/source/camera_assets.h`、`source/external_reader/source/radar_scene.h` |
| `idle_aircraft_display` | `idle_aircraft_display.cpp` | `source/external_reader/source/` 下的 `idle_aircraft.h`、`tactical_sampler.h`，以及 `source/assist/source/` 下的 `aircraft_indicator.h`、`main.cpp` |
| `terrain_reuse` | `terrain_storage_pool.cpp` | `source/external_reader/source/radar_map.h` |

条件组 `dependency_probes` 要求测试指纹 `source/tools/test_dependency_probes.py`，生产指纹 `source/engine/prepare_runtime.py` 与 `source/engine/runtime-dependencies.json`。该组可以引用原始独立 JSON 回执；结果须包含 `passed:true`、正整数 `tests_run`、整数 `failures:0` 和 `errors:0`。不要将它写回其他组的命令原始输出。

日志检查识别已有真实输出：分配器至少 8,063 个等价场景及同一工作区复用；快照的 `seqlock_interleavings` 通过与正检查数；科技采集的 `summary` 通过与正检查数；体素正场景数、零像素差异、正检查数、零失败；飞机及地形的实际 PASS 行。体素不固定旧检查数，预期失败的旧版对照不能冒充当前通过。

前一轮四组继续使用实际运行原始日志；不要将包含 `stdout` 的包装收据或人工通过摘要放进这些组的 `log_path`。实际编译、运行 argv 和退出码可从原独立收据提取，原始日志字节和 SHA256 必须保留。

| 组名 | 必需测试指纹 | 必需生产指纹 |
| --- | --- | --- |
| `console_receipt_order` | `source/suite/tests/console_receipt_order_test.mjs` | `source/suite/frontend/app.js` |
| `catalogue_recovery` | `source/unit_monitor/tests/` 下的 `catalogue_recovery.cpp`、`round_transition.cpp` | `source/unit_monitor/source/sampler.h`、`source/external_reader/source/layout_probe.h` |
| `planner_batch_delivery` | `source/assist/tests/planner_batch_delivery.cpp` | `source/assist/source/` 下的 `external_planner.h`、`planner_task.h`、`value_task.h`，以及 `source/shared/source/latest_worker.h` |
| `scene_extra_recovery` | `source/assist/tests/` 下的 `scene_extra_recovery.cpp`、`scene_extra_recovery.py`、`game_assets_host_test.py` | `source/external_reader/source/` 下的 `scene_extra_layers.h`、`camera_assets.h`、`camera_raster.h`、`radar_scene.h`、`radar_decoration.h`、`world_scene_pipeline.h` |

控制台日志必须包含 24 个不同的通过案例，以及同名 suite 的 `checks:24`、整数 `failures:0`、`passed:true` 终结记录。单位目录日志必须为单条整数结果：`checks:93`、`failures:0`、`scenarios:21`、`recovered_catalogues:20`。规划投递必须包含最终批次、重复对象、应答重试及任务状态记录，并以 `PASS: 136 checks, 0 failures` 对应摘要结束；重复对象混合批次必须有下一帧补回 4 项的记录，重复对象重试全过程必须保持最多 3 次，旧 80 项日志不能通过。场景日志要求 `checks=109 failures=0`，`layer_recoveries=9 pixel_mismatches=0`、`scene_recoveries=12 pixel_mismatches=0`，并且每个对应恢复案例也须零像素差异。任一失败、缺少结果、重复运行拼接或只有 `passed:true` 均不能通过。

这些专项执行了真实 ConsoleStore、Sampler、Planner/Task、场景缓存与 CPU 绘制代码；DOM/WebView2、进程内存和文件输入边界采用宿主替身。控制台测试不代表原生窗口验收，规划测试不代表真实游戏指令投递，CPU 像素恢复不代表物理 GPU 运行。前述平台限制仍适用。

## 保留的逐功能修复门槛

以下十六个既有组继续强制要求 `exit_code:0`、`executed_this_revision:true`、真实 argv、原始日志 SHA256，以及测试/生产源码 SHA256。编译成功不能替代运行成功；缺任何一组都拒绝打包。条件 dependency 规则及上述十个固定组不变。

| 组名 | 原始日志必须包含的当前结果 |
| --- | --- |
| `planner_defense_delivery` | 21 项零失败；被裁掉的防御下一帧可返回、真正返回后的 350ms 冷却、HostTask 续帧结果齐全 |
| `missile_manual_lifecycle` | 16 项零失败；`dead_history count=1 new_owned=1` |
| `missile_manual_capacity` | 23 项零失败；生产所有权、最终投递及 Host 完成路径的精确摘要 |
| `patrol_capacity` | 31 项零失败；宽度 2/4/8/10/20/40/80 的完整闭环均绑定成功，最长输出不超过 16 |
| `patrol_small_buffer` | 独立 ASan 缓冲运行：`buffer count=16 digest=1280`，1 项零失败 |
| `selection_catalogue_identity` | 单条 `21 checks, 0 failures` |
| `toast_clock_wrap` | 单条 `15 checks, 0 failures` |
| `console_composition` | 16 个不同 PASS 案例及同名 suite、checks 16、整数 failures 0、对应 level |
| `console_hotkey_presentation` | 8 个不同 PASS 案例及同名 suite、checks 8、整数 failures 0、对应 level |
| `secondary_drag_bounds` | 精确摘要：7 个边界案例和 900 个范围案例 |
| `secondary_render_retry` | 精确摘要：8 个提交和恢复案例 |
| `icon_cache_reopen` | 失败作用域→同路径重连→缓存保存/恢复成功；健康重连保持 generation 的精确摘要 |
| `superweapon_result_feedback` | 单条整数 checks 13、failures 0、passed true |
| `pause_key_pairing` | 单条整数 checks 22、failures 0、passed true |
| `native_mouse_pairing` | 单条整数 checks 36、failures 0、passed true |
| `passenger_frame_sampling` | 2158 项；首次绘制 live_RPM=0、1000 次重复绘制无实时读取、失败/时效/暂停/确认/需求路由结果全部存在 |

`patrol_capacity` 与 `patrol_small_buffer` 是同一源码的两个独立实际执行，分别保留原始日志，不拼接结果。后者还必须记录 `compile_command`（含 `-fsanitize=address` 与 `-fno-omit-frame-pointer`，不得关闭 sanitizer）、整数 `compile_exit_code:0`，以及带 `--small-buffer` 的运行 argv。环境允许 `ASAN_OPTIONS=detect_leaks=0`，用于执行器不支持 LeakSanitizer 的场景；AddressSanitizer 仍须启用。不能将未启用 AddressSanitizer 的构建产生的同名 PASS 当作 ASan 验证。

下表为新增组的最少源码绑定；精确集合在 `package_release.py::FEATURE_BINDINGS`。允许补充真实编译依赖。对抽取 fixture，源码指纹绑定表示来源关系，不表示完整原生 Host/UI 被执行。

| 组名 | 必需测试文件 | 必需生产文件 |
| --- | --- | --- |
| `planner_defense_delivery` | `source/assist/tests/planner_defense_delivery.cpp` | `source/assist/source/external_planner.h`、`source/assist/source/planner_task.h`、`source/assist/source/value_task.h`、`source/shared/source/latest_worker.h` |
| `missile_manual_lifecycle` | `source/assist/tests/missile_manual_lifecycle.py` | `source/assist/source/missile_bridge.h` |
| `missile_manual_capacity` | `source/assist/tests/missile_manual_capacity.py` | `source/assist/source/compute_frame.h`、`source/assist/source/compute_input_copy.h`、`source/assist/source/compute_policy.h`、`source/assist/source/missile_bridge.h`、`source/assist/source/missile_compute.h`、`source/assist/source/object_index.h`、`source/assist/source/plan_freshness.h`、`source/assist/source/planner_types.h`、`source/assist/source/unit_task_arbiter.h` |
| `patrol_capacity` | `source/assist/tests/mo_patrol_capacity.cpp` | `source/assist/source/mo_orders.h`、`source/assist/source/mo_patrol_graph.h` |
| `patrol_small_buffer` | `source/assist/tests/mo_patrol_capacity.cpp` | `source/assist/source/mo_orders.h`、`source/assist/source/mo_patrol_graph.h` |
| `selection_catalogue_identity` | `source/assist/tests/selection_catalogue_identity.py` | `source/assist/source/bridge.cpp`、`source/assist/source/main.cpp`、`source/assist/source/protocol.h` |
| `toast_clock_wrap` | `source/assist/tests/toast_clock_wrap.py` | `source/assist/source/main.cpp` |
| `console_composition` | `source/suite/tests/console_composition_test.mjs` | `source/suite/frontend/app.js` |
| `console_hotkey_presentation` | `source/suite/tests/console_hotkey_presentation_test.mjs` | `source/suite/frontend/app.js` |
| `secondary_drag_bounds` | `source/unit_monitor/tests/secondary_drag_bounds.py` | `source/unit_monitor/source/main.cpp` |
| `secondary_render_retry` | `source/unit_monitor/tests/secondary_render_retry.py` | `source/unit_monitor/source/main.cpp` |
| `icon_cache_reopen` | `source/unit_monitor/tests/icon_cache_reopen.py` | `source/unit_monitor/source/core.h`、`source/unit_monitor/source/icon_cache.h` |
| `superweapon_result_feedback` | `source/assist/tests/superweapon_result_feedback.py` | `source/assist/source/bridge.cpp`、`source/assist/source/main.cpp`、`source/assist/source/superweapon_sidebar_bridge.h` |
| `pause_key_pairing` | `source/assist/tests/pause_key_pairing.py`、`source/assist/tests/superweapon_result_feedback.py` | `source/assist/source/hotkey_routes.h`、`source/assist/source/native_message_policy.h`、`source/assist/source/pause_view_bridge.h`、`source/assist/source/pause_view_policy.h`、`source/assist/source/pointer_intent.h`、`source/game_runtime/source/runtime.cpp`、`source/shared/source/key_gesture.h` |
| `native_mouse_pairing` | `source/assist/tests/native_mouse_pairing.py`、`source/assist/tests/pause_key_pairing.py`、`source/assist/tests/superweapon_result_feedback.py` | `source/assist/source/hotkey_routes.h`、`source/assist/source/native_message_policy.h`、`source/assist/source/pause_view_bridge.h`、`source/assist/source/pause_view_policy.h`、`source/assist/source/pointer_host.h`、`source/assist/source/pointer_intent.h`、`source/game_runtime/source/runtime.cpp`、`source/shared/source/key_gesture.h` |
| `passenger_frame_sampling` | `source/assist/tests/passenger_frame_sampling.cpp`、`source/assist/tests/passenger_frame_sampling.py`、`source/assist/tests/world_frame_core.cpp`、`source/assist/tests/world_frame_core_test.py`、`source/tests/winmock/windows.h` | `source/assist/source/display_sample_lease.h`、`source/assist/source/label_motion.h`、`source/assist/source/main.cpp`、`source/assist/source/passenger_draw.h`、`source/assist/source/passenger_motion.h`、`source/external_reader/source/game_view.h`、`source/external_reader/source/passenger_bubbles.h`、`source/external_reader/source/process_reader.h`、`source/shared/source/world_frame.h`、`source/shared/source/world_frame_capture.h`、`source/shared/source/world_frame_transport.h` |

控制台输入专项使用实际监听器与受控 DOM；输入配对跨越实际 Host 发布、Runtime 消息选择器与完整 Assist 过滤函数，但操作系统投递、窗口与原生边界仍受控。巡逻专项执行真实路线生成/图读取，图边界为构造样本，没有执行游戏原生巡逻。乘员专项执行真实读取器、帧传输、collector、租约与显示采样，重复绘制的零 RPM 是宿主计数，不能换算成真实 FPS。其余 Windows/游戏/GPU/联机限制与旧组相同。

已执行的本轮测试可以在冻结后封装回执，只要测试时记录的所有测试与生产指纹仍与冻结提交一致；不能把过去版本的测试摘要重命名为本轮验证。回执表达可审查的来源关系，不提供密码学意义上的执行见证。

## 保留的快捷键回归门槛

保留 `hotkey_command_context` 与 `hotkey_message_recovery` 两组，之前二十七组继续保留。两组必须有本轮实际命令、零退出码、未改动的原始结果日志与测试/生产源码指纹。当前版本通过之外，还要保留同一受控场景在冻结 pre.3 源码 `93f02787d42b61ff5c30e450317f99ba00673149` 上的负面对照。基线源码随测试保存为固定字节 fixture，运行时校验 SHA256，不依赖远端拥有这个本地提交对象：旧版必须先编译成功，再运行并触发断言失败，不能将缺源码、提取失败或编译失败当作复现证据。

`hotkey_command_context` 检查生产 Host 快捷键功能的命令上下文与投递，Win32 状态和命令接收端由受控替身提供。`hotkey_message_recovery` 检查真实快捷键 Registry 对消息边沿、异步按键状态和时间戳的处理。测试不代表真实 Windows 键盘钩子、游戏内指令执行或多人联机已验收。

| 组名 | 原始结果与来源要求 |
| --- | --- |
| `hotkey_command_context` | 原始 `result.json` 中至少 22 个场景、67 项检查通过；production-all.log 的真实运行摘要须与报告逐字段一致；registration/message/service 三个旧版对照分别以预期断言与退出码 2 失败；13 个测试/生产来源摘要与冻结源一致，另绑定当前及三个负面对照原始日志的执行时摘要 |
| `hotkey_message_recovery` | 原始 `result.json` 的正整数场景数、检查数与实际 production_run 摘要完全一致；当前缺消息 probe 为 actual=1/exit 0，固定 pre.3 源为 actual=0/exit 1；真实 Registry、手势逻辑、固定旧头文件和测试脚本四个来源摘要与冻结源一致 |

两组结果拒绝重复 JSON 字段、错误类型、额外的通过包装、未请求或未复现的旧版对照、混合源码摘要，以及缺少真实当前执行的报告。两组场景可随新反例增加；Host 保留最低 22 个场景、67 项检查覆盖，门槛同时核对实际摘要一致及精确测试源码，不把某个旧场景数冒充当前运行。

## pre.5 输入定位与冷启动门槛

以下五组在此前二十九组之外强制新增。它们同样要求本轮实际命令、整数零退出码、未改动的运行日志及测试/生产来源摘要；不能用策略单元测试的合成回执或编译日志替代实际运行。

| 组名 | 原始运行契约 | 最低来源绑定 |
| --- | --- | --- |
| `hotkey_input_diagnostic` | 单条完整 `PASS hotkey input diagnostics: 14 scenarios, 83 checks; pure observation, no Win32/game execution` | `source/assist/tests/hotkey_input_diagnostic.cpp`；生产 `hotkey_input_diagnostic.h`、`main.cpp`、`hotkey_host.h` |
| `input_ownership` | 单条完整 `PASS input_ownership: 61 checks; epoch, zero sentinel, 200ms leases, and normalized wrap boundary` | `source/assist/tests/input_ownership.cpp`；生产 `input_ownership.h`、`main.cpp`、`formation_bridge.h` |
| `formation_input_ownership` | 原始 driver JSON：至少 13 个场景、78 项检查；当前编译和运行均为 0、零失败；旧占用表达式对照编译为 0、运行为 1，恰好五个指定场景失败 | driver、`hotkey_command_context.py`；生产 `main.cpp`、`hotkey_host.h`、`input_ownership.h`、`formation_bridge.h`、`hotkey_routes.h`、`control_link.h`、`protocol.h` 及共享 `key_gesture.h`，共 10 项，并核对编译器解析出的生产头文件摘要 |
| `hotkey_live_observation` | 单次二进制 `run.log`：每个场景一条唯一的 `PASS hotkey_live_observation::...`，末行是同名 suite 的真实布尔 `passed:true`、至少 10 个场景和 40 项检查，以及精确的受控边界 scope；PASS 行数必须等于场景数 | driver、`hotkey_command_context.py`；生产 `main.cpp`、`hotkey_host.h`、诊断头、占用头、`hotkey_routes.h`、`control_link.h`、`protocol.h`、`additional_settings.h`、`type_hotkey.h`、`automatic_features.h`、`activation_diagnostic.h` 及共享 `key_gesture.h`，共 14 项 |
| `hotkey_cold_start` | 原始 driver `result.json`：至少 11 个场景、77 项检查；当前编译/运行均为 0、零失败；固定 pre.4 调用点对照编译为 0、运行为 1，`manual_only`、`manual_restart`、`delayed_connection` 三个场景各六项断言失败，其余场景通过 | driver、固定 `fixtures/hotkey_pre1474/bridge_callsite.cpp`、研究中的 `bootstrap/cold_bootstrap.py` 边界支架；生产 `main.cpp`、`runtime_feature_demand.h`、`control_link.h`、`hotkey_routes.h`、`type_hotkey.h`、`hook_policy.h`、`destruct_owner_input.h`、`placement_event_input.h`、`protocol.h`、`additional_settings.h`、`automatic_features.h` 及共享 `key_gesture.h`，共 15 项 |

上表中未展开前缀的生产文件均位于 `source/assist/source/`，共享头位于 `source/shared/source/`，测试 driver 位于 `source/assist/tests/`。冷启动边界支架的完整路径是 `source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py`。各组精确必需集合由 `INPUT_HEADER_BINDINGS` 与 `INPUT_INTEGRATION_BINDINGS` 定义。

两个纯头测试验证观测状态机和占用规则；绑定 Host/Runtime 集成文件表示源码来源关系，不能把纯头执行称为完整应用执行。`formation_input_ownership` 编译实际 Host 入口和 Registry，仅替换 Win32/命令边界；负面对照只替换旧的占用表达式，其他代码仍为当前版本，不能称为历史二进制执行。它的 `log_path` 指向原始报告，`supporting_logs` 必须绑定 `<报告文件 stem>-artifacts/current/run.log` 和 `baseline-expression/run.log`；原始 stdout/stderr、逐场景结果、摘要、失败集合和 SHA256 必须相互一致。五个旧表达式反例是 `unset_high_uptime_poll`、`old_match_active_preview_poll`、`old_match_lease_poll`、`unset_high_uptime_wm`、`ancient_deadline_poll`。

`hotkey_live_observation` 执行实际诊断 helpers、注册、service 和 dispatch 函数，Win32/input/command 边界受控。其 `log_path` 必须是实际二进制 `run.log`，同目录 `result.json` 必须作为 `supporting_logs` 唯一成员绑定执行时的大小和 SHA256。详细报告的五个摘要字段必须与最后一条原始 stdout 逐字段一致；14 个来源摘要、真实编译命令、整数零编译/运行退出码、`windows_executed:false` 和 `game_tested:false` 均须存在。检查可增加场景，不能通过重复 PASS 行或重复运行拼接伪造覆盖。

`hotkey_cold_start` 的 `log_path` 为实际 `result.json`，`supporting_logs` 必须绑定同目录 `current/run.log` 与 `baseline/run.log`。固定旧调用点 SHA256 为 `5cf66cfe242d5d65bf9ea2f468c1a07a0526d9a7de90964e1132e9a9f4a526c2`。两份日志分别与报告的 stdout/stderr 和摘要逐项核对；拒绝缺失负面对照、编译错误冒充断言失败、错误类型、混合源码或变更日志。完整采样、merge、注册函数来自当前生产代码，旧对照仅换回三条 tick 语句；不执行 Windows、真实游戏或旧版本产品。

执行计划与执行回执分开。独立工作目录中的 `run_checks.py` 按 `pre5-check-run-spec.json` 真正运行每组命令，使用全新的输出目录，并在运行前后记录测试和生产源码 SHA256。`planning_source_sha256` 仅用于安排检查，不得作为已执行证据。全部实际运行成功后，`finish_validation.py` 才能把回执绑定最终冻结提交；它不会补写 PASS、改写已执行源码指纹或替代产品构建。

## 本地打包

完整构建及 validation.json 就绪后执行。输出目录必须尚不存在，以防混入旧包。

```bash
python3 -B "$RA2_REPO/packages/v1.0.47/package_release.py" \
  --repo "$RA2_REPO" --expected-commit "$RA2_COMMIT" \
  --binaries "$RA2_EVIDENCE/binaries" \
  --provenance "$RA2_EVIDENCE/build/build-provenance.json" \
  --validation "$RA2_EVIDENCE/validation.json" \
  --base-full "$RA2_BASE/RA2Toolbox-1.0.46-x64-Full.zip" \
  --base-manifest "$RA2_BASE/release-manifest.json" \
  --out "$RA2_EVIDENCE/packages"
```

如稍后已完成源码同步，可在同一命令增加 `--source-sync "$RA2_EVIDENCE/source-sync.json"`。该可选回执必须包含匹配的 `local_commit`、`root_tree_sha`、`remote_root_tree_sha`、`remote_commit`，以及 `remote_tree_verified:true`、`blob_digests_verified:true`。缺省仅记录本地冻结来源；提供并通过核对后记录已同步来源。此参数不执行 push，也不创建 Release。

成功仅输出四个文件：

- `RA2Toolbox-1.0.47-pre.5-x64-Full.zip`
- `RA2Toolbox-1.0.47-pre.5-x64-Update.zip`
- `release-manifest.json`
- `SHA256SUMS.txt`

ZIP 完成 CRC、成员和逐项摘要回读后，脚本再次核对源码、HEAD、产物、日志与回执未变化，再一次性移出临时目录。manifest 固定 `prerelease:true`、`make_latest:false`；不宣称真实 Windows、游戏、物理 GPU 或多人联机已验收。`README.txt` 是随包用户说明草稿，包含上述验收边界及未交付事项。
