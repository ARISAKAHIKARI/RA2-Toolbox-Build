# 科技加強：精確本機科技等級攔截，不改指令／規則

本輪承接 `codex/console-architecture-20261103` 的 `4b6bb9cc59658876eb1e3a10835c85298036371b`（檔案版本明確是 `1.0.41-pre.1`），整合目前分支。已讀 AGENTS、INDEX、既有科技全開／資料還原 notes。Library notebook 未取得，新發現記於此及 evidence。私有 PE 僅唯讀，SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`，不隨 repo 發布。

## 使用者要的功能與實際邊界

科技加強原先不是武器／數值增強：它把21個指定類型的 Type+`0x634` TechLevel 暫寫為1；普通項僅原值-1時改，LUNR另包含2..100。此規則資料會被其他玩家／AI共用，舊模式仍標高危並保留原值／身份 journal。

新的獨立選項是「精确科技等级（不改指令／规则）」：在本機兩個原生側欄 CanBuild 查詢和受控補圖示探查中，虛擬讀成 TechLevel=1，不寫遊戲 `.text`、vtable、Type／House規則或原 CALL 的參數，不用工具箱清單假冒原生面板。不是把所有AI或其他模擬內的 TechLevel 消費者一起改成1。條件仍不滿足的特殊類型不會憑空可生產。

這與目前全開 mode4 有明確區別。全開在 `0x6A97D2/0x6AA781` CALL 前把 buildLimitOnly 改true，跳過整段科技／前置／國別／偷科技／Unbuildable 檢查。新加強只虛擬科技等級，保留那些條件、真正工廠、OwnerMask、BuildLimit及原 includeProduction。兩者同開且某類型被全開選中時，該類型仍遵循更強的全開權限，不能說組合後仍只改等級。

## 精確原生 ABI

`House::CanBuild 0x4F7870` 為 thiscall(Type*,bool buildLimitOnly,bool includeProduction)。新模式在完整原函式的兩個 post-CMP Jcc 使用 hardware execution breakpoint：

| 原生比較 | 斷點位置／處理 | 門檻 |
| --- | --- | --- |
| `0x4F794A` 比 Type+634 與 -1 | `0x4F7951` 原 JNE 前，算術 flags 視為 CMP(1,FFFFFFFF) | 只解除指定類型的負等級禁建 |
| `0x4F7B60` 比 Type+634 與 House 科技等級 EBX | `0x4F7B66` 原 JG 前，flags 視為 CMP(1,EBX) | House科技0仍會拒絕，不是無條件放行 |

此時兩點棧配置一致：`ESP+10` saved House、`+4C` 原return、`+50` Type、`+54` buildLimitOnly、`+58` includeProduction。第一點 Type在EDI，第二點在ECX，House在EBP；其他GPR、EIP、棧及原Jcc不改。只更換算術 EFLAGS mask `0x8D5`（CF/PF/AF/ZF/SF/OF），保留TF/RF/DF/IF等，包括VEH自身恢復執行協議。

原生UI return僅 `0x6A97D7`（include0）／`0x6AA786`（include1）。另准許 Runtime 自己的23-byte naked CanBuild probe：逐byte驗證該編譯stub，return唯一為+20，須已有Assist Capture/Execute lease與有界probe-depth/token。SEH `__finally` 回收深度及保存LastError。VEH callback不呼引擎、配置、記錄或等待；probe的引擎調用在正常Runtime入口。

預查範圍：`0x4F794A` 13byte FNV1a=`0FC7BB79`，`0x4F7B5C` 16byte=`15DAD6B8`，CanBuild入口16byte=`6995B50E`；全開兩CALL原簽名仍保留。模式還核對同Scenario/Session、Frame未回退、Running、本機House指標/UID與權限、21類型array/index/UID/vtable及原等級。未知版本或第三方修改失敗關閉，不聲稱能適用所有MOD。

21類型沿用原名單：WEEDGUY/COW/ALL/PRES/JOSH/POLARB/CLNT/ARND/STLN/LUNR、BUS/HOWI/CMON/HIND/XCOMET/VLAD/CDEST/SCHD、V3ROCKET/CMISL/DMISL。只讀目錄在對應白名單負等級時採effective1，仍過原 insignificant／spawned／owners0篩選。補圖示先做完整原生 CanBuild虛擬等級查詢及真 FindFactory，最後用原生AddCameo，保留原生排序、去重和75項/頁上限。不保證每個特殊飛彈或缺素材類型都會出現。

## 四槽所有權與模式交接

全開用兩個UI CALL槽，加強用兩個Jcc槽，地址不同。單獨加強2槽、同開4槽；由一個Service／VEH統一管理，不以第二個Service搶singleton。每次配置先完整預查free槽，已有非零地址／任何DR7槽控制位都當foreign。新增組槽不足時，交易沒有context寫入；保留已驗證的既有組及權限。支援部分移除，已有全開兩槽不因關加強被重裝。槽不足狀態保留到真正成功，既有關閉組不再每tick重設別組retry／反覆建helper。

SetThreadContext回傳成功不足以宣告完成。讀回失敗時，對重用同一物理槽的old/new兩候選tag都保留lease/VEH；handler按實際Dr地址選tag，未確認不能宣告ready或完成stop。若API假成功，讀回仍舊組，保持實際舊lease並報錯，後續只撤自己的有效地址。foreign替換不覆寫，混合foreign Dr6/TF/BD/BT時完整CONTEXT原樣傳遞。切屏不拆槽；stop acknowledgement等所有組、helper、handler真正退休。

另修正獨立review找到的真診斷缺口：[修前compiled反例](evidence/tech-enhancement-parameter-publish-before-fix.json)已確認DR清零、但RemoveVectoredExceptionHandler失敗且VEH仍留時，畫面錯報Off。嚴格stop/lease退休原本仍有保留，沒有據此放寬。現在以resourceGroups歸屬與pendingFor(group)區分本組真正退休、未知cleanup和正常共享另一組；保留System錯誤／Waiting到真的完成。從未啟用的另一組不背錯誤，健康Full仍持有共用handler也不妨礙Enh顯示正常Off。永久[真Windows before14/current15](evidence/tech-ui-publish-windows.json)使用真Service/DR及實際publish抽取，只有RemoveVEH結果為故障注入；Runtime另有三組驗證實際resourceGroups維護。

精確→legacy切換如有依賴精確權限的本機產品／隊列，保持舊權限並顯示-7，完成／取消後才撤DR再恢復legacy writer。普通原生可建產品不一律擋住切換。Data3或舊text科技模式與精確加強明確互斥，先確認本組退休，不默默改回規則；舊全開改成mode4後可共同用4槽。

本輪額外找出正向legacy→精確的真空窗：[修前反例](evidence/tech-enhancement-parameter-handoff-before-fix.json)在已編譯舊DLL的合成BUS在製狀態下，第一個切換tick Type已從1還原-1，新DR數仍0，產品仍存在。它是權限空窗觀察，沒有執行native prune／Abandon，更不是斷線實測。修正後，legacy journal擁有的本機active／queued類型先保留原Type1與journal，狀態-7、尚無新槽；完成才還原並異步切換。進入全開Data/Parameters的-43守衛亦識別legacy擁有的Type，避免同類空窗。等待期間仍是舊規則模式，不能報新無規則模式已就緒。

## UI／協議

9097總開關保留；9470選0「高危：规则数据（旧版）」或1「实验性：精确科技等级（不改指令／规则）」；9471顯示明確實際狀態。INI `Experimental/TechEnhanceImplementation` 以0/1存取，讀取clamp，選擇後立即Shared同步。default0保留舊設定行為。

Assist92保持既有欄位offset，補明確Assist91尾端padding後追加 `uint32 techEnhanceImplementation`、`int32 techEnhanceParameterStatus`、`uint32 techEnhanceParameterError/Slots/Exceptions`。同輪另有MO Options，最終Shared=`6584992`，兩ABI781項offset相符由整合層驗證。status0off、1ready、2pending、-1簽名、-2槽不足、-3WindowsAPI/讀回、-4foreign、-5thread、-6舊科技／規則衝突、-7等待依賴舊權限的生產。Slots是已確認本組數量，不用0推斷未知lease已清乾淨。Exceptions是本組已確認歸屬的硬體命中，包含分類拒絕，不等於成功生產數。

## 驗證證據與範圍

- [Compiled Runtime28組](evidence/tech-enhancement-parameter-runtime.json)：原生private CanBuild本體＋實際compiled flags callback；SP/MP合成世界、禁建/國別/三偷科技/BuildLimit、缺／真先決、工廠、兩方向槽不足保留既有組、四槽／局部關閉、正反向在產及queued交接、普通產品不誤擋、FullData/Parameters busy守衛、stop、切屏、身份／caller／簽名、兩組各自VEH撤回失敗可見與正常共享handler不誤報。Win32 DR dispatch明確模擬。受測DLL SHA `0588081dea0ae72f6f9152e9e21dd09d6d348ef3e8d30a1b96e79b1c7071ba2b`、PDB `3aa50159a2b7a377c011c65650de3b872b977cc433364b4209c70eef1e5dcf26`，並核對測前後source/DLL/PDB/privatePE SHA穩定。
- [真Windows四槽13類](evidence/tech-enhancement-parameter-windows.json)：同一production Core，自有獨立x86程序，真DR命中、原Jcc旗標、foreign槽、擴槽拒絕保舊組、部分移除、same-slot模糊讀回、假Set及最終完整退休。每輪精確60000命中，三輪；QPC數字見原始報告；背景構建與驗證負載不受控，這是簡化fixture，不是完整classifier或遊戲FPS。
- [真SEH7類×10程序](evidence/tech-enhancement-parameter-seh.json)：精確product guard，success／真PAGE_NOACCESS AV／巢狀AV／拒絕／深度16、finally LIFO及LastError。admission/引擎callback為模型，沒碰遊戲。
- [31項原生等級對照](evidence/tech-enhancement-parameter-native.json)：兩UI return/include和LUNR類Infantry，把subject的flags攔截與另一合成TypeLevel1 reference逐項對照，保留原生門檻，subject Type/House/text不變。
- [9470／INI／Shared](evidence/tech-enhancement-parameter-settings.json)：抽取實際branch與load/save/sync，Windows x86/x64各22check；UI/IO是有界模型。
- 舊全開14組、真Windows原兩CALL13類、legacy世界還原、Data世界21、Data切換38、全目錄refresh54保留，完整source/build binder由本輪整合證據補充。

重跑五個新runner：`tech_enhancement_parameter_runtime.py <game> <dll> --output <json>`；`tech_enhancement_parameter_native.py <game> --output <json>`；其餘 `_windows.py`、`_seh.py`、`_settings.py` 都用 `--zig <zig> --build-dir <dir> --output <json>`。所有assert-enabled fixture拒絕NDEBUG。

本模式仍是硬體例外攔截；「沒改指令／規則」不等於沒有Hook、沒有額外成本、平台一定接受或聯機已證實。未對使用者遊戲程序安裝DR、未啟動遊戲、未做RA2OL雙機／真實側欄視覺／生產體感驗收，也沒有隱藏debug registers、攔截平台檢查或規避檢測的實作。官方機制參考 [Windows VEH](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler) 與 [CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context-r2)。
