修复单位统计的跨局身份识别和旧画面清理：本地玩家阵亡后继续显示尚存对手；下一局复用 House 指针、未采到帧号回退时，也检查对象 ID、名单/名称和败退后恢复状态。采样前后使用绕过页缓存的身份核对；读取失败或中途换局不发布混合结果。换局清空实时快照、分页及绘制表面，保留图标磁盘缓存。

通过 14 个实际 Sampler 回归用例、ASan/UBSan、PE32/ABI/版本/依赖及架构检查。GameRuntime 的可执行代码段与 1.0.4 相同，未新增游戏内拦截。尚未在用户平台、Windows 游戏或真实双机联机复测；本修复处理已确认的生命周期缺口，不声称已经现场重现用户问题。

**智能卸载、科技全开仍停用**；装车、维修、逃生计算未迁移，地图全开方案未替换。保留 1.0.4 默认关闭、标注联机危险的原生飞机路径点及任意建造实验开关。

- `Windows.zip`：完整运行包。
- `Update.zip`：EXE/DLL 更新包，覆盖已有 1.0.2 或更新完整目录。

升级前关闭工具并退出游戏，再替换程序。Release 附件不含源码；[源码](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/v1.0.5/source)及[实现分析](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/v1.0.5/source/RUNTIME_COMPUTE_AND_UNLOCK_REVIEW.md)单独保存在仓库。
