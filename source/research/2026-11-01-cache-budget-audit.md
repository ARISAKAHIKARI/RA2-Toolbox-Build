# 快取上限、計費與淘汰策略審計

使用者質疑各處容量設定過低。本輪只讀檢查，沒有修改容量常數，也沒有把當時可用記憶體當成使用者硬體性能的評價。已完成的姿態自鎖修復見[獨立重現與修復](2026-11-01-pose-cache-transition-liveness.md)。

`external_reader/source/render_memory_budget.h` 的 x64 `TotalBytes=8 GiB` 沒有使用點，並非已配置的記憶體，也不是正在執行的全域配額。各局部上限分別生效，所有權有程序共享、每視圖、每 Context 與每執行緒之別；不能把這些上限直接相加當作 resident。

| 部分 x64 上限 | 所有權／淘汰 |
| --- | --- |
| NativePages 約 3 GiB | 跨畫布共享，按需配置；不是每張小地圖預配置 3 GiB |
| SharedImages 768 MiB | 程序共享 LRU；已發布畫面仍可能持有被移出快取的 Image |
| SharedTiles 256 MiB | 到額度時清空索引；取出後的本地副本另計 |
| 本地 TMP 16M 像素／4096 項 | 未隨架構調整；超像素額度拒絕新素材並延後重試，超項數整庫清空 |
| Geometry 256 MiB、PosePixels 32M 計費單位 | 每 SceneRenderer；PosePixels 不是 32 MiB，GPU 三角形／palette／attachments 也折算其中 |
| VXL Prepared 128 MiB | 每 Context；與共享素材庫可能重複邏輯計費同一共享物件 |
| Layers 64 MiB、UI SHP 32 MiB／256 項、alpha 16 MiB | 保留較小的固定額度；各有整庫清空路徑 |
| 原始檔案 64 MiB | 每 Context LRU，命中仍可能複製資料 |

GPU 額度另管：scene RGBA atlas 128 MiB、material atlas 96 MiB、xBRZ raw atlas 96 MiB；x64 原生地形 atlas 可由 128 增至 512 MiB。MappedBudget 768 MiB 是每 backend，`gpuState()` 為 thread_local，不等於整個 Host 的顯卡記憶體總額。未找到依可用 VRAM 動態分配上述額度的策略。

因此，部分局部額度和淘汰方式確實不協調；但舊實機日誌沒有完整 hit/miss、拒絕與清空計數，不能宣稱它已證明某個小額度是本次卡頓根因。PosePixels 的已重現問題是舊、新姿態相互等待：加大上限只會移動觸發點，不能代替已落地的回收修復。

後續容量調整應先把 SharedTiles、本地 TMP、Layers、UI SHP、alpha 的整庫清空改成保留近期使用素材的淘汰，並記錄容量拒絕、重建、已逐出但仍被畫面持有的資源。再以實際 bytes 和唯一 owner 統一計費，為 CPU 提供單一總預算與可用記憶體壓力回收；GPU 使用裝置實際 budget 另行約束。這些是本輪審計建議，尚未冒稱已實作。

8192 輸出維度、16M 輸出像素、世界座標範圍及畸形 VXL／指令工作量限制是另一類防護，不能當成普通快取容量一起翻倍。純顯示快取的調整也不因配置記憶體而成為聯機模擬寫入。
