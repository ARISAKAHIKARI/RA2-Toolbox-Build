# v1.0.30-pre.4 实现与验证

基线：v1.0.30-pre.3 / dc21beb7e288512393fab05c4f1da40d20a20787。文件版本 1.0.30.3；Assist 57、Runtime 4、入口管理器 3。Assist 的躲弹观察字段和高刷诊断字段变化，宿主与 Runtime 必须成套更新。

## 拖屏频率偏差

原实现有三处问题：宿主及 Runtime 门限使用 GetTickCount；120 Hz 整数周期取 8 ms；额外相机更新调用 WWMouse::GetCoords（0x7BA360），它只返回缓存的 +1C/+20 坐标。前两项造成唤醒量化和节拍漂移，第三项会重复旧的鼠标位置。[Microsoft GetTickCount 文档](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount)说明其分辨率通常受 10–16 ms 系统计时器限制。

新增 shared/precise_clock.h，仅用于显示节拍和快速采样。事件时间戳、心跳、源快照期限仍使用原 GetTickCount 时域；旧的 clock_ms.h 未改。宿主以 QPC 判断 Cursor/Tactical 到期，在前台就绪且启用高刷或快速战术功能时申请 timeBeginPeriod(1)，将现有 timer queue 改为 1 ms；退出时恢复 4 ms 并成对 timeEndPeriod。仍只有现有 GETMESSAGE/POST Hook 和合并唤醒。

Runtime 用微秒相位计算下一绘制时刻，迟到跳过旧槽，保留原相位。滚屏速度按微秒累计，物理位移型右拖保留原位移处理。实时 GetCursorPos + ScreenToClient 从已验证属于当前进程的原生游戏 HWND 获取坐标；不改写 WWMouse 缓存、不模拟鼠标输入。原按钮状态、原相机更新与 GScreen::Render 路径不变。全部模式仍没有运动插帧或逻辑帧推进。

新增 scrollRefreshRateMilli、scrollRefreshMaxGapUs 是按实际额外 Render 调用统计的窗口平均频率和最长间隔，不是屏幕呈现计数。真实表现仍受 Windows 唤醒、消息泵、渲染器和显示器影响。

## 原版散开与本版躲弹

[Phobos 的 aircraft scatter 说明](https://phobos.readthedocs.io/en/latest/Fixed-or-Improved-Logics.html#customize-the-scatter-caused-by-aircraft-attack-mission)与提供的原版函数一致：Aircraft Attack 在 FireWeapon 之后对目标格调用 Cell::ScatterContent。样本 0x418432–0x418476 调用武器开火，随后 0x418478–0x4184BD 调用 0x481670。Cell 逻辑涉及格内精英单位、PlayerScatter、SCATTER 能力与 House IQ 等条件；不能简化成任意三星单位面对所有弹种必躲。

旧躲弹根据飞机目标、距离与剩余冷却 <=12 帧提前移动，方向偏向离飞机更远，后续固定等 12 帧。这与上述开火后散开的时机不一致，沿弹道逃离飞机也未必避得开。

新实现读取 [YRpp BulletClass](https://github.com/Phobos-developers/YRpp/blob/phobos-dev/BulletClass.h) 的 Array 0xA8ED40。样本 Construct 0x4664C0 验证 Type +AC、Owner +B0、Target +10C、Damage +6C；活跃/非 Limbo、原生 Bullet vtable 0x7E46E4、正伤害、当前索引中的空中发射者构成观察条件。使用 TargetCoords +140/+144 与 Velocity +E8/+F0 选择横向避让；垂直落弹按落点距离。识别直接单位目标或单位当前所在的 Cell 目标，不扫描任意附近爆炸范围。敌方碾压分支保留。

每个躲避任务记录最近弹体 ID，实际新弹体绕过原有 12 帧等待及该旧威胁的三次移动预算；同一发重复采样不触发新发条件。仍受每单位每逻辑帧一条自动指令限制。对同一帧较晚出现的弹体重新采样，不再由 dodgeFrame 直接跳过。前台快速采样使用 QPC，宿主唤醒被合并；没有增加 FireWeapon/Scatter Hook 或修改单位能力。

与原版仍有差别：这里只通过同步 Move 事件执行，不能与原版开火流程中的即时 Scatter 等同。没有保证完整碰撞预测、导弹脱锁、瞬时伤害躲避或联机成功率。原生弹体未被观察到、模组换 vtable、目标不匹配、路径阻塞、单位速度不足和事件延迟都可能影响结果。

## 验证范围

最终二进制与逐项报告见 verification/pre4/summary.json。刷新用例执行当前 x86 DLL、原生 ScrollMap/SetTacticalPosition；Win32 输入、边缘/右拖来源和 renderer 为替身。增加冻结旧鼠标缓存、QPC 1 ms 步进 / GetTickCount 16 ms 量化下的 120/240 Hz 节拍、最长间隔和计数验证。战术用例覆盖两个事件后端、内外计算、未开火不躲、同帧较晚投弹、连续新发、同发不刷、横穿方向、治疗弹排除、己方和盟军试打及恢复。

原有入口回归检查继续覆盖命令时钟、租约与事件边界；宿主检查验证粗时钟不动时仍可按 QPC 唤醒，以及高精度定时器申请/释放配对。未进行真实 Windows 图形、完整游戏物理、兰博启动后渲染跳转目标或双机联机测试。科技与地图功能未禁用，也没有把此次改动宣称为战网掉线修复。
