# 原生变卖光标：单机启用失败与无关队列依赖

## 现场证据与范围

用户的 Runtime.log（SHA256 见 evidence/native-sell-cursor-admission.json）为 1.0.48-pre.2。20:39:52 首次明确出现 `sellCursor=1/-2`；重复开关后仍失败。连接与命令状态 ready、session=0、运行异常计数为0、hookConflicts=0。日志证明开关请求已发布、原生光标安装/预检失败，不能精确区分两个站点或某段上下文；不能据此认定某个具体平台 Hook 就是现场唯一根因。正式版1.0.48沿用相同实现，也存在本轮复现的检查缺陷。

调查前检索了 INDEX、2026-10-26-network-house-preserve-pre6、2026-11-07-injector-compatibility-adaptation 后续记录、2026-11-10-input-panels-direct-sell 和持久 SQLite 第174版。既有记录已指出 Phobos 可替换4AC18C的队列处理；本轮没有用户运行中的平台DLL/内存，队列替换测试为合成改写。

## 固定映像与原生合同

- 捕获型原生映像 SHA256：94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd。
- gamemd-spawn(4) EXE：c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a。
- gamemd-spawn(5) EXE：ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707。
- 三者的相关指令逐项一致，不把捕获映像哈希当作原始EXE哈希；不公开这些映像。

已有两个写入站点仍为6929E5（5字节CALL Owner判定）及4AC179（9字节CurrentHouse/index读取）。保留原生CanSell调用、原字节结构、所有权journal、游戏线程和身份检查。

4AC168读取对象引用，4AC16A的sub esp,8加4AC172的push 16构成12字节预压参数。单机AI分支调用完整4C6CB0 Sell接收器后，适配器弹掉这12字节，直接到4AC20C原生收尾；不会执行4AC182的构造或4AC18C的OutList容量判断/追加。

## 可复现的缺陷与修复

旧通用patchcontext把4AC18C队列判断作为NativeSellOwner的全局安装依赖，即使单机AI路径完全跳过它，也会因该段变化拒绝整个开关。用同一生产校验头和私有原生映像，把该10字节替换为合成平台跳转，旧版稳定返回失败；不是凭日志推定现场字节。

改为按实际执行路径检查：

- 安装阶段核对两处自身指令与真正依赖的前后文；补充4AC168参数压栈和4AC20C完整收尾调用检查。
- 4AC18C队列检查迁移到native_sell_policy。单机AI不需要该检查；人类Owner及联机仍必须通过。联机还保留网络所属方适配与AI禁用条件。
- 不覆盖第三方队列Hook，不改平台队列结构，不把任意E9当成可覆盖的原始入口。两个实际写入站点被替换时仍拒绝并明确报告。
- 安装状态区分光标/出售入口的冲突、上下文、读取、原指令和安装失败。UI显示具体原因，Runtime.log同时记录状态与两个入口的signature/hook/reason/error，避免只有笼统-2。

## 验证与未决项

生产头测试 `assist/tests/native_sell_cursor_admission.cpp` 在三份映像上通过：原版准入、队列被替换或不可读时单机AI准入、队列条件对人类/联机仍保留；每份拒绝64处必要依赖字节的单字节变异，也拒绝不可读依赖和被替换的实际写入点。旧源码的相同队列替换反例返回1，已保留记录。

构建、源码绑定和交付摘要在最终包记录。本轮未进行用户平台、Windows真实窗口、真实鼠标点击、完整拆除退款或双机测试。现场若仍失败，新版日志用于区分剩余入口差异，不能宣称已证明覆盖用户当前注入组合。
