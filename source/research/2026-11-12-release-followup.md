# 1.0.50 发布后的检查与修复

## 已发布版本

`v1.0.50` 固定在 `810a4f9b1905f730acfd4a7b9d7b0cba04b74288`，已发布为 Latest。
`v1.0.49` 已撤回为草稿，保留原标签与附件。
本笔记所述后续代码修复不在已发布的 1.0.50 附件中，未覆盖已发布文件。

## 暂停画面合成失败后的恢复

特殊单位叠加已解码全部素材，但 compositor 临时返回空图时，Host 仅以 `missing == 0` 判定已经绘制完成。
同一暂停样本、相机和设置持续不变时，后续请求会一直被“无变化”判断跳过，无法自行恢复。

完成判定增加“已有合成图或实际没有可绘制单位”。临时合成失败仍按已有 16 ms 节流重试；成功后恢复缓存复用，真正空集不重复提交。
扩展 `assist/tests/special_overlay_lifecycle.py`，执行生产 Host 状态处理：修复前确定失败于暂停重试断言；修复后验证 15 ms 不重试、16 ms 重试、恢复后不再重复提交、空集保持空闲。

## 检查与发布流程

- 修复控制参数 fixture 缺少的新共享字段、超武／地图 fixture 被默认攻击脚下快捷键干扰，以及敌方标识 fixture 的旧 `fresh` 读取接口。保留功能断言；ABI 交给独立跨位数校验。
- Windows 地图／控制／科技 fixture 复用现有 `breakpoint_windows_build.py`，编译并链接实际生产 SEH 边界；地图 fixture 恢复 C++ 标准库头文件。
- 将特殊单位、气泡／目录采样、输入恢复、模式说明与前端事件回归接入持续检查。
- 固化本次发布过程中已验证的修正：读取 GitHub 中文响应明确使用 UTF-8，主分支按 API 非强制快进；完整安装包在发布步骤前保存为 Actions artifact，发布接口失败也能恢复。
- 首页和版本说明统一为 1.0.50。

本地受控测试覆盖 Renderer/compositor、Host 异步结果准入、采样连续性和输入租期；不等于真实游戏视觉或联机验证。
本机 Chromium 启动被运行环境拒绝本地 socket，浏览器布局与 Windows 运行检查交由 GitHub Actions 执行。
本次没有新增游戏地址、ABI 或逆向行为结论。

## 发布后的验证记录

- [9060553 的主检查](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/actions/runs/43153536308)：配套 x64 Host、x86 Runtime 与 broker 编译成功；Host 回归和真实 Chromium 控件、3000 行虚拟列表、宽窄布局检查通过。浏览器 fixture 没有脚本异常或 console error。
- 当前线程私有 rendezvous 会注册 VEH；回读不确定时保留清理所需资源但停止功能回调；忽略 continuation 的注入需覆盖一次正常尝试和一次有界清理。旧 Windows fixture 按当前服务接口更新，外部槽位、清理和上下文不变断言继续保留。
- 跨位数测试仍锁定旧协议 104。已核对 1.0.50 发布源本身使用协议 107，声明未被本次修复修改；独立 Windows x86/x64 测量全部一致。更新固定基线为 Shared 23162680 字节，并锁定新增尾部字段偏移，未放宽为任意布局。
- Place fixture 接入当前实际 enqueue/filter 所需依赖；非 Place 销售、私有队列和无关构造器分支为显式失败 seam，不模拟或声称验证这些功能。
- [首轮后续 Windows 记录](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/actions/runs/43154594982)中，独立科技等级设置与建筑放置完成清理通过。
- [修正后的 Windows 复核](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/actions/runs/43155618064)全部通过：控制参数、科技四槽、断点退出状态、928 项 x86/x64 布局及真实生产 Place helper 的 128 项受控入队／输入检查。测试代码提交 `33bd93a15ded1bebfd52b54d66fc86e43856a98d`；此后仅更新说明并移除临时分支验证 workflow，常规检查已调用这些测试。
- Windows 复核 artifact：`13916415326`，SHA-256 `083ba1f9e52a5c9af5a9b9d3541250f38caeae70716f99214972b788ab8ac704`。

这些记录是受控进程和浏览器验证，没有运行真实游戏对战。历史 `publish-v143-pre1.yml` 在直接链接旧硬件 fixture 的步骤仍失败，不将它计入本次已通过的检查。
