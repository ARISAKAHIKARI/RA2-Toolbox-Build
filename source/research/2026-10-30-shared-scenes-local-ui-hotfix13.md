# Hotfix13：共享绘制结果、五类 UI、原生本地反馈

## 证据与适用范围

样本仅本地使用，不发布 EXE 或私有资源字节。受支持 EXE SHA-256：

- `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`

公开布局比对固定 YRpp 提交 `d9795546950fcc75a94325ffc83b44d16ec34ce2`：ParticleClass／ParticleTypeClass、MessageListClass、TextLabelClass、GadgetClass、LinkClass、BitFont、TacticalClass、SuperClass、TechnoClass。新增原生调用没有进入产品；Unicorn 中执行叶代码只用于离线验证。

## 世界来源与缩放前复用

主屏幕独立地形／对象采样在渲染前发布完整 SceneBank。副相机改用同一 SceneRenderer；世界、所属方、边界、显隐策略、原始源时效相符才可消费。主屏幕关闭时，副相机和实景地图有自己的采样生产者；共享来源可用时不重复读取世界对象数组。建筑附属姿态事实准备一次。电弧快照随源一起发布。粒子／光束关闭时停止对应独立采样；重开后旧缺项来源不能满足新请求。

共享原生 TMP 和未按窗口缩放的 VXL/SHP 材料结果。96MiB 的 immutable NativeImageBank 含像素、深度、阴影、纯值 GPU 三角形，不含 HWND／Reader／GPU 驱动资源。源几何只在不可变捕获后计算双指纹；精确调色板、VPL、姿态、世界身份、光栅档和后端参与键。不同相机仍独立裁切、深度／透明合成和缩放；不是一张动态全图成品画布。

PlannerWorld 继续是高级移动攻击／磁电／矿车／防御等规划器的共同不可变输入。六条计算任务的原生武器／装卸／资格查询事实不能用展示 Frame 代替；命令仍经游戏线程统一网关做最终校验。没有使用图像缓存延长命令数据有效期。

同游戏帧的新有效成员列表会刷新 Timeline 当前事实，但不推进插值观察时钟。已确认的空数组能删掉死对象，暂停也不将过期电弧续命。

## 粒子与开关

ParticleClass Array=A83DC8、vtable=7EF954、Type=+AC、Type.BehavesLike=+314、Particle.StartStateAI=+12E（signed byte）、Translucency=+12F。原 GetCurrentFrame 62D830 对 mode 0/1 返回有符号当前值；SHP 素材不接受负帧。原 Draw 62CEC0 的细节档 2 才选透明 25/50/75，普通 ANIM converter=87F6C0、无对象阴影。产品只接入已确认的 mode 0/1 SHP；mode 2 方向火焰、3/4 像素及其它 ParticleSystem 未伪造。

四项默认 ON：粒子／动画、激光／光束、VXL 重光照、SHP 重光照。已有明确 OFF 保留。VPL／调色板不再刷新时使用已确认基础材质和基础对象光照，关闭者不再支付逐对象重光照读取消耗。画质额外处理／xBRZ／Vulkan 的原有独立默认值不被这四项改变。

## 本地 UI 与原生保留

仅主屏幕执行 native_local_ui.h；相机／地图不获取普通命令线、选框、光标或消息文字。

- MessageList=A8BC60，链表首项 +0；TextLabel vtable=7F5B44，Next=+4，X/Y=+C/+10，Text=+30，PixWidth=+38，SkipDraw=+40。5D49A0 绘制消息链；72A4F1..72A55F 取坐标、字体高度和裁切宽度。BitFont*=89C4D0，+4 为内部数据，+1C 是原绘制使用的行高；4A60D0 返回该字体。保守矩形保留原字形，未替换聊天输入，也没有实现 Phobos 新聊天框。
- Super.ShowTimers=A83D50。Super vtable=7F3FE8，Type=+28，RechargeTimer=+30/+38；native caller 6D4A35 从 Type+60 取得标题。6D4CE2..6D4D23：右端 x=战场宽−测量字宽−3，y=战场高−(字体行高+2)*(index+1)。任务计时器 scenario+11E8/+11F0、标题+11F4 走同一 DrawTimer。在产品中保留底部宽松区域；未移植原字形 raster。
- 原主要工厂分支 70ABEA 读取 Techno+3D3。产品保留己方主要工厂及相邻标记区域（该块仍是原生画面），没有捏造 CSF 翻译位置。
- ArchiveTarget=+218，原 setter 70C610 离线确认。产品仅读取并验证选中己方建筑、实际 Map.Cell 指针、合法坐标和当前身份，保留目标旗帜区域／连线走廊。旗帜区域为保守边界，桥面／MOD 外延未实机认证。
- 命令队列路径点、所有组号／老兵标记、射程圈、屏幕文字和 MOD UI 没有因此被标记为全部支持。完整表见 ANIMATION_RENDER_COVERAGE.md。

## 回归与限制

新增 particle_native.py 两样本 1,024 组原生帧决策；local_ui_native.py 两样本 196 组消息／计时锚点及字段决策。均不证明字形像素或完整游戏场景。

新增源共享副相机、共享原生结果身份／并发／淘汰、原生 UI 保留／指针与所属方拒绝、五分类布局、四开关默认与 OFF/ON 持久化回归。最终构建／完整检查结果随交付包 Build-info 记录。无 Windows、物理 GPU 或实际联机运行环境，不能给出 FPS 改善结论。
