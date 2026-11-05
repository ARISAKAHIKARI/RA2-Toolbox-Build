# 1.0.42-pre.1 显示与副相机修复验证

基线为 GitHub `main` / 正式版 1.0.41，`513aeb4cf03238cce956bd35e0c9381c2a7cc484`。保留该版本已有改动，配套重建 x64 主程序、x86 Runtime/broker 与固定版本 WebView2 loader。

- [建筑静止动画绘制准入](2026-11-05-building-animation-draw-eligibility.md)及[证据](evidence/building-animation-render-fixes-20261105.json)：原生 38 组、当前读取器与原始 guard 负控、独立合成 SHP 的 20 次／240 像素和缓存验证。修正比原生绘制更严格的存活过滤，不制造已删除动画的末帧；尚未证明用户现场夹子缺失的具体生命周期状态。
- [星级素材的共用 SHP CPU 缩放](2026-11-05-rank-material-scaling.md)及[证据](evidence/rank-material-scaling-20261105.json)：保留屏幕锚点和小地图最低物理像素；普通 SHP 的纯 CPU 函数提取前后逐像素一致。全局 recovery 开关保持原策略。
- [基地车转换与容器跟踪](2026-11-05-watch-conversion-container-tracking.md)及[证据](evidence/watch-tracking-20261105.json)：严格新生差分、原身份与宿主身份分离、未观察到瞬时展开时保留实时点位；该类己方基地车真正死亡时也可能留在原点，可手动关闭。未知身份不授权猜测接替。

证据各自绑定实际测试的生产源码 SHA-256；构建另记录完整输入指纹并核验 PE 架构、版本、ABI、依赖及包内字节。发布仅上传已本地验证的配套包，下载草稿资产核对 SHA-256 后再公开。没有游戏进程、实机副相机视觉／物理 GPU FPS 或联机验收；历史完整 host fixture 在 Windows 改编下的旧 Surface 断言失败仍如实保留，不能将本轮针对性通过表述为全部历史测试通过。
