# 1.0.49-fix.1：特殊单位标识、覆盖层连续性与输入恢复

## 范围与证据

基线 `f854e7c7c31058f03e66ca9cd83a8bdb204cd32b`。用户要求停用隐形、幻影和敌方间谍的模型补绘，复用敌方单位标识；同时报告跟随单位的信息闪烁及快捷键失效。本轮为来源审查和受控宿主测试，没有用户现场日志，也没有 Windows 游戏 / 平台实测，因此不能把以下缺陷认定为现场全部症状的唯一原因。

没有新增原生入口或修改游戏代码段。沿用既有 1.001 字段：真实 House `+0x21C`、cloak `+0x220`、disguise `+0x1D8`、真实 Type 指针、Type ID `+0x24`、永久 Invisible `+0xC9A`、Infantry Infiltrate `+0xEBE`。原生幻影类型 ID 为 `MGTK`；模组伪装载具按已生效的 disguise 状态归类。所有者筛选仍排除己方、盟友、中立、死亡及装载单位。

既有支持映像 SHA-256（未在本轮重新反汇编或执行）：
- `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`

## 修改

1. 单位状态的“敌方单位提示”下增加“仅隐形、幻影和敌方间谍”。默认关闭，开启后过滤现有标识；真实兵种 / 所属颜色沿用公共采样和绘制。Display/EnemySpecialOnly 独立保存，切换设置或同帧新 publication 都刷新成员列表。旧补绘 UI 移除，保留内部实现和旧选项编号，但旧 INI / 按钮消息不能再发布 revealUnits。
2. passengerView 原先遇到 unconfirmed 或 WorldFrameScope 获取失败直接 clear，绕过 120ms Sample lease。现在只在同 PID、match、house、相机、视口和选项下保留完整值；失败不续期、不做另一轮实时 RPM，成功空结果立即撤下。baseline 负对照在第一次 unconfirmed paint 失败，新实现通过。
3. 普通覆盖层原先先推进 compositor camera，再等异步 World 位图，期间旧层被拒绝。现在延迟提交相机，完成的 World 画面和相机通过现有 replaceGroupAt 原子提交。移动相机下只容许不足 120ms 的完成帧；换局、尺寸、投影和隐藏仍失效。普通信息显示也使用已有短读失败视口保留。紧边界位图的变换原点对齐设备像素，避免非整数缩放时边界移动改变数字栅格相位。
4. formation / MO 拖动若丢失鼠标 UP，active preview 会持续占用所有快捷键。新增物理松开 250ms 取消路径，覆盖全部五种可配鼠标键及原生编队拖动；不会合成 UP 或补交指令。native continuation 的 down / consumed lease 一并清理。编队键盘按住状态从物理键查询恢复，避免队列键态残留。
5. Host 只检查 helper 存活不足以证明 Win32 回调仍在。新增连续观察下 callback 3s 不前进的传输恢复，至少 10s 间隔；宿主采样间隔超过 1s 重新观察，避免宿主卡顿误触发。异步 Remove ACK 后再 Install，保留 Shared、订阅、match 和 request/ACK，不重放指令。
6. 显式重连始终重新注册并同步快捷键；过期连接请求传输修复。status -2 的故障连接走原有 stop / retirement，等待清理完成，不强制确认未完成命令，不强制卸载。

## 回归

- `assist/tests/special_enemy_markers.cpp`：真实阵营、三类单位、己方/盟友/中立/死亡/运输/永久隐藏排除、同帧设置及 publication 切换。
- `assist/tests/reveal_settings.py`：8 种旧 INI / 消息组合均无法重新开启模型补绘。
- `assist/tests/passenger_frame_sampling.py`：2185 项受控检查，包含旧源负对照、1000 次同帧绘制零实时 RPM、120ms 不续期和相机/选项失效。
- `assist/tests/world_presentation.cpp`：五种缩放的栅格相位、相机期限及已有提交恢复。
- `assist/tests/input_ownership.cpp`、`lost_gesture_release.py`：物理键、五种鼠标键、延迟/丢失释放及续行租约。
- `assist/tests/host_broker_retry.py`：38 项完整生产 pulse 测试，覆盖异步恢复、退避、多个目标、请求序号保留及健康回调不重装。
- `assist/tests/manual_reconnect.py`、`formation_input_ownership.py`、`hotkey_routes.cpp`：重连动作、13 场景 78 项生产快捷键路径和现有路由。
- `assist/tests/external_reader.cpp`：原有统一采样与相机高频缓存回归。

受控时钟、内存、键盘和 Win32/broker 替身仅证明上述路径；Windows 编译 / PE 核验记录随更新包单列，不等价于视觉流畅度、系统热键送达或联网验证。旧 `fault_reconnect.py` fixture 与已有 FPS / 只读状态接口不匹配，本轮没有将它作为通过证据。未改变网络事件构造、接收校验或平台权限规则。
