# Hotfix21：Vera绘制成组迁移与动态GPU页面

本轮按用户要求先整理完整职责图，再完成一轮连续集成。起点是Hotfix20；远端基线 `9d38179b0ba2140cd8bfa6f4b691ab482615209f`。12领域的现状、依赖、生产调用点和保留边界见[迁移总表](2026-10-30-vera-render-migration-map.md)，不能将目录盘点或“已有路径”记作本轮新迁移。

## 代码来源与改变

固定上游 `ARISAKAHIKARI/vera20k@3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f`，YuriPlanet及贡献者，GPL-3.0。重点阅读 `sprite_source.wgsl`、`unit_pose_cache.rs`、`unit_shadow_cache.rs`、`terrain_read_only*.wgsl`、`terrain_packed.wgsl`、`native_surface_format.rs`、`terrain_batch.rs`、`terrain_submission.rs`、presentation实例/阶段/侧栏/UI和radar模块。原Rust/wgpu接口适配到现有C++/Vulkan外部只读宿主，不复制完整模拟器、游戏资源或Metal专用队列上限。

- **材料与姿态分离**：已具备VPL页的VXL保留中性覆盖和索引；颜色放入独立不可变256项色表。现场换色仍重新校验，但不要求同一几何姿态重做GPU光栅。VPL变化、姿态/HVA变化仍使缓存失效。无有效索引的diffuse材料沿用原路径。
- **统一有序命令**：视口和原尺寸动态页共享32字DrawJob编码与一个GLSL文件，分别编译普通、FP64、页面、分bank页面四个变体。CPU回退仍消费同一原始Image、附件、色表和深度。
- **只读Z裁减**：每个32像素bin只在连续“不写Z”段内逆向寻找最后被Z接纳的不透明正文，从它开始按原顺序合成后续阴影与透明材质；遇到Z写入者就结束该段。透明孔、Z拒绝、packed半透明和过滤采样不能当作不透明替代。这里借鉴Vera的等价结果裁减，采用本宿主的每像素顺序计算，没有照搬其atomic实现。
- **原生混合**：NativeSteps档增加上游RGB565半阴影和25/50/75透光混合，包括该profile的5/6位展开表。保留未乘alpha的source用于packed运算；通用预览、过滤、RGB高精度仍使用原有premultiplied素材。插值帧清除不再有效的packed来源。
- **动态小地图页**：先上传静态128×128颜色/深度，再在GPU按世界坐标和原顺序合成动态正文/阴影，之后才面积缩图、迷雾、屏幕特效和直出。页缓存比较底图身份/版本、页坐标和完整有序DrawJob；移动、移除、换色、光照或地图尺寸变化触发重绘。不同页即使共享空白源也不能共享合成槽。GPU未完成时才调用CPU ObjectCanvas，避免先在CPU重复生成动态页。原有存储、素材、候选像素、bin引用和VXL批次预算仍作为整帧回退边界。
- **波形调度**：连续波形区间按保守足迹建立依赖层；足迹包含目标范围、最大3倍scale位移及取整余量。互不干涉的波形可共用前置快照，读取或写入可能相交的波形保序；普通特效和电弧保持独立边界。仅复制各批次读区包围矩形的行段，不复制整个屏幕。波形像素数学仍为既有移植版本，此项不等于上游原游戏波形全像素认证。
- **完整输入复用**：主画面及观察窗比较实际FX命令、批次、索引素材，以及位置、深度、采样、色表、混合和光照；不再仅因存在特效而一概禁用相同帧复用，也不因游戏tick相同就沿用旧画面。
- **本地UI分层**：载员接受0<Size<=512的有限小数，逐项截断total，Infantry/Unit添加Size-1填充，Aircraft只添加自己的帧。新增选中己方移动单位/步兵Tiberium格：gems帧5、ore帧2、空帧0。各UI生产者以临时Plan提交，读取/字体/栅格失败只丢弃该生产者或图层，当前相机与对象一致性检查仍保留。

## 资源格数值与原生证据边界

已查询研究索引、Hotfix2/14/20记录和同一SQLite v95。沿用已记录样本SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 与 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a` 的pips结构支持；本轮未重新执行原代码，也不能把旧Ammo/整数Passenger案例当作新资源格/小数Size已在这些EXE上实测。

| 字段/函数 | 本轮依据和使用 |
| --- | --- |
| `DrawPips 0x709A90`，thiscall、ret12；`GetPipMax 0x716290` | 沿用现有证据和上游 `ObjectType::pip_max`；Tiberium移动单位5格、Infantry3格 |
| `CargoTotalSize 0x473460`、FTOL `0x7C5F00` | 上游 `ui_overlays::passenger_pip_filler_count/scale_pip_draws`、`passenger_pips.meta.json` 的小数Size消费证据；本轮只读重建 |
| Type `+0x380/+0x3D4/+0x3E4/+0x5E0/+0x684/+0x805` | 当前Size、PipScale、wrap、capacity、Ammo及gap；生产双读配置与对象身份 |
| Techno `+0x33C` 四个float，Type.Storage `+0x800` | 上游 `ui_overlays.rs` 的独立gem/ore绘制与oracle描述；本宿主仅接纳非负完整整数float槽，每槽<=1,000,000，正容量<=1,000,000；不推断任意负值、NaN或自定义分数Storage行为 |
| 原资源draw区间 `0x70A230..0x70A29C`；base Fill `0x708C30` | 上游 `sim/game_entity/pip_fill.rs`、`util/native_x87/masked.rs`；除、乘、加0.5每一步PC53/chop，然后FTOL。专用 `native_pip_numeric.h` 以整数长除法和53位截断实现受限正数域；没有改宿主浮点环境，也未搬整个x87模拟器 |

否定“资源格直接有理数四舍五入即可”的假设：上游明确指出4/40×5在PC53/chop中仍低于0.5。也不能把所有Passenger RTTI都当作Size填充者；Aircraft是单自身帧分支。上游oracle的完整边界仍以其meta为准，不把上游受控记录说成本轮本地执行。

## 独立表面与仍未移植项

`mainview::sample` 双读Tactical矩形 `0x886FA0`；`render_upgrade_host::update` 仅把该矩形换算成客户区覆盖范围。世界画面不占用侧栏表面，完整世界帧始终携带生成它的相机。`render_feedback_host` 的原生放置/特效透视孔只在同一精确相机上使用。已有地形、矿石、树木、Bib/门/造卖及槽Anim的职责已映射并复核，本轮没有伪称重写这些既有采样器。

仍明确保留：游戏原生侧栏/建造队列状态机、原生放置红绿判定、全部特殊spawn/mind-control/garrison/absorber状态UI、卸载建筑的tether/radio专属重绘、完整A-buffer、RGB555或自定义16位格式的packed混合、Phobos自定义预览/素材扩展。它们需要另外接通外部读取状态或不同的合成契约，不以猜测字段、错误排序或大块透视孔伪装完成。

## 本轮验证

按用户要求只做交叉编译及离线文件检查：Zig0.13.0，x64宿主+x86 Runtime/Broker；四个GLSL变体经glslang编译、spirv-val校验；PE架构/版本/导入/manifest/既有ABI静态检查。完整哈希与结果见 [render-hotfix21-final.json](evidence/render-hotfix21-final.json)。没有运行CPU测试套件、Vulkan/D3D、Windows loader、受控原指令或游戏，不宣称实际FPS、拖屏残影消失或完整原版像素一致性。
