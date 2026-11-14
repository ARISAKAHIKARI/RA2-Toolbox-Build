# 特殊单位独立显示与旧渲染设置关闭（1.0.51-pre.6）

问题：特殊单位开关原先是 EnemyMarkers 的过滤子选项，模型叠加启用后还会禁止整路敌方文字标记；旧配置 RenderUpgrade=1 被保留，但原生及控制台入口均禁止任何更改，导致无法关闭。

这是工具箱 Host/UI 源码行为修复，不涉及新游戏函数、地址、ABI 或字段推断；支持的游戏布局及谓词沿用已有实现。未执行新引擎逆向，无新的可执行文件哈希适用范围。

## 实现

- EnemySpecialOnly 保留内部和旧配置身份，含义改为独立特殊显示开关；新增 EnemySpecialEnabled 配置键，缺省回读旧键，保存两键同值。EnemyMarkers 与它互不控制，模式只依赖自己的开关。
- TacticalSampler 统一遍历同一封存 roster，分别输出普通与特殊 Frame，每路保持 2048 容量。位置/名称首次采集共用，发布、回滚、相机重定位同步处理两路；提交仅复制有效前缀。第六参数保留旧过滤调用兼容，Host 使用第七参数申请独立特殊目标。
- 模型 worker 仅消费特殊 Frame。文字策略优先普通 Frame；普通关闭且特殊文字模式开启时使用特殊 Frame；两路同时文字显示只绘一次。模型启用不再禁止普通文字。
- UIKey 加入模式，暂停或同一观察源下切换模式也能触发 Host 更新。菜单、身份、所有权、迷雾遮罩及源时效策略不变。
- RenderUpgrade 默认值仍是 0。已保存开启值可通过可见入口发起关闭并走现有 stop/save 路径，关闭后入口禁用；新开启、同步期间及非点击通知被拒绝。GPU 和相机运动偏好独立。

## 证据与验证

- `assist/tests/special_overlay_settings.py`：当前 UI、owner、INI 及 Host 文字策略，独立组合与新旧键迁移。
- `assist/tests/special_enemy_markers.cpp`：两路开关组合、同帧源更新、一次 roster 读取、发布/回滚/相机/菜单/重置。
- `assist/tests/enemy_markers.cpp`：普通列表已满、后出现的特殊目标仍保留，原有敌我/生命周期/视口验证。
- `assist/tests/special_overlay_lifecycle.py`：普通开关不影响模型激活与完成帧，特殊关闭会清理；原有时效及重试验证。
- `assist/tests/special_unit_overlay.cpp`：真实类型/所有者、模型叠加及特殊对象筛选。
- `assist/tests/render_material_settings.py`：使用实际早期拒绝路径与正常处理路径，默认关闭、旧开启值关闭保存、禁止重新开启。旧测试绕过了早期处理，此次补齐并收窄加载片段到渲染配置。
- `suite/tests/console_display_entry_test.py`：实际可见控件允许旧启用状态下发关闭，关闭后拒绝新开启；保留既有冻结版本负对照。

以上是受控源码/内存夹具及编译验证。不能据此推定实际游戏视觉、FPS、逻辑帧或联机效果；发行工作流结果和构建摘要收录 verification 包。
