# 设置页首次加载慢／切换后空白：Hotfix7

用户在 Hotfix6 实机反馈：第一次按钮出现很慢，随后切换内容区始终为空。Hotfix4/5/6 的局部重绘与恢复修正不能视为用户环境已解决。本次只调整设置页生命周期、提示窗口和消息调度，保留独立场景读取、渲染和基地车修复。

## 能从源码及受控执行确认的缺口

1. 选中页即使主窗口可见，`request/apply` 仍依赖另一次 foreground 归属查询才允许从隐藏变为显示。前台切换观察滞后或归属未识别时，请求可以被保留而不执行。新实现按选中页与主窗口可见状态显示，放在主窗口正上方、已经位于主窗口上方的外部应用之下，保留 NOACTIVATE，不抢前台。实际 Z 顺序检查不再因外部应用在前台而省略。
2. 战术类别与可见布局曾通过两个独立消息更新，快速切换时分别排队，且 Hotfix6 强制重复类别布局。现在类别进入同一个最新 Layout 邮箱，处理一次最新布局时才在页面所属线程应用该类别。相同页面点击仍推进 order；类别消息重做正确页的子控件显示与裁剪。
3. 每个提示控件曾创建一个独立顶层 tooltip HWND。共享现代控件现在每个父页面只创建一个，工具仍按各自 HWND 注册；控件销毁只注销自身，父页销毁回收共享窗口。受控执行 600 个控件只创建一个 tooltip，而非 600 个。记录的是窗口创建数，不能据此声称某个毫秒级启动提升。
4. 三个模块使用普通 GetMessage/PeekMessage 循环，且处理自投递的高频后台刷新。Windows 的默认取消息顺序会优先处理 posted work，不能仅凭“每类最多一个待处理刷新”推导输入和 WM_PAINT 不会饥饿。新增有界公平取消息：硬件输入、绘制、定时器轮流获取处理机会，每四次优先消息后处理普通队列；光标每轮最多派发 32 条再处理自己的时钟。没有重放、伪造或吞掉输入；错误和 WM_QUIT 保持原语义。

参考：[Microsoft GetMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagea)、[GetQueueStatus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getqueuestatus)。这是基于代码与文档找到的可复现缺口，没有用户现场线程栈，不能断言它们解释每次空白或长时间阻塞。

## 初始化和验证

注册页面保留已经建立的真实 DPI，避免第一次显示再次为整页生成相同字体与布局。光标 WM_SIZE 即使滚动值不变也重新布局控件；窗口队列、真实控件及依赖状态继续保留，不加入 DWM cloak、截图页面、跨线程 parent/owner 或同步跨线程绘制。

新增 `frontend_apply_test.py` 直接执行生产 request/apply，覆盖隐藏页／前台滞后、1000 次三类别切换、合并到最后类别、DPI 重入期间新隐藏请求和定位失败重试。`ui_message_queue_test.py` 执行生产 Queue，持续补充 posted/input/paint/timer 10000 轮，各类均取得进展。`modern_ui_test.py` 增加 600 工具共享和独立注销／父页回收。保留 restore/visibility/display/dialog 回归。

本地为 Linux；没有真实 Windows 桌面或游戏执行结果。尝试准备兼容测试环境未完成，不计为 Windows 测试。最终交付以本地三组件编译／PE 校验及上述实际完成的受控检查为准。本次无新增引擎地址、ABI、Hook 或游戏写入；持久逆向笔记保持 v85。
