# 平台 Map::Reveal 三处上下文跳板：证据仍不足放行

## 问题与既有依据

用户补交平台单次采集后，核对 `0x577D90` 的新 E9 是否只是透明转送，是否能为原生揭示和旧 direct Map 模式增加精确兼容。调查前已读研究索引及 SQLite v125 中的启动依赖隔离、旧平台启动兼容、地图揭示/特殊格记录。本记录只整理已经完成的只读分析；未更改生产准入或执行游戏代码。

采集 ZIP 为 `RA2Master-Capture-20261104-221046-3684.zip`，SHA-256 `39b10d1e2a4338ce9b2f92449429b28c5e365ce3859a1b47220217240f19fe71`。`session.json` 报告 Probe 0.2.1、PE32 x86、映像基址 `0x400000`、映像大小 `0x794000`，磁盘 EXE SHA-256 `ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707`。19 项核心启动签名匹配，信标和 Reveal 两项变化；这不等于未知替换函数已经获得兼容授权。私有映像、目标函数原始字节及反汇编不提交。

## 三个站点与有效性

主映像 `0x577D90..0x577F24`（右端不含，共 404 字节）均为 valid/stable=1，包含下列三处 E9：

| 原站点 | 私有目标 | MRGA1 handler RVA | handler 返回 0 时的 fallback |
| --- | --- | --- | --- |
| `0x577D90` | `0x03D60000` | `0x146AB0` | 重放完整入口 5 字节，回 `0x577D95` |
| `0x577E1E` | `0x03D70000` | `0x146AD0` | 重定位 CALL，仍调用 `0x4ADEE0`，回 `0x577E23` |
| `0x577F03` | `0x03D80000` | `0x146B20` | 重定位 CALL，仍调用 `0x4ADCD0`，回 `0x577F08` |

三个目标均为 MEM_COMMIT、MEM_PRIVATE、PAGE_EXECUTE_READ，AllocationBase 等于目标地址，各有完整 4096 字节区域及全 1 valid mask；对应 `platform-private-276..278.mem`。入口目标另有 `platform-target-64356352.mem`，与 private-276 全页逐字一致。目标/private 片段没有专用 stable mask，不能写成目标 stable=1，也不是原子快照。

相邻下一函数 `0x577F30` 另跳到 `0x03D90000`，使用同类包装器；它不属于上述 Reveal 主函数范围。附带 JSON 只保存地址、元数据及摘要，不含原始代码。

## 能证明的 ABI 与不能证明的语义

每个包装器有效代码至偏移 `0x7E`，先保存通用寄存器和标志，向模块 handler 传递保存上下文的指针。上下文含 Origin、EFLAGS、EDI/ESI/EBP/ESP/EBX/EDX/ECX/EAX。handler 返回 0 才选择包装器 `+0x74` 的原指令 fallback；非零返回值可直接决定其他续址。handler 也能修改被恢复的寄存器、标志和栈；包装器只约束修改后的 ESP 不低于原 ESP、差值四字节对齐且不超过 4096。保存/恢复现场不等于现场不被改变。

因此，正确原指令重放和续址只证明存在 fallback，不能证明实际调用总走 fallback。入口 fallback 使用 `[ESP+4]` 参数和原 ECX，原生余体保存/恢复 EBX、EBP、EDI，`0x577F21` 以 `ret 4` 返回，与既有 thiscall、单个 House 指针栈参数的记录一致。原余体仍包含 House `+241`、CurrentHouse `+240`、Cell `+130/+12C` 写入以及多人特殊格路径；本次没有执行它们，也没有证明平台 handler 对这些行为的影响。

模块列表给出 MRGA1.DLL 基址 `0x55770000`、SizeOfImage `0xC3A000`，三个实际 handler 为 `0x558B6AB0/0x558B6AD0/0x558B6B20`。共同失败处理目标为模块 RVA `0x98A80`。模块名称和大小仅作为采集身份线索，不构成兼容授权。

## 结论、拒绝的推断与下一步

当前证据不足以放行 Reveal 或旧 direct Map 模式。已排除“只验证入口 E9 或复制的原指令即可证明透明”的推断；主函数内还有两个可改变上下文/控制流的 handler，不能仅凭尾部 `ret 4` 推定替换函数的完整 ABI 与语义。

关键缺口是三个 handler 本体及其依赖。本次模块目标片段从信标目标 `0x558C3CD0` 向后采集，不覆盖位于其前方的三个 Reveal handler，ZIP 也没有完整 MRGA1 映像。补采应包含这些 handler 的控制流/调用依赖及 valid/stable 数据，或完整只读模块映像。未来兼容边界至少需覆盖三个原站点、三个完整包装器及其重定位关系、handler 身份/行为、原生续体和内存归属；不能按模块品牌、MEM_PRIVATE 页形状或任意 E9 放行。

以上为用户实时采集的静态分析，没有执行平台 handler、受控完整 Reveal、Windows 功能验收或真实联机测试。沿用 pre.4 的局部拒绝与无关功能依赖隔离；没有绕开平台入口或覆盖其跳板。
