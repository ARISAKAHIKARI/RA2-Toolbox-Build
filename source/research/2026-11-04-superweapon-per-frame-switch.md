# 每帧持续授予超武实验开关

用户在原生动作 34 的验证结论后明确要求“加个开关试试”。前置研究、索引和持久 SQLite 已先复查。此轮将该原生调用接入产品，不把它伪装成网络事件。

## 可见行为

- 实验性功能新增“高危：每帧持续授予全部超武”，默认关闭，INI 为 `Experimental/SuperweaponPermanentGrant=0`。
- 启用后遍历当前玩家 House 已注册的 Supers；每个观察到的新游戏帧，逐项执行原生 `GrantPermanentSW`。同帧重复唤醒不重做，也没有每秒、每 60 帧或分批限额。充能不强制设 Ready，不发送发射事件。
- 原生动作的永久授予会保留。因此关闭、暂停工具箱或断开连接只停止后续调用；已授予超武继续留在该局，不冒险移除地图或游戏另外授予的超武。
- 普通游戏模式也可试开，但会改变本机模拟状态，仍可能联机不同步。没有加入单机白名单或额外确认弹窗。

实际接入仍由原有消息调度器及 Assist lease 执行，Host 启用现有 4ms frame-sampling 唤醒，并将新帧物理输入送入同一调度器。不安装模拟帧 Hook 或硬件断点；如果游戏在两次观测之间推进多帧，只处理当下这一帧，不追补过去的 Grant。不能把这个观察语义称为无调度遗漏的逐帧回调保证。

## 原生边界与身份

支持 PE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。`0x6E1C40` 为 thiscall、四参数、RET 16 的完整地图动作；117-byte FNV `8B1ECC5B`。原始函数仅从 action `+90` 读取 Value，产品传递一个具有该字段布局的局部值载体；完整函数指纹不符时拒绝，不依赖未构造的其他 TAction 字段。

另核对四个下游入口前 12 bytes：Grant `0x6CB560=5DC034C2`、AddCameo `0x6A6300=B32476A7`、GetTab `0x6ABC60=C602F953`、redraw `0x6A60A0=049FEAAA`。原生动作本身调用 Grant(false,false,false)、清 Super `+60 CanHold`，并为当前玩家执行侧栏呈现。

使用 House Supers vector `+254`（items `+258`、count `+264`），核对实例 owner `+2C`、Type `+28`、实例／类型 UID `+10` 及类型 ArrayIndex `+98`。每次调用前重新检查帧、世界、House 身份、vector 与实例；失败停止本帧后续项并报告当前已成功项数，已经执行的原生授予不回滚。战败、失败方、观察者、菜单／暂停及未知世界不授予。没有写游戏指令、虚表、充能值或断点槽。

配置和状态由 Assist95 的追加字段传递，Shared 为 23,141,824 bytes；Host/Runtime namespace 同步为 v095。界面显示实际授予项数／最后观察游戏帧以及原生签名、身份或结果异常。

## 验证

`superweapon_grant_runtime.py` 执行实际编译 DLL 的 manager/filter/dispatcher/授予 helper 及原始动作／Grant／timer 注册：15 项 PASS，包含 180 连续帧、370 次原生动作、同帧去重、无 Host-ready 的物理输入、丢失后补授予、关闭／重新开启、战败、原生指纹改变与外来 owner 拒绝。仅 Win32、工作线程创建失败及本地呈现为明示 seam；没有执行 Launch 或真实网络。

`superweapon_grant_settings.py` 执行实际开关分支、INI load/save 与 Shared 同步语句：22 项 PASS；冷连接 demand 现有用例 146 项 PASS。实际编译 DLL 的消息路由 31 项 PASS。跨位数 ABI 的 787 项布局检查通过，Windows 配对构建以本提交 CI 为准。原有全量 `auto_settings_ui_test.py` 的旧 fixture 缺少多个已经存在的 Host 全局声明，未计为通过；新增设置用例单独覆盖此开关。

证据摘要：`evidence/2026-11-04-superweapon-switch.json`。不把受控原生执行、Windows 自有进程检查或编译通过称为真实对局、完整超武生命周期或联机一致性验收。
