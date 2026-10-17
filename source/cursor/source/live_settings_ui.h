static void applyScale(){
 // An unfinished edit must not block an unrelated switch. Retain only the
 // invalid field (or dependent width/height pair); apply other valid values.
 bool incomplete=false;
 auto value=[&](HWND h,int lo,int hi,int old){try{return numeric(h,lo,hi);}catch(...){incomplete=true;return old;}};
 int p=value(scaleBox,25,400,scalePercent),x=value(offsetXBox,-256,256,offsetX),y=value(offsetYBox,-256,256,offsetY);
 int gw=gameWOverride,gh=gameHOverride,sw=screenWOverride,sh=screenHOverride;
 auto pair=[&](HWND a,HWND b,int& w,int& h){try{int nw=numeric(a,0,16384),nh=numeric(b,0,16384);
  if((nw==0)!=(nh==0)||(nw&&nw<320)||(nh&&nh<200))throw std::runtime_error("pair");
  w=nw;h=nh;}catch(...){incomplete=true;}};
 pair(gameWBox,gameHBox,gw,gh);pair(screenWBox,screenHBox,sw,sh);
 int dp=value(dpiBox,0,500,dpiOverride);if(dp&&dp<50){dp=dpiOverride;incomplete=true;}
 int ms=value(smoothMsBox,10,200,smoothMs),gap=value(clickGapBox,0,250,clickGap);
 auto check=[](HWND h){return SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;};
 int dm=std::clamp((int)SendMessageW(dpiModeBox,CB_GETCURSEL,0,0),0,2),cm=std::clamp((int)SendMessageW(clickModeBox,CB_GETCURSEL,0,0),0,2);
 bool sm=check(smoothBox),ad=check(adaptiveBox),ce=check(clickBox),au=check(autoBox),fit=check(fitBox);
 bool geometry=p!=scalePercent||x!=offsetX||y!=offsetY||gw!=gameWOverride||gh!=gameHOverride||sw!=screenWOverride||sh!=screenHOverride||dp!=dpiOverride||dm!=dpiMode||au!=autoScale||fit!=aspectFit;
 bool animation=sm!=smoothing||ad!=adaptive||ms!=smoothMs;
 bool clicking=ce!=clickEnabled||cm!=clickMode||gap!=clickGap;
 // Also retry disk writes after a previous save failure; unchanged runtime
 // state is not evidence that the INI contains it.
 scalePercent=p;offsetX=x;offsetY=y;gameWOverride=gw;gameHOverride=gh;screenWOverride=sw;screenHOverride=sh;dpiOverride=dp;dpiMode=dm;autoScale=au;aspectFit=fit;
 smoothing=sm;adaptive=ad;smoothMs=ms;clickEnabled=ce;clickMode=cm;clickGap=gap;
 if(animation){tween.reset();timing.resetObservation();activeMotion.reset();lastPhase=size_t(-1);}
 if(clicking)burst.cancel();if(geometry||animation)lastFrame=-2;
 bool saved=true;auto put=[&](const wchar_t* section,const wchar_t* key,int v){saved=WritePrivateProfileStringW(section,key,std::to_wstring(v).c_str(),ini().c_str())&&saved;};
 put(L"Cursor",L"ScalePercent",p);put(L"Cursor",L"HotspotOffsetX",x);put(L"Cursor",L"HotspotOffsetY",y);put(L"Cursor",L"AutoScale",au);put(L"Cursor",L"AspectFit",fit);put(L"Cursor",L"GameWidth",gw);put(L"Cursor",L"GameHeight",gh);put(L"Cursor",L"ScreenWidth",sw);put(L"Cursor",L"ScreenHeight",sh);put(L"Cursor",L"WindowsScalePercent",dp);put(L"Cursor",L"WindowsScaleMode",dm);
 put(L"Animation",L"Smooth",sm);put(L"Animation",L"Adaptive",ad);put(L"Animation",L"BlendMs",ms);put(L"Click",L"Enabled",ce);put(L"Click",L"Mode",cm);put(L"Click",L"IntervalMs",gap);
 textIfChanged(settingsHint,!saved?L"当前已生效，但写入设置失败，请检查目录权限":incomplete?
  L"有效设置已自动保存；未完成的数字或尺寸组合保留上次值":L"设置已自动保存");
}

