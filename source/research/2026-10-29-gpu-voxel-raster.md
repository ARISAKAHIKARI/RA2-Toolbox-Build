# GPU VXL 光栅预处理与整帧合成

## 范围与来源

用户要求把适合 GPU 且更快的渲染工作尽量迁移，不限于分数倍率解析。此前 CPU `voxel::render` 为每个面变换顶点、执行原生 VPL 选页，再串行遍历两个三角形包围盒中的像素。本次把这些像素覆盖、局部 Z 竞争及颜色/索引输出移入 Vulkan 计算预处理，结果保留在整帧颜色/材质 atlas，供后续场景深度、现场光照、透明/阴影和迷雾合成使用。

调查前读取研究索引、`2026-10-25-native-voxel-light-direction.md` 和 `2026-10-29-overlay-opacity-voxel-posture.md`，沿用现有已验证的 VXL/HVA 几何、Drive/Ship 姿态及 VPL helper。本次没有新增游戏地址、调用或写入，因此没有新的 PE 地址/ABI 结论。这里的 CPU oracle 是现有公开源码 `voxel::render`，不是声称已执行或替换游戏内部 Vulkan renderer。

## 保留在 CPU 的工作与 GPU 包

`gpu_voxel.h::prepare` 使用相同 `posePoint`、`posturePoint`、`postureLight`、`nativeVoxelPage` 和 VPL 表，生成最终投影三角形及预乘颜色/原始着色后 palette index。保留原生光照中的朝零 float、sin/sqrt 查表和特殊法线规则，避免把它们用任意 GPU 浮点近似替代。模型/HVA 解码仍保留，GPU 开关不改变其几何来源。

三角形采用 std430 **64 字节**布局：三个 vec4 顶点及 uvec4 material；push constants 为32字节。CPU 计算精确图像包围盒、anchor 和 depthBottom，VXL 与站立形状的深度关系不变。原生1×与高密度几何都可使用；它不是仅在最终缩放时才运行的后端。顶点变换、精确 native VPL 选页及当前包围盒准备仍在 CPU，不称为所有 VXL 数学已迁移。

## 确定性并行光栅

`gpu_voxel_raster.comp` 编译为三个 Vulkan 1.0 kernel：

1. 各三角形独立遍历包围盒，用原 CPU 半像素中心及 -0.0001 barycentric 容差，以 uint atomicMax 选择最大有序 IEEE float Z。
2. 对等于最终最大 Z 的候选执行 atomicMin，选择最早三角形，重现 CPU 的严格 `z > previous`，避免 GPU 执行顺序改变共面材质。
3. 每像素从赢家写出颜色及32位索引；无赢家写透明。正负零统一为同一 depth key。

Z 与赢家共用一个 scratch buffer 的不重叠区间，因此每个 shader 仅用4个 storage buffer binding。scratch 在不同 actor 之间顺序复用，只按最大单个 actor 增长；各 pass 有 compute 写读屏障，每个 actor 清理前有 compute→transfer 屏障。大图 resolve 使用二维 dispatch，避免依赖 x 方向超过65535个 workgroup。

`vulkan_voxel_backend.h::Stage` 仅负责准备和记录，不提交 queue、不等待 fence、不进行逐 actor CPU 回读。整帧协调器将新/变化的 packet 输出到同一常驻 atlas，已有 owner 复用，不再上传/光栅未变的姿态。Stage 使用有界三角形及 scratch 存储，检查范围、有限顶点、设备限制及工作量；OOM 停用 GPU 本轮路径，超时资源沿用主协调器的隔离规则。Vulkan `precise` 禁止 FMA 收缩，但不把它解释为所有硬件除法均与 CPU 逐 bit 相同。

## 生产路径与 CPU 回退

`Image::gpuRaster` 保存不可变 shared packet，`Context::presentationGPU` 使 VXL body 和原有 flatten shadow 只准备包，不先生成 CPU 像素。CPU 缓存预算计入目标图像及三角形包开销，空 pixels 不再被误计为零成本。`Image::skipZ` 表达非 flat projectile 的无 Z 读规则，避免只为标记规则而分配 NaN 深度图。

`Surface::blit` 在需要 CPU 回退时才 materialize 同一 packet。保留 caller 的 shadow 偏移、附件、flat、writeDepth、depthBias 和 projectile no-Z 标志；内部 voxel Z buffer 不误作地形深度。GPU 失败不会为逐对象回读等待，也不重新采样游戏对象来拼接另一时刻的场景。原 CPU 路径默认不变。

## 静止姿态跨实时 VPL 刷新的包复用

SceneRenderer 每轮主动更新 live VPL，不能据相同逻辑帧直接保留旧渲染结果；然而每轮都分配新的三角形包也会让 GPU 误认为素材一直变化。`gpu_voxel_cache.h` 现在将已成功捕获的几何绑定到独立 shared lifetime nonce，复制/移动保留身份，重新捕获产生新身份；不以游戏对象地址、model.data 地址或 ImageKey 单独判断。手工可变且没有 capture nonce 的 ActorArt 绕过该缓存。

key 精确比较256项 RGBA 色盘、27个32位 pose/参数字，包括 body/turret/pitch、turretOffset、rasterScale、projectile/shadow 与完整坡面/rocking 状态。VPL 每轮仍实际读取和认证；Context 对256页×256索引的有效映射做完整字节比较，内容未变时保持同一不可变 snapshot 身份，变化时换新身份。因此不依赖一个内容 hash 的无碰撞假设，也不把现场 cell light 烘焙入包；cell light 继续由场景 shader 处理。

缓存最多256项且保守计费不超过64MiB，包含 packet capacity、key/entry 与每项完整 VPL snapshot 的计费；热姿态直接返回同一 shared packet，使 CPU 顶点准备、GPU 三角形上传和局部光栅都可跳过。flatten shadow 用独立 key，并只在 miss 时复制/压平模型；车体坡面变化不无端重建原有平面阴影。process/match 切换清空缓存。Stage 的三角形上传另外固定96MiB预算，scratch按最大单actor复用且最多128MiB，两者按实际所需字节分配，避免96MiB预算被二次幂增长到128MiB。

生产资产夹具在40轮 fresh VPL 捕获后断言 packet owner 与 build计数不变；另验证10轮 body/shadow 同时命中、真实坡面变化只失效body、palette/pose/raster/VPL变化失效、capture nonce 重建及copy/move命中。独立 cache 测试还检查相同原始地址但不同 shared control block 不命中、LRU预算、oversized绕过及identity reset；优化与ASan/UBSan通过。

## 验证与边界

`gpu_voxel.cpp` 对96组姿态、261025像素、15915个三角形、1083427个候选像素比较 GPU 输入准备→CPU packet oracle 与原 `voxel::render`，RGBA、palette index、float depth 全部精确一致；涵盖 VPL/回退光、独立炮塔、projectile pitch、坡面/rocking、1–4密度、alpha124/255及共面顺序。优化版和 ASan/UBSan 通过。

实际软件 Vulkan CI `40742034261`，commit `bd5314e7ae4fcdc711f4ab7068ee78c223f34ad0`，job `123005885508`：`gpu_voxel_vulkan.cpp` 执行真实三阶段 shader，再读回供测试验证，**96组／261025个颜色与索引像素精确一致**，额外 atlas offset 前后哨兵未被改写；ASan/UBSan 与 Vulkan validation 检查通过。运行设备为 llvmpipe LLVM20.1.2。测试 harness 的逐 actor readback 只用于独立 oracle；生产 Stage 没有该 API。

`gpu_voxel_scene.cpp` 进一步检查 deferred VXL→常驻索引/颜色→现场光照/超采样→完整场景单次回读，以及热缓存零纹理/三角形上传、shadow、emissive、站立 Z 与 projectile no-Z。无 ICD 的本地环境已对96组完整同帧 CPU 回退验证，实际整帧 Vulkan 结果由主整合 CI 另行记录，不能以独立 raster 结果替代。

生产 `game_assets_host_test.py` 新增 GPU VXL/flatten shadow 延迟光栅、模型与文件读取不增加、保留 slope/VPL，以及相同 Surface CPU 回退像素/深度断言；原有完整资产测试通过。现有 `voxel_preview`、`voxel_posture` 和 `voxel_lighting` 回归继续作为兼容门槛。

未测量真实显卡游戏 FPS，不以软件正确性推导性能。未知移动器、原生 Shadow_Matrix 等原有边界不因 GPU 迁移而消失；此项保留现有已知行为，不额外宣称完整原生阴影。对应 shader 哈希与数值证据见 `evidence/gpu-voxel-raster.json`。
