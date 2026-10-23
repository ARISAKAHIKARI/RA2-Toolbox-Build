# pre.9：pre.8 通信名称不一致和快捷键队列误报

## 用户现象与已确认原因

用户更新 pre.8 后不断看到“快捷键请求队列已满”，所有快捷键不能使用。本轮查阅 INDEX、pre.3/pre.8 与 SQLite v26；这是工具箱 Host/Runtime 通信错误，未增加原版引擎地址、补丁或事件类别。

pre.8 的 `Version=66`、DLL `AssistProtocolInfo`、Host `AssistHostProtocolInfo` 都是66，但主程序 `connect()` 仍通过硬编码 `Local\RA2Assist_v065_%lu` 创建共享内存；DLL 已用 `Local\RA2Assist_v066_` 打开。实际发布的 pre.8 EXE 中旧格式字符串位于文件偏移1777710。原版引擎和热键配置不是该名称不一致的原因。

因此外部只读采样仍可令 snap.status/renderStatus=1，命令桥却没有有效身份，`commandMatch()==0`。`Pending::add()` 对 match=0 返回 false，Host 把任何 false 都显示成“队列已满”；即使队列空也会这样。此前成对检查仅比较协议号/Shared大小和 DLL 前缀，没有核对 Host 的实际名称。此前 Runtime 夹具用 DLL 协议号构造可见映射名，绕过了 Host 错误；这解释了为什么旧受控用例通过但交付包不可用。

另外发现独立问题：过期动作原来只在 pop 中删除，而 serviceHotkeyBatch 在命令/选兵回执未到时提前返回，陈旧动作可能占满128槽。该问题不需要伪造 ACK 或取消游戏中的已提交指令来解决。

## 修正

- 协议头中的同一 `mappingName()` 根据 `Version` 生成三位前缀和 PID，Host 创建与 Runtime 打开都调用它。两端导出相同计算所得前缀；Host 安装入口前同时校验 DLL 前缀和 ABI，构建验证也核对 EXE 前缀。
- 没有有效命令身份/新鲜连接时不入队；清掉本地等待动作，提示连接未就绪并限制提示频率。恢复后需重新按键，不重放断线期间操作。
- 本地 Pending 在插入前和每次服务时清理超时/旧对局动作，不再把清理放在 ACK 门槛之后；不写共享 ACK，不突破原有串行交付。
- 保留 Assist66、Shared 布局、事件接口及其他功能。文件版本1.0.37.9。

## 验证及边界

`mapping_pair_runtime.py` 从实际 Host PE 读取前缀（历史包使用实际编译的创建格式字符串），将该名称作为 Win32 替身唯一可见映射，再执行编译 DLL。pre.8 在0/5/3/4四种模式都复现 disconnected。新包分别在以下两份样本上验证命令桥身份、指针请求和普通命令 ACK：

- LBW `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`
- 战网 `c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`

`hotkey_host_test.py` 执行生产分派器，覆盖只读画面就绪但 commandMatch=0 时140次输入不入队/不误报满、连接过期、128槽阻塞时超时清理、两种 ACK 通道及恢复后仅执行新按键，另保留组合、双击与选兵回归。`hotkey_routes.cpp` 测满队列插入前清理。

新夹具仍不是 Windows 游戏窗口实测：它执行真实 DLL，在模拟 Win32 映射边界使用 Host 的名称，不执行完整 Windows Host 主循环或双机网络。本次没有新增磁电物理或SHP画面通过声明。结果位于 `verification/review-137-pre9/`。
