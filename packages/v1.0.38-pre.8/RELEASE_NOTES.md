# RA2 Toolbox 1.0.38-pre.8

修复步兵进入建筑或载具后观察窗停止更新，并改进黑幕显示、步兵亮度和矿石／地形图案。

## 本次更新

- 观察窗在步兵入驻后继续跟随载具或建筑，离开后跟回原步兵。
- 修复跟踪窗口误清除和旧异步结果回流；限制未知宿主的搜索量。
- 小地图黑幕跟随现有选项，半透明模式下不绘制黑幕。
- 补算步兵的额外亮度；补齐矿石图案池和部分 TMP 地形变体。

本版已通过 48 项回归及 Windows 构建、覆盖层、设置保存与重启检查。坦克剩余偏暗仍需实景对齐。

## 下载与更新

- [更新包](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/download/v1.0.38-pre.8/RA2-Toolbox-v1.0.38-pre.8-Update.zip)：RA2Toolbox.exe 和 GameRuntime.dll，请成套替换。
- [源代码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/releases/download/v1.0.38-pre.8/RA2-Toolbox-v1.0.38-pre.8-Source.zip)：对应版本的源码、构建脚本和研究材料。

更新前退出游戏与工具箱，保留原 INI、assets 和 cache。构建来源和文件校验值见 release-manifest.json。

## 许可与鸣谢

本项目采用 GPL v3，第三方代码和资料保留原有版权与许可。感谢 YRpp、Phobos、Ares、ccmaps-net 和 FA2sp 提供的接口与渲染参考；项目介绍和完整声明见[仓库主页](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build)。
