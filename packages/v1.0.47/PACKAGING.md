# 1.0.47-pre.2 交付准备

本目录准备本地可审查的 Full / Update 包。脚本不修改源码、不执行 Windows 程序、不安装依赖、不推送源码，也不创建 GitHub Release。人类版本为 `1.0.47-pre.2`，PE 文件版本为 `1.0.47.2`；协议保持 Assist 101、Cursor 155、Runtime 4。

下列构建、最终打包命令需要本轮冻结提交及真实回执就绪后再执行。原七组门禁必须本轮重新执行，再加控制台应答、单位目录恢复、规划投递和场景附加图层恢复四组，共十一组。不得沿用上一版通过摘要，也不得把脚本的合成门控测试当作十一组本轮验证。

## 固定资源基线

| 输入 | 字节数 | SHA256 |
| --- | ---: | --- |
| `RA2Toolbox-1.0.46-x64-Full.zip` | 4,428,436 | `d8d5d871e0358579b49c1a2dedbfca48c601ea22119bf897e18593ae7233533c` |
| `release-manifest.json` | 61,217 | `1d30a70514dcba77b26a330db130150850fe24c6fd962c513212542db10dc26b` |

基线必须是已发布的 **1.0.46.4**，source commit 为 `f452d1cb95c57f465356b55a3d7b83fb7403a70a`。脚本核对 ZIP CRC、路径、成员集合及每个成员的大小与 SHA256。

Full 保持 53 个成员；原始素材、三套主题、八个许可证和七个 engine 文件共 47 个资源以此为基线。唯一允许替换的资源是明确审核的 `engine/prepare_runtime.py`；其余资源必须原字节保留。Update 包含三程序、loader、README、Start.cmd、七个 engine 和八个许可证，共 21 个成员。Update 不附带原始素材，适用于已有完整安装。两包共同成员必须完全相同。

七个 engine 都必须与本轮冻结源码及构建回执逐字节一致。相对 1.0.46.4 基线保留了 `prepare_runtime.py` 的单文件依赖验证优化，因此回执必须显式列出这个变更并提供 `dependency_probes`，与十组固定检查合计十一组；脚本仍可识别没有 engine 变更的十组分支。本轮不捆绑 Python 包、生成插帧、预览、缓存、源码、开发记录或个人设置。光标混合画布修复因自动审核阻断仍未交付，未纳入本版。

依赖优化仅在 NumPy、SciPy、OpenCV 三项 managed 缓存记录全部有效且顺序匹配时，把原有三个验证子进程合并为一个。原 `PROBE_CODE` 字节不变，每项版本、来源及功能断言都继续执行；每项重新建立搜索路径，已加载依赖可在该批次内复用。独立总完成标记避免前一项成功掩盖后项零码早退。缺项、批验证失败或批子进程启动错误回到原逐项检查与修复路径；取消继续传播。下载、安装、PowerShell 和取消主体未改，Python 候选检查、引导和实际预处理进程也没有被消除。

独立审查已核对 13 项便携测试及原始收据。测试实际执行原探测代码与批次脚本，但科学模块及 Popen 为替身；8 批计数由 24 次验证进程调用变为 8 次，24 次模块检查与各功能守卫次数保留。这是调用路径验证，不是实际 Windows 子进程启动耗时、真实科学扩展或物理 GPU 验收。

## 最小门控检查

可在冻结、构建前执行以下轻量策略测试。它只读取真实基线并运行合成正负回执，不构建产品、不执行十一组本轮验证。

```bash
RA2_REPO=/workspace/scratch/40121bc881b8/ra2-ui-146
RA2_EVIDENCE=/workspace/scratch/5a310fe852f8/functional147pre2/delivery
RA2_BASE=/workspace/scratch/5a310fe852f8/ui146-delivery-final
python3 -B "$RA2_REPO/packages/v1.0.47/test_package_release.py" \
  --base-full "$RA2_BASE/RA2Toolbox-1.0.46-x64-Full.zip" \
  --base-manifest "$RA2_BASE/release-manifest.json" \
  --report "$RA2_EVIDENCE/package-gates.json"
```

门控覆盖：真实资源与 engine 保留、基线篡改、旧提交/旧版本/错误树、未执行或缺组回执、缺失与变更源码指纹、缺失/变更/空/仅编译日志、失败体素场景、禁入成员、构建产物字节不一致，以及 engine 变更缺少依赖检查或超出审核范围。四个新组额外检查精确计数、两组场景恢复像素差异、伪 `passed` 包装、重复运行拼接与失败输出。门控报告会明确 `synthetic_receipt_fixtures:true` 与 `product_build_performed:false`。

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
  "version": "1.0.47-pre.2",
  "file_version": "1.0.47.2",
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

本轮 `checks` 必须恰好包含下述十个固定组名和 `dependency_probes`，共十一组。各组至少包含下表指纹，可以增加真实依赖。所有路径都相对于仓库，所有哈希都必须与构建回执及冻结 Git blob 的实际字节一致。`log_path` 可以绝对指定，或相对于 validation.json；日志不进入产品 ZIP。

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

新增四组使用实际运行原始日志；不要将包含 `stdout` 的包装收据或人工通过摘要放进这些组的 `log_path`。实际编译、运行 argv 和退出码可从原独立收据提取，原始日志字节和 SHA256 必须保留。

| 组名 | 必需测试指纹 | 必需生产指纹 |
| --- | --- | --- |
| `console_receipt_order` | `source/suite/tests/console_receipt_order_test.mjs` | `source/suite/frontend/app.js` |
| `catalogue_recovery` | `source/unit_monitor/tests/` 下的 `catalogue_recovery.cpp`、`round_transition.cpp` | `source/unit_monitor/source/sampler.h`、`source/external_reader/source/layout_probe.h` |
| `planner_batch_delivery` | `source/assist/tests/planner_batch_delivery.cpp` | `source/assist/source/` 下的 `external_planner.h`、`planner_task.h`、`value_task.h`，以及 `source/shared/source/latest_worker.h` |
| `scene_extra_recovery` | `source/assist/tests/` 下的 `scene_extra_recovery.cpp`、`scene_extra_recovery.py`、`game_assets_host_test.py` | `source/external_reader/source/` 下的 `scene_extra_layers.h`、`camera_assets.h`、`camera_raster.h`、`radar_scene.h`、`radar_decoration.h`、`world_scene_pipeline.h` |

控制台日志必须包含 24 个不同的通过案例，以及同名 suite 的 `checks:24`、整数 `failures:0`、`passed:true` 终结记录。单位目录日志必须为单条整数结果：`checks:93`、`failures:0`、`scenarios:21`、`recovered_catalogues:20`。规划投递必须包含最终批次、重复对象、应答重试及任务状态记录，并以 `PASS: 136 checks, 0 failures` 对应摘要结束；重复对象混合批次必须有下一帧补回 4 项的记录，重复对象重试全过程必须保持最多 3 次，旧 80 项日志不能通过。场景日志要求 `checks=109 failures=0`，`layer_recoveries=9 pixel_mismatches=0`、`scene_recoveries=12 pixel_mismatches=0`，并且每个对应恢复案例也须零像素差异。任一失败、缺少结果、重复运行拼接或只有 `passed:true` 均不能通过。

这些专项执行了真实 ConsoleStore、Sampler、Planner/Task、场景缓存与 CPU 绘制代码；DOM/WebView2、进程内存和文件输入边界采用宿主替身。控制台测试不代表原生窗口验收，规划测试不代表真实游戏指令投递，CPU 像素恢复不代表物理 GPU 运行。前述平台限制仍适用。

已执行的本轮测试可以在冻结后封装回执，只要测试时记录的所有测试与生产指纹仍与冻结提交一致；不能把过去版本的测试摘要重命名为本轮验证。回执表达可审查的来源关系，不提供密码学意义上的执行见证。

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

- `RA2Toolbox-1.0.47-pre.2-x64-Full.zip`
- `RA2Toolbox-1.0.47-pre.2-x64-Update.zip`
- `release-manifest.json`
- `SHA256SUMS.txt`

ZIP 完成 CRC、成员和逐项摘要回读后，脚本再次核对源码、HEAD、产物、日志与回执未变化，再一次性移出临时目录。manifest 固定 `prerelease:true`、`make_latest:false`；不宣称真实 Windows、游戏、物理 GPU 或多人联机已验收。`README.txt` 是随包用户说明草稿，包含上述验收边界及未交付事项。
