# 1.0.37-pre.7 实现与验证

基准 pre.6。用户日志与原版 `Unit::ReadyToNextMission` 证实 Sticky=6 不可切换后续任务；磁电/高级移动停驻统一改为 Guard=5，共用准入拒绝旧 Sticky。修正图像名偏移 +1F8，放宽地形未使用字段验证，增加准确格式诊断。Assist 65、Runtime 4 不变。

运行验证记录位于 `verification/review-137-pre7/`：

- `sticky_mission_native.py`：两份私有样本，各8案，执行原版队列、任务就绪和任务切换代码。空闲移动器、radio和地图查询为替身。
- `magnetron_guard_runtime.py`：实际 DLL，内外计算、单机/联机队列、Guard、旧 Sticky 拒绝和关闭后的无续发；pre.6 负例在 `guard-before.log`。
- `magnetron_stop_reacquire_runtime.py`、`magnetron_manual_drop_runtime.py`、`enhanced_object_attack_runtime.py`、`advanced_range_runtime.py`：验证现有输入归属、两武器目标、范围、延迟事件、手动保护及范围切换。
- 当前 `external_planner.cpp`、`advanced_range.cpp`、资源、SHP、地形、观察窗、朝向及只读诊断测试通过。资源回退使用原生 ImageFile 偏移且图像名不同于类型ID，覆盖原先测试遗漏。
- 历史 `advanced_regression.cpp` 缺少新 CombatProbe 数据，在未修改 pre.6 的第12行也失败；保存 baseline 与当前失败记录，未算作通过，也未以此回退现在正确的范围决策。
- PE32、版本1.0.37.7、导出、ABI、Windows manifest及依赖验证通过；发布 manifest给出最终二进制哈希。

实机修复效果仍需更新后新对局确认；旧存档已经留下的 Sticky 不会自动清除。地形虽然已找到 SNO 文件，尚无当前文件头样本证明全部缺失原因；保留具体诊断。未执行完整 Windows 游戏、寻路、音频或双机网络测试。

详见 [研究记录](research/2026-10-24-sticky-assets-137-pre7.md)。
