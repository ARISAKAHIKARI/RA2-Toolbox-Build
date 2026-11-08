RA2 Toolbox 1.0.45

安装与更新
1. 完整退出游戏和工具箱，再替换整套程序。仅重启工具箱不能更换仍驻留在游戏进程中的旧 Runtime。
2. 已有完整安装：将 Update.zip 全部解压覆盖原目录，保留 Assist.ini、settings.ini、Toolbox.ini、已有素材、缓存和 runtime 文件夹。
3. 首次安装：使用 Full.zip，解压后通过 Start.cmd 或 RA2Toolbox.exe 启动，确认界面版本为 1.0.45。
4. 主程序、GameRuntime.dll、GameRuntimeHost32.exe、WebView2Loader.dll 和 engine 文件夹应全部使用本版配套文件。
需要 64 位 Windows 10/11 和 Microsoft Edge WebView2 Evergreen Runtime。

依赖按需下载
Full.zip 已移除体积较大的 Python、NumPy、SciPy 和 OpenCV 预处理环境。
正常启动工具箱、游戏辅助功能和播放随包提供的已预处理光标，不需要下载这些依赖。
首次使用“预处理补帧”，或导入需要预处理的新光标时，工具箱会检查本地环境，缺失时从 Python 官网和 PyPI 官方文件服务下载。
下载、校验和准备过程在后台进行，状态会显示在预处理区域；可取消，网络失败后再次点击“预处理补帧”即可重试。
依赖使用固定版本并校验 SHA256，准备完成后自动继续原来的预处理任务。已有可用环境会直接复用，完成后的光标也可离线播放。
环境保存在工具箱目录内，不需要全局安装 Python、pip 或修改系统环境变量。请将工具箱解压到当前用户可写的文件夹。
Update.zip 不删除旧完整安装的 runtime 文件夹，也不会自动清理已有光标和缓存。

本版继续包含 1.0.44 的 Broker、固定建造预定面板和 SHA/SHP 置灰修复。
程序文件版本为 1.0.45.0。发布附件中的 release-manifest.json 和 SHA256SUMS.txt 可用于核对下载包。
包内不含游戏本体、项目开发源码或私有转储；对应项目源码由本版 Git 标签提供。
