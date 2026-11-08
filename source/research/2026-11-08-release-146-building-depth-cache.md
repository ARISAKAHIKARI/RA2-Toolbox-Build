# 1.0.46 修订：建筑深度元数据缺页后的错误缓存

## 问题和调查边界

用户报告建筑的一部分会被地形挡住。调查前检索了 INDEX、Hotfix28 渲染恢复、目标/地形合成、异步原生深度和混合场景优化记录，以及持久 SQLite 笔记 version 165。功能基线为 `e5e9abd69cdf2963f7bef3dd8fbc3e12c4537a63`，本轮按用户要求并入同一个 1.0.46 正式版。

本次定位工具箱实际 `Context::captureActor`、SHP/TMP 解码、`SceneRenderer` 缓存和 `Surface` 合成，没有重新执行游戏绘制指令。原生布局沿用既有 SHA-256 为 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 的研究与固定 Vera20K `3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f` 深度语义；这不是本轮对游戏像素的 oracle 或实机认证。

## 确定性反例

建筑基础尺寸来自 Type+EF0 的 native foundation 索引，以及 21 项宽/高表 `8192B8` / `819310`。旧 `animations::foundation` 用返回默认零的读取覆盖已有 INI 回退值，`captureActor` 忽略失败后将宽高 clamp 成 1。即使上层拒绝当前不完整世界，内部 actor/raster 缓存也可能已经提交了错误元数据。

实际 Context 反例使用一个 3×3 建筑和 64×96 图像。仅让基础尺寸表所在 sealed 页面首次读取失败，旧 capture 仍成功且缓存 1×1，真实 buildingDepth 将下部17行、共1,088个像素裁掉。实际 SceneRenderer 中，下一帧读取恢复后依然 `materialsReady=true`、decode attempts=0，最终画面与完整输入的新 renderer 相差904个像素；完整输入控制差异为0。

另一个独立反例让全局 BUILDNGZ 指针 `89DDBC` 首次不可读。旧代码在读取和解码前就推进 `maskRevision`，将缺页伪装成当前资源版本确实没有遮挡形状；下一帧恢复后不再尝试。影响深度的 `ZShapePointMove`（Type+1530/+1534）和 `NormalZAdjust`（Type+1520）也存在把读取失败当默认值缓存的问题。

## 修复与有效回退

生产修改仅在 `source/external_reader/source/camera_assets.h`。建筑缓存提交前必须读到 foundation 索引、有效 native 宽高以及两个 ZShapePointMove 分量和 NormalZAdjust。成功读到 Custom/扩展索引时保留已有 INI 尺寸回退；有效索引的表页缺失或尺寸非法会失败，不能退成1×1。

仅实际需要 BUILDNGZ 锥体时才读取和解码它。保留 native `43D787` 的宽度≥8跳过行为，以及 Flat 不使用锥体的语义。指针读取失败必须重试；成功读到 null 且确无文件资源仍是合法无 mask 情况。非零指针或实际存在的文件未能解码时不缓存成永久空值。

新的 mask 和 revision 先放在本次 actor 的局部候选中，只有完整 actor 捕获、最终对象身份和调色板校验都成功后才提交。迟到失败不会丢掉此前正确的 mask，也不会将新资源版本标为完成。下一帧完整输入能重新解码，并恢复到健康参考输出。

没有改变地形深度公式、前后次序、原生 Z 比较或 GPU shader，也没有把建筑统一置顶。早期怀疑的 BUILDNGZ 双重偏置已有历史修复，本轮证据不支持重复改动该公式；裁剪形状与完整 canvas 的更广泛原生等价问题也未作为本次修复收益。

## 真实生产路径回归

专项 `building_depth_capture.cpp/.py` 使用实际 Context、SHP/TMP 解码、SceneRenderer 缓存、CPU Surface 和 GPU packet 记录，只有 Win32、文件与进程内存为受控边界。当前209项检查全部通过；10类缺读/失败事务、36个不同尺寸/形状/相机/姿态的恢复场景均与健康参考逐像素一致。

相同夹具使用保留的旧生产头时出现61个失败，36个恢复场景累计26,984个差异像素；修复后为0。完整健康输入在旧/新实现中的摘要均为 `cf6be4c502df7ecf`。前景地形仍正常遮挡138个建筑像素，其余6,006个建筑像素可见；严格前后建筑深度关系也保留。

GPU packet 与 CPU walk/coverage 的数据对应关系通过受控检查；实际尝试 Vulkan 时环境返回 instance unavailable，没有据此宣称物理 GPU 或 Windows 渲染通过。最终源绑定、健康等价、预期失败基线及相关旧夹具结果见 [release-146-revision-20261108](evidence/release-146-revision-20261108/) 中建筑收据。

这证明并修复了能造成所报症状的一条确定路径；没有实际游戏截图或现场帧采集，不能把所有可能的建筑/悬崖/MOD遮挡问题都宣称已经验收。发布后应以相同场景观察实际效果。

另行运行既有完整 `game_assets_host_test.py` 时，在进入建筑部分之前的 overlay Surface 颜色断言失败。使用旧生产头亦在同一断言失败，已单独记录为既存验证缺口；不能将该完整夹具列为通过，也不为通过它而修改无关颜色断言。
