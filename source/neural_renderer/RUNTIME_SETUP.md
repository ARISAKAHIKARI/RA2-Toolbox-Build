# DLSS 5 Neural Rendering 运行时

更新包不捆绑 NVIDIA 的大型 NR DLL。关闭工具箱及其渲染助手后，将 `Install-DLSS5Runtime.ps1` 放在工具箱 EXE 同目录，在 PowerShell 中运行：

```powershell
.\Install-DLSS5Runtime.ps1
```

也可以指定安装位置或已有的 7-Zip：

```powershell
.\Install-DLSS5Runtime.ps1 -InstallDirectory 'D:\RA2Toolbox' -SevenZipPath 'C:\Program Files\7-Zip\7z.exe'
```

脚本从 NVIDIA RTX Remix 的公开依赖地址获取固定版本包，只提取并安装 x64 `nvngx_dlssnr.dll` 和随附的 `LICENSE.txt`，放入工具箱旁的 `neural-runtime` 目录。它会校验包与文件的大小、SHA-256、PE 架构及 NVIDIA 的有效数字签名；不运行下载的 DLL，不修改配置或游戏文件。Windows 自带的 `tar.exe` 无法读取该包时会尝试已安装的 7-Zip。

下载约351 MiB；安装后的 DLL 约254 MiB。已有的同版本官方包可以用 `-ArchivePath` 指定，仍会执行全部校验。失败时保留现有 DLL，并尝试恢复已替换的许可文件。

运行时受其随附的 NVIDIA RTX SDK 许可约束，不是本工具箱自行开发的模型，也不代表 NVIDIA 对本项目的背书。脚本安装成功只表示依赖就绪；显卡、驱动及实际 NR 调用通过后，工具箱才能报告 DLSS 5 生效。
