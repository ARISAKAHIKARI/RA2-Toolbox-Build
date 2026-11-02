#include "../../shared/source/key_gesture.h"
static void loadAdditionalSettings(){
 superweaponOptions.detached=value(L"Display",L"SuperweaponDetached",0)!=0;superweaponOptions.collapsed=value(L"Display",L"SuperweaponCollapsed",0)!=0;
 superweaponOptions.x=std::clamp(value(L"Display",L"SuperweaponX",20),0,32768);superweaponOptions.y=std::clamp(value(L"Display",L"SuperweaponY",150),0,32768);
 superweaponOptions.scale=std::clamp(value(L"Display",L"SuperweaponScale",100),75,200);superweaponOptions.rows=std::clamp(value(L"Display",L"SuperweaponRows",8),1,20);
 // Older releases persisted these two implicit defaults as if the user had
 // opted in. Migrate once; subsequent explicit high-risk choices are retained.
 const bool explicitPatchPreferences=value(L"Compatibility",L"NoCodeWriteDefaultsVersion",0)>=1;
 buildTimeTooltipEnabled=explicitPatchPreferences&&value(L"Display",L"BuildTimeTooltip",0)!=0;harvesterCounterEnabled=value(L"Display",L"HarvesterCounter",0)!=0;powerDeltaEnabled=value(L"Display",L"PowerDelta",0)!=0;
 renderProcessing=value(L"Display",L"RenderProcessing",value(L"Display",L"RenderExtraQuality",0))!=0;
 renderXbrzEnabled=value(L"Display",L"RenderXbrzEnabled",0)!=0;
 renderColorPrecision=std::clamp(value(L"Display",L"RenderColorPrecision",1),0,1);
 renderHighRefresh=value(L"Display",L"RenderHighRefresh",1)!=0;
 renderMaterialRefresh=value(L"Display",L"RenderMaterialRefresh",1)!=0;
 renderSceneDetail=std::clamp(value(L"Display",L"RenderSceneDetail",1),0,1);
 renderUpgradeEnabled=value(L"Display",L"RenderUpgrade",0)!=0;
 unifiedOverlayEnabled=value(L"Display",L"UnifiedOverlay",0)!=0;
 renderUpgradeLayer=std::clamp(value(L"Display",L"RenderUpgradeLayer",1),0,1);
 renderUpgradeLayer=1; // Migrate the old opaque-on-top mode; information and input feedback must remain visible.
 renderPaletteRange=std::clamp(value(L"Display",L"RenderPaletteRange",1),0,1);
 renderPixelScale=std::clamp(value(L"Display",L"RenderPixelScale",3),0,3);
 renderPhysicalOutput=value(L"Display",L"RenderPhysicalOutput",0)!=0;
 renderVulkanEnabled=value(L"Display",L"RenderVulkan",1)!=0;
 renderParticleEffects=value(L"Display",L"RenderParticles",1)!=0;renderLaserEffects=value(L"Display",L"RenderLasers",1)!=0;
 renderVoxelLighting=value(L"Display",L"RenderVoxelLighting",1)!=0;renderSpriteLighting=value(L"Display",L"RenderSpriteLighting",1)!=0;
 extra.viewZoomEnabled=value(L"ViewZoom",L"Enabled",0)!=0;extra.viewZoomPercent=value(L"ViewZoom",L"Percent",100);if(!viewzoom::valid(extra.viewZoomPercent))extra.viewZoomPercent=100;
 extra.moFeatures=uint32_t(value(L"MOControls",L"Features",0))&23u;extra.superweaponPanel=value(L"Display",L"SuperweaponPanel",0)!=0;
 const wchar_t*gestureNames[]={L"FuzzyOrder",L"AreaOrder",L"PatrolCircle",L"FormationQuick"};
 for(int i=0;i<moorders::GestureCount;++i){auto hold=std::wstring(gestureNames[i])+L"HoldKey",button=std::wstring(gestureNames[i])+L"MouseButton";
  extra.moGestures.keys[i]=uint32_t(value(L"Mouse",hold.c_str(),extra.moGestures.keys[i]))&65535;
  int physical=std::clamp(value(L"Mouse",button.c_str(),1),0,6);extra.moGestures.buttons[i]=physical==5?0:uint32_t(physical);}
 const wchar_t*zoomKeys[]={L"ViewZoomOut",L"ViewZoomIn",L"ViewZoomReset"};for(int i=0;i<3;++i)extra.viewZoomKeys[i]=uint32_t(value(L"Hotkeys",zoomKeys[i],extra.viewZoomKeys[i]))&65535;
 extra.smallMapClamp=value(L"Experimental",L"SmallMapClamp",0)!=0;
 extra.sidebarEnabled=value(L"Experimental",L"SidebarEnabled",0)!=0;extra.sidebarMode=value(L"Experimental",L"SidebarMode",0)==2?2u:0u;
 extra.sidebarKey=uint32_t(value(L"Hotkeys",L"ToggleSidebar",extra.sidebarKey))&65535;extra.pauseKeys[0]=uint32_t(value(L"Hotkeys",L"PauseToggle",extra.pauseKeys[0]))&65535;extra.pauseKeys[1]=uint32_t(value(L"Hotkeys",L"PauseStep",extra.pauseKeys[1]))&65535;
 extra.techEnhance=value(L"Experimental",L"TechEnhance",0)!=0;
 extra.crcMapShadow=value(L"Experimental",L"CrcMapIsClearShadow",0)!=0;
 extra.networkHousePreserve=value(L"Experimental",L"NetworkHousePreserve",0)!=0;
 extra.nativeSellCursor=value(L"Experimental",L"NativeSellCursor",0)!=0;
 extra.nativeMegaGateway=explicitPatchPreferences&&value(L"Experimental",L"NativeMegaGateway",0)!=0;
 extra.nativeCombatQueries=value(L"Experimental",L"NativeCombatQueries",0)!=0;
 extra.controlCommandOwner=value(L"Experimental",L"ControlCommandOwner",0)!=0;
 // Replace the former flag implementation in slot 0; retain the native slot
 // and saved companion preferences, which remain suppressed in Master mode.
 extra.controlImplementation=value(L"Experimental",L"ControlImplementation",0)==1?1:0;
 extra.placementImplementation=buildingmodes::placement(value(L"Experimental",L"PlacementImplementation",0));
 extra.placementEventKey=value(L"Hotkeys",L"PlaceCompletedBuilding",extra.placementEventKey)&65535;
 extra.overlayKey=value(L"Hotkeys",L"OverlayToggle",0xC0)&65535;
 extra.stickyKey=value(L"Hotkeys",L"StickyGuard",extra.stickyKey)&65535;
 for(int i=0;i<selectedmode::Count;++i)extra.missionModeKeys[i]=value(L"Hotkeys",selectedmode::setting(i),0)&65535;
 extra.watchSecondary=value(L"Watch",L"SecondaryEnabled",0)!=0;
 extra.watchSecondaryX=std::clamp(value(L"Watch",L"SecondaryX",10000),0,10000);extra.watchSecondaryY=std::clamp(value(L"Watch",L"SecondaryY",0),0,10000);
 extra.watchEnabled=value(L"Watch",L"Enabled",0)!=0;extra.watchDanger=value(L"Watch",L"DangerCarriers",0)!=0;for(int n=0;n<3;++n){const wchar_t* keys[]={L"PinKey",L"PinKey2",L"PinKey3"};extra.watchKeys[n]=value(L"Watch",keys[n],n?extra.watchKeys[0]:extra.watchKeys[n])&65535;}
 extra.formationEnabled=value(L"Formation",L"Enabled",0)!=0;extra.formationHold=value(L"Formation",L"HoldKey",18)&65535;extra.formationMouse=value(L"Formation",L"MouseButton",2)==1?1:2;
 const wchar_t* shapeKeys[]={L"RectangleKey",L"DispersedKey",L"CircleKey",L"FanKey",L"SquareKey",L"FrameKey"};for(int n=0;n<6;++n)extra.formationKeys[n]=value(L"Formation",shapeKeys[n],extra.formationKeys[n])&65535;
 for(int i=0;i<compute::Count;++i){int fallback=compute::inside(compute::Default,compute::Feature(i));
  if(i==compute::MagnetronGlobal||i==compute::MagnetronDrop)fallback=compute::inside(computeRequested,compute::Magnetron);
  bool mode=value(L"Compute",compute::key(i),fallback)!=0;if(mode)computeRequested|=1u<<i;else computeRequested&=~(1u<<i);}
 idleAircraftKey=WORD(value(L"Hotkeys",L"IdleAircraft",MAKEWORD('P',HOTKEYF_CONTROL|HOTKEYF_ALT))&65535);
 minimapPosition=std::clamp(value(L"Display",L"MinimapPosition",0),0,3);minimapX=std::clamp(value(L"Display",L"MinimapX",20),0,32768);minimapY=std::clamp(value(L"Display",L"MinimapY",60),0,32768);
 minimapAlignX=std::clamp(value(L"Display",L"MinimapAlignX",0),0,2);minimapAlignY=std::clamp(value(L"Display",L"MinimapAlignY",0),0,2);
 minimapMode=std::clamp(value(L"Display",L"MinimapMode",0),0,1);
 minimapSecondary=value(L"Display",L"MinimapSecondary",0)!=0;
 minimapSecondaryX=std::clamp(value(L"Display",L"MinimapSecondaryX",10000),0,10000);minimapSecondaryY=std::clamp(value(L"Display",L"MinimapSecondaryY",10000),0,10000);
 minimapSecondaryWidth=std::clamp(value(L"Display",L"MinimapSecondaryWidth",320),160,4096);
 minimapEnabled=value(L"Display",L"MinimapZoom",0)!=0;minimapScale=std::clamp(value(L"Display",L"MinimapScale",10),5,200);
 for(int i=0;i<featuretypes::Count;++i){auto&f=featureTypes.filters[i];std::wstring custom=std::wstring(featuretypes::key(i))+L"Custom";wchar_t text[13000]{};
  GetPrivateProfileStringW(L"FeatureTypes",featuretypes::key(i),featuretypes::initial(i),text,13000,path(L"Assist.ini").c_str());featureTypeText[i]=text;
  oreconfig::parse(featureTypeText[i],f.ids,f.count);f.custom=value(L"FeatureTypes",custom.c_str(),0)!=0;
 }

 extra.dodgeEnabled=value(L"Dodge",L"Enabled",0)!=0;extra.dodgeMode=std::clamp(value(L"Dodge",L"Logic",0),0,1);
 extra.burstEnabled=extra.burstHoldEnabled=false;extra.burstKey=0; // Retired: ignore old INI values.
 extra.missileScope=std::clamp(value(L"MissileDefense",L"TargetScope",0),0,1);extra.missileAllocation=std::clamp(value(L"MissileDefense",L"Allocation",0),0,1);extra.magnetronGlobal=value(L"Magnetron",L"Global",0)!=0;
 extra.missileDefense=value(L"MissileDefense",L"Enabled",0)!=0;extra.missileScanHz=std::clamp(value(L"MissileDefense",L"ScanHz",10),1,60);
 extra.missileReserve=value(L"MissileDefense",L"SleepReserve",1)!=0;extra.lowDamageFilter=value(L"Combat",L"FilterLowDamage",0)!=0;extra.lowDamageNetwork=value(L"Combat",L"NativeFilterInMultiplayer",0)!=0;
 extra.antiTheft=value(L"Modes",L"AntiTheft",0)!=0;
 extra.scrollRefreshMode=0; // only cursor refresh is available in this release
#ifndef RA2_UNIFIED
 extra.scrollRefreshHz=std::clamp(value(L"Experimental",L"ScrollRefreshHz",120),30,500);
#endif
 extra.c4Test=featureavailability::InfantryForcedEntry&&value(L"Experimental",L"InfantryC4",0)!=0;
 extra.c4Mode=uint32_t(std::clamp(value(L"Experimental",L"InfantryEntryMode",0),0,1));
 extra.c4Key=value(L"Experimental",L"C4HoldKey",'C'|(4<<8));extra.c4Mouse=std::clamp(value(L"Experimental",L"C4Mouse",1),0,6);if(extra.c4Mouse==5)extra.c4Mouse=1;
 for(int i=0;i<9;++i)socialKeys[i]=WORD(value(L"Hotkeys",i?(L"Taunt"+std::to_wstring(i)).c_str():L"Cheer",0)&65535);
 extra.mapStrategy=shroudpolicy::savedStrategy(value(L"Display",L"ShroudStrategy",shroudpolicy::legacyStrategy(value(L"Experimental",L"RenderReveal",0))));
 extra.renderReveal=shroudpolicy::runtimeMode(extra.mapStrategy);
 extra.manualReveal=value(L"Experimental",L"ManualReveal",value(L"Experimental",L"RevealMap",0))!=0;
 extra.experimentalReveal=shroudpolicy::revealEnabled(extra.manualReveal,extra.mapStrategy);
 extra.nativeAirWaypoints=value(L"Experimental",L"NativeAirWaypoints",0)!=0;
 extra.airWaypoints=value(L"Aircraft",L"ZWaypoints",0)!=0;
 int pathMode=std::clamp(value(L"Aircraft",L"WaypointMode",extra.nativeAirWaypoints?2:extra.airWaypoints?1:0),0,4);extra.airWaypoints=pathMode==1;extra.nativeAirWaypoints=pathMode>=2;extra.nativeAirNetwork=pathMode>=3?uint32_t(pathMode-2):0;extra.airPathKey=uint32_t(value(L"Aircraft",L"WaypointHoldKey",0));
 extra.productionReminderMask=uint32_t(value(L"Production",L"ReminderLines",0))&63;
 extra.constructionQueue=value(L"Production",L"ReserveNext",0)!=0;
 extra.idleAircraft=value(L"Display",L"IdleAircraftIndicator",0)!=0;
 extra.parasiteMarker=value(L"Display",L"ParasiteMarker",0)!=0;extra.crateBuffMarkers=value(L"Display",L"CrateBuffMarkers",0)!=0;
 extra.passengerBubbles=value(L"Display",L"PassengerBubbles",0)!=0;extra.dangerPassengers=value(L"Display",L"DangerPassengers",0)!=0;extra.magnetronDrop=value(L"Magnetron",L"AutoDrop",0)!=0;
 extra.enemyMarkers=value(L"Display",L"EnemyMarkers",0)!=0;
 extra.overrideNative=value(L"Mouse",L"OverrideNative",0)!=0;
 extra.allHotkeys=value(L"Hotkeys",L"ExecuteAllMatches",0)!=0;
 // Missing per-feature values inherit the old category setting exactly once.
 for(int i=0;i<pointerinput::MouseCount;++i){
  const wchar_t*legacy=i<pointerinput::MouseAttack?L"EnhancedButton":i==pointerinput::MouseAttack?L"AttackMoveButton":L"SellButton";
  int fallback=std::clamp(value(L"Mouse",legacy,pointerinput::mouseDefault(i)),0,6);
  extra.mouseButtons[i]=std::clamp(value(L"Mouse",pointerinput::mouseSetting(i),fallback),0,6);
 }

 extra.loadMode=std::clamp(value(L"AutoLoad",L"Logic",0),0,1);extra.loadRadius=std::clamp(value(L"AutoLoad",L"RangeCells",30),1,512);
 extra.enter=value(L"Cooperative",L"AdvancedEnter",0)!=0;extra.air=value(L"Cooperative",L"AircraftSpread",0)!=0;
 extra.aircraftAllocationPolicy=std::clamp(value(L"Cooperative",L"AircraftAllocationPolicy",0),0,2);
 extra.enterKey=value(L"Cooperative",L"EnterHoldKey",18)&65535;extra.airKey1=value(L"Cooperative",L"AirHoldKey1",17)&65535;extra.airKey2=value(L"Cooperative",L"AirHoldKey2",0)&65535;

 extra.diskKey=value(L"SpecialClick",L"DiskHoldKey",17)&65535;extra.sellKey=value(L"SpecialClick",L"SellHoldKey",17)&65535;
 extra.disk=value(L"SpecialClick",L"DiskPrimary",0)!=0;extra.sellDrained=value(L"SpecialClick",L"SellDrained",0)!=0;extra.sellVehicles=value(L"SpecialClick",L"SellVehicles",0)!=0;const bool sellOptIn=value(L"Experimental",L"EnemyBuildingSellOptInVersion",0)>=1;extra.sellEnemy=playertest::SellAvailable&&sellOptIn&&value(L"SpecialClick",L"SellEnemyBuildings",0)!=0;extra.controlAll=playertest::Available&&value(L"Experimental",L"ControlAll",0)!=0;extra.switchPlayerEnabled=false;
 extra.frozen=value(L"Display",L"Frozen",0)!=0;extra.variantKey=value(L"TypeSelection",L"VariantKey",'T'|(2<<8))&65535;
 extra.defaults=cooperative::bounded({value(L"AircraftQuota",L"First",1),value(L"AircraftQuota",L"Second",2),value(L"AircraftQuota",L"Maximum",0)});
 wchar_t entries[32768]{};GetPrivateProfileSectionW(L"AircraftQuotaOverrides",entries,32768,path(L"Assist.ini").c_str());
 for(wchar_t*p=entries;*p;p+=wcslen(p)+1){std::wstring line=p;size_t e=line.find(L'=');if(e==std::wstring::npos)continue;std::string id;for(size_t j=0;j<e;++j)if(line[j]>32&&line[j]<127)id+=char(line[j]);if(!typecatalog::validId(id)||targetQuotas.size()>=512)continue;
  int a,b,c;if(swscanf(line.c_str()+e+1,L"%d,%d,%d",&a,&b,&c)!=3)continue;targetQuotas[id]=cooperative::bounded({a,b,c});
 }
}
static bool saveAdditionalSettings(){
 write(L"Display",L"SuperweaponDetached",superweaponOptions.detached);write(L"Display",L"SuperweaponCollapsed",superweaponOptions.collapsed);write(L"Display",L"SuperweaponX",superweaponOptions.x);write(L"Display",L"SuperweaponY",superweaponOptions.y);write(L"Display",L"SuperweaponScale",superweaponOptions.scale);write(L"Display",L"SuperweaponRows",superweaponOptions.rows);
 const wchar_t*gestureNames[]={L"FuzzyOrder",L"AreaOrder",L"PatrolCircle",L"FormationQuick"};
 for(int i=0;i<moorders::GestureCount;++i){auto hold=std::wstring(gestureNames[i])+L"HoldKey",button=std::wstring(gestureNames[i])+L"MouseButton";write(L"Mouse",hold.c_str(),extra.moGestures.keys[i]);write(L"Mouse",button.c_str(),extra.moGestures.buttons[i]);}
 const wchar_t*zoomKeys[]={L"ViewZoomOut",L"ViewZoomIn",L"ViewZoomReset"};for(int i=0;i<3;++i)write(L"Hotkeys",zoomKeys[i],extra.viewZoomKeys[i]);
 write(L"Hotkeys",L"ToggleSidebar",extra.sidebarKey);write(L"Hotkeys",L"PauseToggle",extra.pauseKeys[0]);write(L"Hotkeys",L"PauseStep",extra.pauseKeys[1]);
 write(L"Experimental",L"SmallMapClamp",extra.smallMapClamp);
 write(L"Experimental",L"SidebarEnabled",extra.sidebarEnabled);write(L"Experimental",L"SidebarMode",extra.sidebarMode);
 write(L"MOControls",L"Features",extra.moFeatures&23u);write(L"Display",L"SuperweaponPanel",extra.superweaponPanel);
 write(L"Compatibility",L"NoCodeWriteDefaultsVersion",1);
 write(L"ViewZoom",L"Enabled",extra.viewZoomEnabled);write(L"ViewZoom",L"Percent",extra.viewZoomPercent);
 write(L"Experimental",L"TechEnhance",extra.techEnhance);write(L"Experimental",L"PlacementImplementation",extra.placementImplementation);
 write(L"Hotkeys",L"PlaceCompletedBuilding",extra.placementEventKey);
 write(L"Experimental",L"CrcMapIsClearShadow",extra.crcMapShadow);
 write(L"Experimental",L"NetworkHousePreserve",extra.networkHousePreserve);write(L"Experimental",L"NativeSellCursor",extra.nativeSellCursor);
 write(L"Experimental",L"NativeMegaGateway",extra.nativeMegaGateway);
 write(L"Experimental",L"NativeCombatQueries",extra.nativeCombatQueries);
 write(L"Experimental",L"ControlCommandOwner",extra.controlCommandOwner);
 write(L"Experimental",L"ControlImplementation",extra.controlImplementation);
 write(L"Hotkeys",L"OverlayToggle",extra.overlayKey);
 write(L"Hotkeys",L"StickyGuard",extra.stickyKey);
 for(int i=0;i<selectedmode::Count;++i)write(L"Hotkeys",selectedmode::setting(i),extra.missionModeKeys[i]);
 write(L"Watch",L"SecondaryEnabled",extra.watchSecondary);
 write(L"Watch",L"SecondaryX",extra.watchSecondaryX);write(L"Watch",L"SecondaryY",extra.watchSecondaryY);
 write(L"Watch",L"Enabled",extra.watchEnabled);write(L"Watch",L"DangerCarriers",extra.watchDanger);for(int n=0;n<3;++n){const wchar_t* keys[]={L"PinKey",L"PinKey2",L"PinKey3"};write(L"Watch",keys[n],extra.watchKeys[n]);}
 write(L"Formation",L"Enabled",extra.formationEnabled);write(L"Formation",L"HoldKey",extra.formationHold);write(L"Formation",L"MouseButton",extra.formationMouse);
 const wchar_t* shapeKeys[]={L"RectangleKey",L"DispersedKey",L"CircleKey",L"FanKey",L"SquareKey",L"FrameKey"};for(int n=0;n<6;++n)write(L"Formation",shapeKeys[n],extra.formationKeys[n]);
 write(L"Display",L"PassengerBubbles",extra.passengerBubbles);write(L"Display",L"DangerPassengers",extra.dangerPassengers);write(L"Magnetron",L"AutoDrop",extra.magnetronDrop);
 for(int i=0;i<compute::Count;++i)write(L"Compute",compute::key(i),(computeRequested>>i)&1u);
 write(L"Hotkeys",L"IdleAircraft",idleAircraftKey);
 for(int i=0;i<9;++i)write(L"Hotkeys",i?(L"Taunt"+std::to_wstring(i)).c_str():L"Cheer",socialKeys[i]);
 write(L"Display",L"ShroudStrategy",extra.mapStrategy);
 write(L"Display",L"MinimapPosition",minimapPosition);write(L"Display",L"MinimapX",minimapX);write(L"Display",L"MinimapY",minimapY);
 write(L"Display",L"MinimapAlignX",minimapAlignX);write(L"Display",L"MinimapAlignY",minimapAlignY);
 write(L"Display",L"RenderUpgrade",renderUpgradeEnabled);
 write(L"Display",L"BuildTimeTooltip",buildTimeTooltipEnabled);write(L"Display",L"HarvesterCounter",harvesterCounterEnabled);write(L"Display",L"PowerDelta",powerDeltaEnabled);
 write(L"Display",L"RenderProcessing",renderProcessing);
 write(L"Display",L"RenderXbrzEnabled",renderXbrzEnabled);
 write(L"Display",L"UnifiedOverlay",unifiedOverlayEnabled);
 write(L"Display",L"RenderUpgradeLayer",renderUpgradeLayer);
 write(L"Display",L"RenderPaletteRange",renderPaletteRange);
 write(L"Display",L"RenderColorPrecision",renderColorPrecision);
 write(L"Display",L"RenderPixelScale",renderPixelScale);
 write(L"Display",L"RenderHighRefresh",renderHighRefresh);
 write(L"Display",L"RenderMaterialRefresh",renderMaterialRefresh);
 write(L"Display",L"RenderSceneDetail",renderSceneDetail);
 write(L"Display",L"RenderPhysicalOutput",renderPhysicalOutput);
 write(L"Display",L"RenderVulkan",renderVulkanEnabled);
 write(L"Display",L"RenderParticles",renderParticleEffects);write(L"Display",L"RenderLasers",renderLaserEffects);
 write(L"Display",L"RenderVoxelLighting",renderVoxelLighting);write(L"Display",L"RenderSpriteLighting",renderSpriteLighting);
 write(L"Display",L"MinimapMode",minimapMode);
 write(L"Display",L"MinimapSecondary",minimapSecondary);
 write(L"Display",L"MinimapSecondaryX",minimapSecondaryX);write(L"Display",L"MinimapSecondaryY",minimapSecondaryY);write(L"Display",L"MinimapSecondaryWidth",minimapSecondaryWidth);
 write(L"Display",L"MinimapZoom",minimapEnabled);write(L"Display",L"MinimapScale",minimapScale);
 for(int i=0;i<featuretypes::Count;++i){std::wstring custom=std::wstring(featuretypes::key(i))+L"Custom";
  write(L"FeatureTypes",custom.c_str(),featureTypes.filters[i].custom);
  saveWriteOK=(WritePrivateProfileStringW(L"FeatureTypes",featuretypes::key(i),featureTypeText[i].c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;
 }

 write(L"Dodge",L"Enabled",extra.dodgeEnabled);write(L"Dodge",L"Logic",extra.dodgeMode);
 write(L"BurstRefresh",L"Enabled",0);write(L"BurstRefresh",L"HoldEnabled",0);write(L"BurstRefresh",L"HoldKey",0);
 write(L"MissileDefense",L"TargetScope",extra.missileScope);write(L"MissileDefense",L"Allocation",extra.missileAllocation);write(L"Magnetron",L"Global",extra.magnetronGlobal);
 write(L"MissileDefense",L"Enabled",extra.missileDefense);write(L"MissileDefense",L"ScanHz",extra.missileScanHz);
 write(L"MissileDefense",L"SleepReserve",extra.missileReserve);write(L"Combat",L"FilterLowDamage",extra.lowDamageFilter);write(L"Combat",L"NativeFilterInMultiplayer",extra.lowDamageNetwork);
 write(L"Modes",L"AntiTheft",extra.antiTheft);write(L"Experimental",L"ManualReveal",extra.manualReveal);WritePrivateProfileStringW(L"Experimental",L"RevealMap",nullptr,path(L"Assist.ini").c_str());
 write(L"Experimental",L"RefreshStrategy",extra.scrollRefreshMode);
#ifndef RA2_UNIFIED
 write(L"Experimental",L"ScrollRefreshHz",extra.scrollRefreshHz);
#endif
 write(L"Experimental",L"InfantryC4",extra.c4Test);write(L"Experimental",L"C4HoldKey",extra.c4Key);write(L"Experimental",L"C4Mouse",extra.c4Mouse);write(L"Experimental",L"InfantryEntryMode",extra.c4Mode);
 write(L"Experimental",L"RenderReveal",extra.renderReveal);write(L"Experimental",L"NativeAirWaypoints",extra.nativeAirWaypoints);write(L"Aircraft",L"ZWaypoints",extra.airWaypoints);write(L"Aircraft",L"WaypointMode",airpathpolicy::mode(extra.airWaypoints,extra.nativeAirWaypoints,extra.nativeAirNetwork));write(L"Aircraft",L"WaypointHoldKey",extra.airPathKey);write(L"Production",L"ReserveNext",extra.constructionQueue);write(L"Production",L"ReminderLines",extra.productionReminderMask);write(L"Display",L"EnemyMarkers",extra.enemyMarkers);write(L"Display",L"IdleAircraftIndicator",extra.idleAircraft);
 write(L"Display",L"ParasiteMarker",extra.parasiteMarker);write(L"Display",L"CrateBuffMarkers",extra.crateBuffMarkers);
 write(L"Hotkeys",L"ExecuteAllMatches",extra.allHotkeys);write(L"Mouse",L"OverrideNative",extra.overrideNative);
 for(int i=0;i<pointerinput::MouseCount;++i)write(L"Mouse",pointerinput::mouseSetting(i),extra.mouseButtons[i]);

 write(L"AutoLoad",L"Logic",extra.loadMode);write(L"AutoLoad",L"RangeCells",extra.loadRadius);
 write(L"Cooperative",L"AircraftAllocationPolicy",extra.aircraftAllocationPolicy);write(L"Cooperative",L"AdvancedEnter",extra.enter);write(L"Cooperative",L"AircraftSpread",extra.air);write(L"Cooperative",L"EnterHoldKey",extra.enterKey);write(L"Cooperative",L"AirHoldKey1",extra.airKey1);write(L"Cooperative",L"AirHoldKey2",extra.airKey2);
 write(L"SpecialClick",L"DiskHoldKey",extra.diskKey);write(L"SpecialClick",L"SellHoldKey",extra.sellKey);
 write(L"SpecialClick",L"DiskPrimary",extra.disk);write(L"SpecialClick",L"SellDrained",extra.sellDrained);write(L"SpecialClick",L"SellVehicles",extra.sellVehicles);write(L"SpecialClick",L"SellEnemyBuildings",playertest::SellAvailable&&extra.sellEnemy);
 write(L"Experimental",L"PlayerCommandOptionsVersion",1);write(L"Experimental",L"EnemyBuildingSellOptInVersion",1);write(L"Experimental",L"ControlAll",playertest::Available&&extra.controlAll);write(L"Experimental",L"SwitchPlayerEnabled",0);
 write(L"Display",L"Frozen",extra.frozen);write(L"TypeSelection",L"VariantKey",extra.variantKey);
 write(L"AircraftQuota",L"First",extra.defaults.first);write(L"AircraftQuota",L"Second",extra.defaults.second);write(L"AircraftQuota",L"Maximum",extra.defaults.maximum);
 std::wstring block;for(const auto&entry:targetQuotas){const auto&q=entry.second;block+=wide(entry.first.c_str())+L"="+std::to_wstring(q.first)+L","+std::to_wstring(q.second)+L","+std::to_wstring(q.maximum);block.push_back(0);}block.push_back(0);if(block.size()==1)block.push_back(0);
 return WritePrivateProfileSectionW(L"AircraftQuotaOverrides",block.c_str(),path(L"Assist.ini").c_str())!=FALSE;
}
static std::string quotaID(){wchar_t text[64]{};GetWindowTextW(extraControls[QuotaID],text,64);std::string id;for(wchar_t c:std::wstring(text)){if(c<33||c>126)return {};id+=char(c);}return typecatalog::validId(id)?id:std::string{};}
static void readTargetQuota(){ControlUpdate guard;auto id=quotaID();if(id.empty()){notify(L"请输入有效的游戏对象 ID。");return;}auto found=targetQuotas.find(id);auto q=found==targetQuotas.end()?extra.defaults:found->second;
 SendMessageW(extraControls[QuotaOverride],BM_SETCHECK,found==targetQuotas.end()?BST_UNCHECKED:BST_CHECKED,0);
 const int controls[]={QuotaFirst,QuotaSecond,QuotaMaximum},values[]={q.first,q.second,q.maximum};for(int i=0;i<3;++i)SetWindowTextW(extraControls[controls[i]],std::to_wstring(values[i]).c_str());
}
static bool extraNumber(int control,int minimum,int maximum,int&out){BOOL ok;int n=GetDlgItemInt(ui,700+control,&ok,FALSE);if(!ok||n<minimum||n>maximum)return false;out=n;return true;}
static bool readAdditionalControls(){
 AdditionalSettings next=extra;next.aircraftAllocationPolicy=std::clamp(int(SendMessageW(extraControls[AircraftPolicy],CB_GETCURSEL,0,0)),0,2);next.loadMode=SendMessageW(extraControls[LoadLogic],CB_GETCURSEL,0,0)==1?1:0;
 if(!extraNumber(LoadRadius,1,512,next.loadRadius)||!extraNumber(DefaultFirst,0,512,next.defaults.first)||!extraNumber(DefaultSecond,0,512,next.defaults.second)||!extraNumber(DefaultMaximum,0,512,next.defaults.maximum)){notify(L"请输入有效的范围（1–512）和受击数（0–512）。");return false;}
 auto binding=[](int preset,int custom){return presetKey(int(SendMessageW(extraControls[preset],CB_GETCURSEL,0,0)),uint32_t(SendMessageW(extraControls[custom],HKM_GETHOTKEY,0,0)));};
 next.enterKey=binding(EnterPreset,EnterCustom);next.airKey1=binding(AirPreset1,AirCustom1);next.airKey2=binding(AirPreset2,AirCustom2);
 if(!extra.allHotkeys&&keygesture::physical(next.airKey1)&&keygesture::normalize(next.airKey1,keygesture::Mode::Hold)==keygesture::normalize(next.airKey2,keygesture::Mode::Hold)){notify(L"飞机攻击的两个按键需要不同。");return false;}
 next.enter=SendMessageW(extraControls[EnterEnabled],BM_GETCHECK,0,0)==BST_CHECKED;next.air=SendMessageW(extraControls[AirEnabled],BM_GETCHECK,0,0)==BST_CHECKED;
 extra=next;return true;
}
static void fillExtraTypes(bool force=false){
 static uint32_t revision=~0u;if(!extraControls[QuotaType]||(!force&&revision==catalogueRevision))return;revision=catalogueRevision;
 int selected=int(SendMessageW(extraControls[QuotaType],CB_GETCURSEL,0,0));std::string keep=selected>=0&&selected<int(quotaTypeIDs.size())?quotaTypeIDs[selected]:"";
 SendMessageW(extraControls[QuotaType],CB_RESETCONTENT,0,0);quotaTypeIDs.clear();int restore=-1;
 for(const auto&t:savedTypes.types){std::string id=t.id;if(!typecatalog::validId(id))continue;std::wstring label=(const wchar_t*)t.name;label+=L" ["+wide(id.c_str())+L"]";int row=int(SendMessageW(extraControls[QuotaType],CB_ADDSTRING,0,(LPARAM)label.c_str()));if(row<0)continue;quotaTypeIDs.push_back(id);if(id==keep)restore=row;}
 if(restore>=0)SendMessageW(extraControls[QuotaType],CB_SETCURSEL,restore,0);
}
static uint32_t additionalFlags(){uint32_t f=extra.frozen?Frozen:0;if(techMode)f|=UnsafeTech;if(extra.antiTheft)f|=AntiTheft;if(extra.constructionQueue)f|=ReserveConstruction;if(extra.nativeAirWaypoints&&!extra.airPathKey)f|=NativeAirWaypoints;else if(extra.airWaypoints||extra.nativeAirWaypoints)f|=AirWaypoints;if(extra.experimentalReveal)f|=ExperimentalReveal;if(extra.enter)f|=AdvancedEnter;if(extra.air)f|=AircraftSpread;if(extra.disk)f|=DiskPrimary;if(extra.sellDrained)f|=SellDrained;if(extra.sellVehicles)f|=SellVehicles;if(playertest::SellAvailable&&extra.sellEnemy)f|=SellEnemy;return f;}
static void syncRefreshRate(){extra.scrollRefreshMode=0;if(!shared)return;
 shared->scrollRefreshHz=0;shared->scrollRefreshMode=0;
}
static void syncAdditional(){
 if(!playertest::Available)extra.controlAll=false;if(!playertest::SellAvailable)extra.sellEnemy=false;if(!playertest::SwitchAvailable)extra.switchPlayerEnabled=false;if(!shared)return;
 shared->viewZoomPercent=extra.viewZoomEnabled?uint32_t(extra.viewZoomPercent):100;
 shared->smallMapClamp=extra.smallMapClamp;
 shared->sidebarMode=extra.sidebarEnabled&&extra.sidebarMode==2?2u:0u;
 shared->pauseViewKeys[0]=extra.pauseKeys[0];shared->pauseViewKeys[1]=extra.pauseKeys[1];
 shared->moFeatures=extra.moFeatures&23u;
 InterlockedIncrement(&shared->moGesturePolicySeq);shared->moGestures=extra.moGestures;MemoryBarrier();InterlockedIncrement(&shared->moGesturePolicySeq);
 shared->buildTimeTooltip=buildTimeTooltipEnabled;shared->placementImplementation=extra.placementImplementation;shared->techEnhance=extra.techEnhance;
 shared->nativeAirNetwork=extra.nativeAirNetwork;
 shared->crcMapShadow=extra.crcMapShadow;
 shared->networkHousePreserve=extra.networkHouseRequested();shared->nativeSellCursor=extra.nativeSellCursor;
 shared->nativeMegaGateway=extra.nativeMegaGateway;
 shared->nativeCombatQueries=extra.nativeCombatQueries;
 shared->controlCommandOwner=extra.commandOwnerRequested();
 InterlockedIncrement(&shared->formationPolicySeq);shared->formationPolicy.enabled=extra.formationEnabled;shared->formationPolicy.hold=extra.formationHold;shared->formationPolicy.mouse=extra.formationMouse;memcpy(shared->formationPolicy.keys,extra.formationKeys,sizeof extra.formationKeys);MemoryBarrier();InterlockedIncrement(&shared->formationPolicySeq);
 shared->missileReserve=extra.missileDefense&&extra.missileReserve;shared->lowDamageFilter=extra.lowDamageFilter;shared->lowDamageNetwork=extra.lowDamageFilter&&extra.lowDamageNetwork;syncRefreshRate();shared->magnetronDrop=extra.magnetronDrop;shared->dodgeEnabled=extra.dodgeEnabled;shared->dodgeMode=extra.dodgeMode;shared->burstEnabled=shared->burstHoldEnabled=shared->burstHeld=0;shared->missileScope=extra.missileScope;shared->missileAllocation=extra.missileAllocation;shared->magnetronGlobal=extra.magnetronGlobal;shared->missileEnabled=extra.missileDefense;shared->missileScanHz=extra.missileScanHz;shared->controlAll=extra.controlRequest();shared->switchPlayerEnabled=extra.switchPlayerEnabled;shared->computeRequested=computeRequested;shared->advancedScanHz=advancedScanHz;shared->advancedPriority=advancedTargetPriority;shared->c4Test=featureavailability::InfantryForcedEntry&&extra.c4Test;shared->renderReveal=extra.renderReveal;shared->loadMode=extra.loadMode;shared->loadRadius=extra.loadRadius;shared->enterKey=extra.enterKey;shared->airKey1=extra.airKey1;shared->airKey2=extra.airKey2;shared->aircraftAllocationPolicy=extra.aircraftAllocationPolicy;
 static uint32_t publishedSettings=~0u;
 if(!shared->quotaConfigSeq||publishedSettings!=settingsRevision){publishedSettings=settingsRevision;
  InterlockedIncrement(&shared->quotaConfigSeq);auto&c=shared->quotaConfig;c.defaults=extra.defaults;c.count=0;
  for(const auto&e:targetQuotas){if(c.count>=512)break;auto&entry=c.entries[c.count++];memset(entry.id,0,24);memcpy(entry.id,e.first.data(),std::min(size_t(23),e.first.size()));entry.quota=e.second;}
  MemoryBarrier();InterlockedIncrement(&shared->quotaConfigSeq);
 }
 externalSampler.flashCount=0; // production tab reminders stay entirely in the host overlay
}
static void serviceCooperative(){
 if(!shared||!commandFresh(GetTickCount()))return;DWORD now=GetTickCount();
 auto mode=shared->cooperativeRequest.mode;
 if(compute::inside(shared->computeApplied,mode==cooperative::Enter?compute::Enter:compute::Aircraft)){
  if(shared->runtimeCoopDone==shared->cooperativeRequest.serial){cooperativeDelivered=shared->runtimeCoopDone;retryCooperative=false;}
  return;
 }
 uint32_t receipt=shared->cooperativeAck;
 if(receipt!=cooperativeReceipt){
  MemoryBarrier();cooperativeReceipt=receipt;
  for(size_t i=0;i<submittedCooperative.size()&&i<cooperative::MaxSources;++i){
   if(shared->cooperativeAcceptedBits[i/32]&(uint32_t(1)<<(i%32)))acceptedCooperative.push_back(submittedCooperative[i]);
   if(i<size_t(std::max(0,shared->cooperativeProcessed)))processedCooperative.push_back(submittedCooperative[i]);
  }
  if(cooperative::damageMode(cooperativePending.mode))retryCooperative=(shared->cooperativeResult==-7||(shared->cooperativeResult==1&&shared->cooperativeRejected>0))&&
   cooperative::retrySourcePool(cooperativePending,cooperativeSourcePool,processedCooperative)&&freshness::intentAlive(cooperativePending.tick,now);
  else retryCooperative=shared->cooperativeResult==-7&&
   cooperative::retryRemaining(cooperativePending,submittedCooperative.data(),int(submittedCooperative.size()),shared->cooperativeProcessed)&&
   freshness::intentAlive(cooperativePending.tick,now);
  submittedCooperative.clear();
  if(!retryCooperative)notify(L"协同指令本批已提交 "+std::to_wstring(shared->cooperativeAccepted)+L" 个"+(shared->cooperativeRejected?L"；失效或已被手动接管 "+std::to_wstring(shared->cooperativeRejected)+L" 个。":L"。"));
 }
 if(shared->cooperativePlan!=shared->cooperativeAck)return;
 static cooperative::Request request;
 long seq=shared->cooperativeSeq;if(seq&1)return;
 MemoryBarrier();memcpy(&request,&shared->cooperativeRequest,sizeof request);MemoryBarrier();
 if(seq!=shared->cooperativeSeq)return;
 if(request.count>cooperative::MaxSources&&request.serial!=cooperativeDelivered){cooperativeDelivered=request.serial;retryCooperative=false;notify(L"选区超过 512 个，增强请求未提交。");return;}
 bool retry=retryCooperative&&request.serial==cooperativePending.serial&&request.match==cooperativePending.match;
 if(retry)request=cooperativePending;else {retryCooperative=false;acceptedCooperative.clear();processedCooperative.clear();cooperativeSourcePool=request;}
 if(!cooperative::valid(request)||(!retry&&request.serial==cooperativeDelivered)||request.match!=snap.match||DWORD(now-request.tick)<8)return;
 if(!freshness::intentAlive(request.tick,now)){
  cooperativeDelivered=request.serial;retryCooperative=false;notify(L"协同请求已超时，请重新下令。");return;
 }
 // Read the immutable game-thread frame capture; never independently scan
 // another game frame from the host/UI while preparing this command.
 shared->cooperativeFrameWanted=request.serial;
 const long batch=shared->computeFrameSeq;if(batch&1)return;
 static auto captured=std::make_unique<cooperative::FrameInput>();MemoryBarrier();
 if(!cooperative::copyFrame(*captured,shared->cooperativeInputFrame))return;MemoryBarrier();
 if(batch!=shared->computeFrameSeq||captured->request.serial!=request.serial||captured->match!=uint32_t(snap.match)||captured->frame!=shared->computeFrame){wakeGame();return;}
 const DWORD acquired=captured->tick;const int frameBefore=captured->frame;
 if(!freshness::valid(captured->match,snap.match,acquired,frameBefore,now,shared->computeFrame,freshness::Cooperative)){wakeGame();return;}
 auto input=cooperative::unpack(*captured);cooperative::restrictSources(input,request);
 if(retry)cooperative::reserveAccepted(input,acceptedCooperative);
 auto assignments=cooperative::allocate(input);
 if(!freshness::valid(request.match,shared->computeFrameMatch,acquired,frameBefore,GetTickCount(),shared->computeFrame,freshness::Cooperative)||
    !freshness::intentAlive(request.tick,GetTickCount())||shared->cooperativeRequest.serial!=request.serial||shared->cooperativeSeq!=seq){wakeGame();return;}
 shared->cooperativeFrameWanted=0;
 cooperativeDelivered=request.serial;retryCooperative=false;
 if(assignments.empty()){notify(cooperative::damageMode(request.mode)?L"预计伤害已足额，或没有有效的可分配目标。":L"屏幕内没有可分配的同类空位或有效敌方目标。");return;}
 cooperativePending=request;submittedCooperative=assignments;
 memcpy(shared->cooperativeOrders,assignments.data(),assignments.size()*sizeof(cooperative::Assignment));
 shared->cooperativeCount=int(assignments.size());shared->cooperativeMode=request.mode;shared->cooperativeSerial=request.serial;
 shared->cooperativeTick=acquired;shared->cooperativeFrame=frameBefore;shared->cooperativeMatch=request.match;
 MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->cooperativePlan);wakeGame();
 if(assignments.size()<size_t(request.count))notify(L"已分配 "+std::to_wstring(assignments.size())+L" 个；其余单位受容量、受击上限或目标兼容性限制。");
}



