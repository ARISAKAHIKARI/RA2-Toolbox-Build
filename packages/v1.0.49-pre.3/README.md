RA2 Toolbox 1.0.49-pre.3 — JJ 硬件科技许可兼容候选

覆盖现有完整包使用；请一并替换 EXE、GameRuntime.dll 和 Host32，重启工具箱及游戏后再启用。素材与依赖包不变。

在科技设置中开启“高危：硬件科技生产许可重查兼容”，再使用科技全开参数拦截或科技加强精确等级模式。默认关闭，保留原侧栏范围和其他平台现有行为。原实现和私有 OutList 模式均保留。

兼容项将硬件科技权限扩展到本方许可类型的其他 CanBuild 查询；不改规则、不额外占断点。额外许可放行次数不代表生产成功。科技加强的“按等级 1 比较”仍独立配置。

可能影响联机同步；关闭可能取消依赖该权限的在产项目。平台若绕开原生查询自行读取规则，本项仍不能解决。总统/校车旧版缺项原因仍待确认。

受控源码回归与配套 Windows 构建/PE 静态检查已通过；尚未进行 JJ 实机或双机联机验证，不宣称已修复 JJ。

源码提交：[b06e7ac](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/b06e7aca2b0b3f4b59d6efbc622bb06a31899ed1)。未创建新的 GitHub Release。

[RA2Toolbox-1.0.49-pre.3-x64-Update.zip](https://chatgpt.com/library?file_id=libfile_fde3_f-c7b9616408ac81918f5384ad3f2985df_e70932d743d88191950045c9a74e889b&account_id=personal)

[RA2Toolbox-1.0.49-pre.3-verification.zip](https://chatgpt.com/library?file_id=libfile_fde3_f-9d71529cc84c8191aec1315c0214f2d2_e70932d743d88191950045c9a74e889b&account_id=personal)

最终 Runtime 在完整配套构建后重编译了唯一变更的 bridge 翻译单元并重新链接；其余成品及对象按哈希复用，最后重新核验全部 PE、Assist107 与配套关系。完整构建、增量收尾、两项负向对照和最终回归记录包含在 verification 包。没有 JJ 或真实联机验收。
