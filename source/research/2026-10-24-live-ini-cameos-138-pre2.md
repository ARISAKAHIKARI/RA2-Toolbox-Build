# pre.2：读取游戏使用的 INI 与图标配置

日期：2026-10-24。状态：实现与受控测试准备；没有新的游戏实机读取证据。

## 问题与证据范围

用户要求建造图标、单位统计先根据 INI 确定文件名，并询问能否读取游戏实际使用的 INI。调查先查阅 INDEX、pre.14 覆盖层说明、pre.2 ccmaps 渲染研究及现有 asset/cameo 实现。持久 SQLite 的本地执行环境仍不可用，先记录到仓库，后续写回同一笔记文件。

本轮新增 ABI 来自 YRpp 固定提交 d9795546950fcc75a94325ffc83b44d16ec34ce2 的 [CCINIClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/CCINIClass.h)、[GenericList.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GenericList.h)、[IndexClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/IndexClass.h)；属于头文件布局推导，没有本轮反汇编确认或游戏进程验证。目标是 YR 1.001 布局；没有新增“已实机支持”的 EXE SHA256。旧研究的 LBW 268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0、战网 c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a 不能自动当作本轮 INI ABI 的执行证据。

## 地址与结构

- Rules 的全局变量 0x887048 保存 CCINIClass 指针；Art 在 0x887180 是对象本体。二者不能按同一种间接级数读取。
- CCINIClass 构造器设定 vtable 0x7E1AF4。实现必须验证该值，未知布局回退。
- 32 位 GenericNode 大小 0x0C，GenericList 大小 0x1C；INIClass::SectionIndex 推导偏移 0x28。
- INISection::Name +0x0C，EntryIndex +0x2C；INIEntry::Key +0x0C、Value +0x10。
- IndexClass 开头依次为 table/count/capacity，表项是 CRC 与 32 位对象指针，共 8 字节。本实现按名字做有界查找，不假定排序或复用引擎的 Archive 游标，不调用任何引擎函数。

限制每表 16384 项、容量 65536，名称 127 字节、值 4095 字节；检验地址范围、字符串终止与重复 section/key，捕获前后核对索引与 root。缓存以资源/进程/局次及五秒刷新代次失效。原生对象已经给出图标名时无需遍历 INI。

## 采用顺序

1. 类型已解析的 Image/Cameo 优先，显式 None 不得被磁盘默认值复活。
2. 缺少图标名时，查本局保留的 Rules/Art CCINI；有效空值也不能被同键磁盘值覆盖。
3. 对不可读或不存在的键，从实际进程目录的 loose/MIX 资源读取 INI 补充，按 Image -> Art Cameo/AltCameo 与有限别名链解析；最后保留既有缺图行为。
4. 建造面板与单位统计共用 RuntimeCameos 解析路径；副摄像机的 Art/Rules 查询也通过 EffectiveINI 先查 live，再查资源文件。

**全局 INI 不等于所有最终生效值。** 地图与 Ares/Phobos 等扩充可能直接改类型字段或保存在扩展对象中；不能宣称全局 INI 包含所有覆写，更不能承诺还原完整原始文件。尚未覆盖扩展引擎的私有 Cameo 配置或任意动态字段变化。

## 同时发现的图标问题

旧单位统计解码仅把 flags=3 当透明游程，flags=2 被当成带行长的原始像素；现与已有副摄像机 SHP 解码一致，bit 1 表示 RLE，透明尾部填充可裁剪，文字像素越界拒绝。图标文件名缓存扩展至 127 字节；持久缓存版本升到 2，避免沿用旧误解码图。

## 验证

新增 source/assist/tests/live_ini.cpp 覆盖 Rules 指针与 Art 对象、大小写、越界指针/计数、未终止值、并发索引变化、缓存更新、空值与 native/live/file 优先级；cameo_catalog.cpp 覆盖 Image/Cameo 映射、别名、None、长文件名和 flags=2 图标；unit_monitor/tests/icon_cache.cpp 覆盖旧格式拒绝及长文件名往返。验证工作流同时保留既有显示、资源、Windows 编译与真实控件持久化测试。

测试结果以对应 GitHub Actions 运行记录为准。内存模型测试不证明实际游戏中的资源选择、图像效果或性能。
