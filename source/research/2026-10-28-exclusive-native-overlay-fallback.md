# 2026-10-28：独占全屏保留原生分辨率覆盖层

本说明修正物理 4K 重绘实验的一个错误假设：**独占全屏不等于覆盖层不可显示**。在现网游戏里，既有的独立透明覆盖窗口可以正常显示。Windows 显示模式切换也不应成为隐藏整个覆盖层的理由。

分支：`experiment/render-upgrade-v139pre6-20261028`。此处只修改 EXE 外部渲染/物理显示探测策略，**不改变游戏代码段、显示模式、原生事件、GameRuntime 或正式 latest**。功能线原有的其他补丁功能仍存在，因此不是严格无代码段修改构建。

## 新行为

| 游戏/显示器情况 | 物理 4K 增强 | 已有场景/信息覆盖层 |
|---|---|---|
| 游戏逻辑 2560×1440，完整窗口客户区真正为 3840×2160，且监视器/当前显示模式一致 | 允许 1.5× 自适应重建（默认采样模式为自动时） | 按物理 3840×2160 客户区合成 |
| 4K 桌面上的普通 2560×1440 窗口 | 不伪称输出 4K | **继续按客户区 2560×1440** 绘制 |
| 独占全屏切换到 2560×1440，当前显示模式也是 2560×1440，配置模式仍为 4K | 视为“疑似切换”，**只关闭 4K 增强** | **继续按客户区 2560×1440** 绘制 |
| 当前模式、窗口、DPI 信息不一致或读取失败 | 禁止未经验证的物理 4K 增强 | 保持原本有效的覆盖层与客户区大小（不会因物理探测失败而停用） |
| 游戏逻辑和客户区均为 3840×2160 | 不额外放大 | 按 3840×2160 绘制 |

技术点：`physicalpresentation::renderPath(...)` 只决定 **EnhancedPhysicalPixels** 或 **LegacyClientPixels**，不决定窗口可见性。原有可见性条件仍由游戏窗口、前台、对局和已有渲染状态决定。原先的 `if(physicalBlocked)visible=false` 逻辑已删除。

原本能工作的透明 HWND 仍通过 `overlaycompositor::host().configure(...,client.right,client.bottom,...)` 和 `UpdateLayeredWindow` 呈现。设置中的 `RenderPhysicalOutput` 只是实验性 **4K 物理增强开关**，不能强制改变用户已有的独占/窗口模式，也不会增加 DirectDraw/Direct3D Hook。

### 状态与判定的诚实边界

`SuspiciousModeSwitch` 只是一项**模式变化迹象**，不等价于“独占全屏已被证明”。它决定回退使用客户区而非额外放大，不作为拒绝覆盖的理由。独占模式下覆盖窗口能否真正位于游戏表面之上仍以用户当前客户端表现为准，工具不承诺所有 DirectDraw 实现都必然支持。

### 验证

- `physical_presentation.cpp`：2560×1440 独占疑似切换 -> `LegacyClientPixels`，原覆盖层仍可启用；4K 真实客户区 -> `EnhancedPhysicalPixels`；DPI 模糊、普通窗口均回退。
- `physical_presentation_windows.cpp`：小窗口不能伪装为 4K，且必须保留旧尺寸覆盖路径。
- `validate_physical_presentation.py`：明确禁止重新引入 `physicalBlocked` 关闭窗口路径；继续检查只读显示探测及禁止修改游戏/显示模式。

**尚未进行真实独占全屏/1440p 与 4K 显示器双机截图验证。** Windows CI 编译与模拟窗口测试不代表各类显卡驱动中的独占显示都已实测。
