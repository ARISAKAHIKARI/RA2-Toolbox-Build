#ifdef NDEBUG
#undef NDEBUG
#endif
#define RA2_UNIFIED
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../source/ui_layout.h"
#include "../source/settings_fold_layout.h"
#include "../../suite/source/settings_categories.h"
using namespace assist;
static HWND ui=HWND(1);static int activePage=0,scrollY=0,scrollX=0,layouts=0,checks=0;
static std::map<HWND,int>ids;static std::map<int,std::wstring>captions;static std::set<int>openedSettingGroups;
static int testID(HWND h){return ids[h];}static int testText(HWND,wchar_t*out,int){out[0]=0;return 0;}
static BOOL testShow(HWND,int){return TRUE;}static BOOL testCaption(HWND,int id,const wchar_t*s){captions[id]=s;return TRUE;}
#define GetDlgCtrlID testID
#define GetWindowTextW testText
#define ShowWindow testShow
#define SetDlgItemTextW testCaption
namespace modernui {static void tip(HWND,const std::wstring&){};}
static std::wstring controlHelp(int,const std::wstring&){return L"help";}
static void layoutPanel(){++layouts;}
// Actual production PageControl, applySettingsFolds, pageBottom, and category
// packing. Win32 window IO alone is replaced; layout code is not duplicated.
#include SETTINGS_FOLD_ROUTES
static void check(bool okay){assert(okay);++checks;}
static void add(int id,int page,int y,int visible,int group=0,int height=0){HWND h=HWND(INT_PTR(ids.size()+10));ids[h]=id;pageControls.push_back({h,page,{20,y,300,height?height:visible},visible,{},group});}
static PageControl&control(int id){return *std::find_if(pageControls.begin(),pageControls.end(),[&](const auto&c){return ids[c.hwnd]==id;});}
int main(){
 add(705,1,0,26);add(9601,1,0,26);add(0,1,40,26,9601);add(9195,1,40,26,9601,240);add(710,1,80,26,9601);
 add(9050,1,120,26);add(9602,1,120,26);add(9056,1,160,26,9602,180);add(9051,1,200,26,9602);
 add(719,1,240,26);add(455,0,0,26);add(456,0,0,26);add(899,4,0,26);
 add(880,2,0,26);add(9603,2,0,26);add(9040,2,40,26,9603);add(9041,2,80,26,9603);add(881,2,120,26,9603);add(804,2,160,26);
 add(476,4,40,26);add(807,4,80,44);add(421,4,140,26);add(422,4,180,152);add(423,4,180,26);add(424,4,218,26);add(425,4,256,26);
 add(830,3,0,26);add(9300,3,40,26);add(430,-2,0,26);
 finalizeTacticalLayout();check(layouts==1&&openedSettingGroups.empty());
 for(int id:{705,9050,880,719,804})check(!control(id).foldHidden);
 for(int id:{9195,710,9056,9051,9040,9041,881})check(control(id).foldHidden);
 check(control(9195).rect.h==240&&control(9195).visibleHeight==26);check(control(807).page==suite::categories::Rules);
 for(int id:{455,456,899})check(control(id).page==suite::categories::Automatic);
 check(control(830).page==-3&&control(9300).page==-3);check(control(422).rect.y==control(423).rect.y);check(control(424).rect.y-control(423).rect.y==38);
 std::vector<int>closedY;for(const auto&c:pageControls)closedY.push_back(c.rect.y);const int closedBottom=pageBottom();
 const int automaticY=control(804).rect.y;openedSettingGroups.insert(9601);applySettingsFolds();check(!control(9195).foldHidden&&!control(710).foldHidden&&control(9051).foldHidden);check(control(804).rect.y==automaticY);check(pageBottom()==closedBottom+80);check(captions[9601]==L"收起设置");
 openedSettingGroups={9601,9602,9603};applySettingsFolds();for(const auto&c:pageControls){check(!c.foldHidden);check(c.rect.y==c.fullRect.y);}check(control(9195).rect.h==240);
 for(int iteration=0;iteration<20;++iteration){openedSettingGroups.clear();applySettingsFolds();for(size_t n=0;n<pageControls.size();++n)check(pageControls[n].rect.y==closedY[n]);openedSettingGroups={9601,9602,9603};applySettingsFolds();}
 openedSettingGroups.clear();applySettingsFolds();check(pageBottom()==closedBottom&&captions[9602]==L"展开设置");
 // Overlapping list/button columns remain one row, even with mixed groups.
 auto mixed=settingsfold::layout({{0,100,80,0},{0,100,26,9601},{0,140,26,9601},{0,194,26,0}},{});
 check(!mixed[0].hidden&&mixed[1].hidden&&mixed[2].hidden&&!mixed[3].hidden&&mixed[3].y==194);
 printf("{\"checks\":%d,\"passed\":true,\"scope\":\"actual category packing/fold reflow/page extent, simulated Win32 windows\"}\n",checks);
}
