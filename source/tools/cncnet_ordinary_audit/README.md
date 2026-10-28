# 普通 CnCNetYR v17 兼容性复现

本目录是只读分析与受控 x86 执行工具，不会向运行中的游戏安装补丁，不分发引擎、DLL 或内存捕获。结论针对产品源码 `b372ace50101e0099a5e9533c50470b4cabb1536`、发布提交 `5508c80f7b81abad4fc51847eb7f3530acddc9af`，普通 DLL 必须匹配 SHA-256 `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`。

完整结论和限制见 [研究记录](../../research/2026-10-28-cncnet-ordinary-compatibility.md)。工具的 PASS 可能表示“缺陷按预期复现”，不能用总通过数替代结果内容；BLOCKED 是未覆盖，不能当作成功。

## 静态登记与动态写入

依赖 Python 3、g++。使用完整固定公开上游 `fbe8d7d2620584d04ba14fa3580943bfa1448d40` 的原始文件；工具逐项核对 69 个 Git blob SHA，不读取 Private 子模块。文件换行转换也会使验证拒绝。

```sh
python3 hooks/run_audit.py --repo /repo/RA2-Toolbox-Build --ordinary-dll /official/CnCNet-Spawner.dll --upstream-source /public/yrpp-spawner --output-dir /results/hooks
```

详情见 [hooks/README.md](hooks/README.md)。输出覆盖 39 个产品许可入口、245 个普通登记项、75 条公开动态写入、155 条 guard 声明／151 个范围。

## 受控执行

依赖 Python 3、pefile、Unicorn、Capstone。使用用户有权提供的引擎文件及发布 Runtime；调用现有仓库夹具并建模 Win32／世界，没有真实图形、socket 或双机任务执行。

- [相机与浮点](render/README.md)：实际普通 DLL 与 Runtime；两种引擎的相机和浮点路径均完成。
- [网络](network/README.md)：实际发布发送适配器、原生 codec、三个实际普通版事件长度导出；样本 3 完成，样本 4 的发送续行不满足前提并明确 BLOCKED。

普通相机 BSS 补丁生成还可独立复现：

```sh
python3 render/resolve_camera_patch.py /official/CnCNet-Spawner.dll --output /results/camera-constructor.json
```

它只执行固定官方普通 DLL 的一个 CRT 数据构造器，在两个模块基址验证生成的五字节跳转，不执行主机 DLL、不注入真实游戏。

本次结果不能推广到其他普通／Hardened 构建、其他插件组合或用户未提供的真实载入态。
