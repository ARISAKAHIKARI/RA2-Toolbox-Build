# 实景小地图：GPU 分页面积重采样、原生迷雾与静态画面复用

## 问题与边界

用户要求继续优化小地图与所有覆盖层，并明确把适合 GPU 的工作移到 GPU。调查前查阅 INDEX、持久 notebook v80（95篇）中的原尺寸小地图合成、Vulkan 场景、GPU 原生迷雾与 DDraw 参考记录。此项属于工具箱宿主渲染与缓存，不新增游戏地址、函数调用、Hook、进程写入或协议字段；新算法不适用某个游戏 EXE SHA/ABI。全部数字来自生产宿主代码及合成输入，未取得实机游戏或硬件显卡 FPS 证据。

实景小地图沿用原先“原尺寸 TMP、覆盖图、SHP/VXL、阴影、Z 合成完毕，再统一面积缩小”的语义。主视口的目标像素 GPU 路径不能直接用于很小的整图：那会改变原生像素覆盖、Z 和小物件面积贡献。新实现把这条 native 管线的面积滤波与最终原生迷雾交给 GPU，**没有宣称该小地图的原尺寸物件光栅/合成也全部迁到 GPU**。示意图模式仍使用已有 CPU 缓存；导航、可见范围与原生雷达边界不变。

## GPU 管线

`NativeCanvas::areaPlan` 提供稀疏128×128页的 const vector 别名、页位置、变更版本，以及原始/目标尺寸和精确 span。没有先在 CPU 拼出整张大地图 RGBA。

1. 未变原生页复用常驻 GPU RGBA。变动页上传后，以两个 compute pass 构造4×4整数块的二维 summed-area table：33×33×3个uint，共13,068字节/页。
2. 输出像素按原来的 box/area 权重积分。整块区域用四角积分值；不能整除4的边缘使用原始像素条带，分数边缘在FP64中加权，最后只做一次RGB舍入。4×4是积分缓存粒度，不是把原图降成4×4色块。
3. 稀疏页孔洞使用原始背景RGB；结果沿用 NativeCanvas 完整地图的opaque RGB契约。缩小、放大或一轴缩小另一轴放大均按同一面积规则。只有完整地图 `present` 路线使用此后端；`presentRegion` 的透明越界、nearest/xBRZ语义不被替换。
4. 原生 SHROUD/FOG Plan 复用既有整数GPU Stage，在同一 command buffer 内写入紧凑遮罩，再进行最终变暗。每个新完成画面只提交一次、等待一个fence、最终读回RGBA一次；无逐页回读。遮罩超出GPU规划预算时可保留CPU规划的alpha并由GPU应用。
5. 无动态物件、修理标识、缺失素材点，且地形/光照/遮罩不变时，SceneRenderer 返回同一个完成Scene owner：不重新dispatch、不回读，也不触发小地图新前缓冲。GPU缓存命中不意味着每刷新都要重复GPU工作。

## 页身份、时效与预算

GPU source key同时比较vector地址、shared ownership和`Page::revision`。只比较弱引用owner不够：临时Plan销毁后，NativeCanvas可能仍唯一拥有该页并原地写入。每次原生页绘制在回调前递增revision，即使回调部分写入后分配失败也不会复用旧GPU内容。临时Plan在同步GPU提交期间持有强引用；常驻缓存只持弱引用，避免把上一帧所有原生页留住、耗尽CPU COW页预算。生产测试覆盖“同一canvas先呈现、原地画两像素、再呈现”，只重传/重建一页，并验证canvas释放后预算页数立即归零。

源cache规划按地址索引候选，再核对owner/version；不是每帧对每页线性扫描所有旧页并反复weak-lock。2048页上限对应128MiB常驻原始RGBA与约25.52MiB设备端积分buffer。metadata、上传staging、最终颜色与回读分别有界，并使用已有按实际VkMemoryRequirements计数的512MiB映射预算；这不是所有驱动显存/虚拟地址开销的总上限。GPU页数、设备能力或分配限制不满足时仍走原CPU面积滤波。GPU失败不降低分辨率，不无限重试设备初始化；无in-flight工作的分配失败先释放本后端buffer再CPU重放，超时仍按既有进程级隔离规则处理。

仅有double范围校验还不足以保证最外页安全：`(2688.0/19)*19`可比2688大一个ulp，直接ceil会访问不存在的第22列。主机编码的每个面积端点都与NativeCanvas::Filter一样夹到原生尺寸，再计算面积；实际GPU/ASan回归包含2688×128→19×3的整页外边界。

## 验证

- `assist/tests/native_area.cpp`：生产NativeCanvas CPU输出与独立直接面积积分比较，749,296个channel checks，最大误差0；覆盖稀疏页、分数span、大幅缩小、放大、混合轴、页外边界和非法plan保留原输出。
- `assist/tests/vulkan_area.cpp`：bootstrap **40801399801**，head **d3084cf33f25c18aa8e4a4785bdc1a2cb0fce56e**，job **123210095828**；在ASan/UBSan和Khronos validation下实际llvmpipe执行。32帧、3,507,092个channel checks最大误差0；生产整图SceneRenderer另有642,312个channel checks，断言逐通道误差≤1（夹具未单独输出该部分最大差值），涵盖静态Scene owner复用、SHP与CPU VXL原生合成、现场色盘变化、姿态变化、迷雾和CPU/GPU切换。未变source的实际GPU滤波测试为零上传、零积分重建、一次最终回读；生产静态图缓存为零dispatch/零回读。
- `assist/tests/vulkan_allocation_fallback.cpp`：扩展64KiB映射预算故障夹具，检查Area后端raw/prefix/metadata等资源归零、CPU稀疏面积重放与重复调用不再初始化；最终主CI结果单独记录。
- 实际shader由`tools/build_vulkan_area.py`使用官方glslang和spirv-val生成。源SHA256 `86372801cdc32acd3968c5aec29ef6c670c7b4d9e3c0a2ce20ad8c5be413b48d`，SPIR-V17564字节、SHA256 `b96963ec3595cf3d9df2f8d4482221fbbaa62470a17db8db747f14a5a62dc5f7`；没有占位shader。

本地无Vulkan ICD，只执行CPU回退和交叉编译；实际GPU运行证据来自上述软件设备CI。尚未测量AMD/NVIDIA/Intel真实显卡收益、DDraw/DWM最终呈现或游戏联机表现。现场读取、解码、原生小地图对象合成、标记文字、最终GDI窗口与一次GPU→CPU图像回读仍有CPU开销。最终可发布包必须对其同一源码提交重新执行完整CI，不能以bootstrap替代。
