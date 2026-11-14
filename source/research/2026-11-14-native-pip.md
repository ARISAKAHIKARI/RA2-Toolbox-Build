# 原生画中画：MO 路线、共用 Render 与恢复事务

用户确认仅为画中画增加游戏原生模式，全图小地图保留原方案。目标 1.0.51-pre.9；默认关闭，旧外部模式保留，游戏内两模式互斥，副屏独立。

## 来源与范围

已核对 MagicShiba/MO_ExperienceEnhancements `a529eeea932ac778df6b30516fe3b068926833ac` 的 `src/gui/PiP.cpp`、`Resample565.cpp`、`src/core/EngineAddr.h` 与 `notes/画中画.md`。MO 小窗借用原生相机与缓存，在游戏绘制线程调用三阶段 Tactical::Render，缩小 RGB16 后局部复制。性能路线是复用原生素材/缓存并减少外部重建和整窗合成，不是异步无游戏 CPU 成本，也不能归因于 GPU。

字段参考 CnCNet/YRpp `d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 Drawing.h：DirtyAreaStruct 为 RectangleStruct+bool，经对齐为 20 字节。本轮没有私有游戏 EXE、反汇编或实机测量；历史 1.001 基线哈希 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0` 仅标识旧研究，未本轮重执行。旧 strict/CodeWritesAllowed=false 笔记属于历史构建；当前仓库已有受管理的原生缩放/侧栏 Hook，本轮按用户授权复用其注册表所有权。

## ABI、借用状态与输入

- Tactical::Render `0x6D3D10`，x86 thiscall `(this, DSurface*, int flag, int phase)`，flag=1、phase=0/1/2。本轮只核对上游 ABI 和主映像可执行地址；没有实机机器码/平台跳转验证。
- map-completion `0x4F451B` 由 zoom/PiP 共用单个注册表项，最后租约才恢复；外来/被替换项不覆盖，恢复失败保留所有权。
- Tactical `0x887324`，相机字段 B0/B4/B8/BC/D64/D68/D6C/D70/D74/D78；D7C/D7D 按两个字节保存。视口 `0x886FA0`，缓存矩形 `0xB0CE28`。
- 替换 Surface/Z/A 五指针 `0x88731C/0x8872FC/0x887314/0x887644/0x87E8A4`，使用两个私有 576×384 RGB16 Surface 与两个深度缓冲。
- 保存 Tactical+DB0 选中计数、`0xB0CEC8` 最多500×12字节，Tactical+E0/E4 最多800个可见格；超合同上限不截断。
- Dirty vector data/capacity/count `0xB0CE7C/80/88`，最多1024×20字节；FullRedraw `0xB0E63C`。恢复内容至当前 backing，不复活已释放旧指针；容量不足保留事务且不发布超容量 count。
- 第一笔写前标记 borrowed，完整 capture 才允许进入；产品 x86 MSVC SEH 边界33 finally 恢复事务，分别隔离两个 Surface 的 Unlock，再归还 capture/depth/LastError。嵌套拒绝不能清理外层。
- 资源仅空闲时分配；原生绘制仅公共完整 Render 范围执行，禁止 Host 工作者调用。每逻辑帧最多一次，缓存2:1缩为288×192；已完成耗时决定下一次17–250ms间隔，当前同步函数不能抢占。左键框选期间复用缓存。
- 每次覆盖保存底图，下次主 Render 入口、隐藏/关闭时恢复。底图和跟随绑定比赛世代/场景/Tactical/逻辑帧；换局、倒退帧、Surface/几何变化禁止回放旧像素。
- 打开固定当前区域；按钮/可配快捷键跟随选中单位 pointer+UID，取消选择不解除，死亡/UID复用停止跟随。统一图层显隐、菜单、聊天、后台、加载门控均生效。
- 现有 WH_GETMESSAGE 消费小窗内按下的左右键及对应松开，画布坐标缩放，隐藏或移出后仍平衡松开。左击只暂存本地视角跳转，由游戏分发验证比赛/200ms期限后复用 `0x6D6000` 导航，不写单位命令或网络事件。
- Assist 108 绑定配套 Host/Runtime；全图小地图链路无改动。

## 验证

`native_pip_core.cpp`：全部部分进入失败、完整字节恢复、邻接字段/容量、脏区换 backing/容量不足和时钟回绕。`native_pip_bridge.cpp`：生产 adapter 配合合成 engine/surface，三阶段绘制、完整内存比较、同帧复用、底图、跟随/UID复用、换局/倒退帧、缩放输入/隐藏后松开、异常与退休。`native_pip_hook_lease.py` 提取生产租约函数检查双顺序、单次安装、外来/替换拒绝和失败恢复。`native_pip_windows.py` 在真实 Windows x86 进程链接产品 MSVC SEH，测试访问异常、只读 Tactical 部分进入/恢复重试、首个 Unlock 异常仍释放第二个；固定基址测试 PE 的 loader-owned BSS 提供合成全局地址，不装载游戏。连续 action/restore 访问异常的 Windows 检查曾暴露嵌套 finally/except 的父帧访问损坏；恢复/解锁已拆为独立 noinline SEH 帧，c12d59e1 的 Windows 检查三次独立进程通过。

Windows 全量 Host x64/Runtime x86、PE/跨位布局检查结果以对应 workflow/artifact 为准。独立测试不能证明实机画面、平台/联网兼容或 FPS 改善。
