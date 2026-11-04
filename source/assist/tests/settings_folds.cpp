#ifdef NDEBUG
#undef NDEBUG
#endif
#define RA2_UNIFIED
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../source/ui_layout.h"
#include "../source/settings_fold_layout.h"
#include "../source/additional_settings.h"
#include "../source/event_delivery.h"
#ifdef NDEBUG
#error "settings_folds assertions must be active"
#endif
#include "../../suite/source/settings_categories.h"
using namespace assist;
static HWND ui=HWND(1),featureSearch=HWND(2),focused=nullptr;static int activePage=0,scrollY=0,scrollX=0,layouts=0,checks=0;
static std::map<HWND,int>ids;static std::map<int,std::wstring>captions;static std::set<int>openedSettingGroups,failedSettingGroups;
static std::map<HWND,std::wstring>texts,tooltips;
static AdditionalSettings extra;static bool freshConnection=true;
struct UIShared {int crcMapShadowStatus=0,networkHouseStatus=0,playerControlStatus=0,techParameterStatus=0,lowDamageStatus=0,playerCurrent=0,nativeSellCursorStatus=0,nativeMegaGatewayStatus=0,controlCommandOwnerStatus=0;
 unsigned networkHousePreserve=0,crcMapShadowTracked=0,crcMapShadowSubstitutions=0;}storage;
static UIShared*shared=&storage;static struct {int techError=0;}snap;static struct {int session=3;}incoming;
static bool commandFresh(DWORD){return freshConnection;}
static void setConsoleTooltip(HWND control,const std::wstring&text){tooltips[control]=text;}
static int testID(HWND h){return ids[h];}static int testText(HWND h,wchar_t*out,int size){const auto&text=texts[h];const auto n=std::min<size_t>(text.size(),size-1);std::copy_n(text.c_str(),n,out);out[n]=0;return int(n);}
static BOOL testShow(HWND,int){return TRUE;}static BOOL testCaption(HWND,int id,const wchar_t*s){captions[id]=s;return TRUE;}
static BOOL testSetText(HWND h,const wchar_t*s){texts[h]=s;return TRUE;}
static HWND testItem(HWND,int id){for(const auto&p:ids)if(p.second==id)return p.first;return nullptr;}
static HWND testFocus(){return focused;}static HWND testSetFocus(HWND h){auto old=focused;focused=h;return old;}
static BOOL testClient(HWND,RECT*r){*r={0,0,760,540};return TRUE;}
#define GetDlgCtrlID testID
#define GetWindowTextW testText
#define ShowWindow testShow
#define SetDlgItemTextW testCaption
#define SetWindowTextW testSetText
#define GetDlgItem testItem
#define GetFocus testFocus
#define SetFocus testSetFocus
#define GetClientRect testClient
namespace modernui {static void tip(HWND,const std::wstring&){};}
static std::wstring controlHelp(int,const std::wstring&){return L"help";}
static void layoutPanel(){++layouts;}
static int logical(int n){return n;}static void scrollTo(int n){scrollY=n;layoutPanel();}static void notify(const wchar_t*){}
// Actual production PageControl, applySettingsFolds, pageBottom, and category
// packing. Win32 window IO alone is replaced; layout code is not duplicated.
#include SETTINGS_FOLD_ROUTES
static void check(bool okay){assert(okay);++checks;}
static void add(int id,int page,int y,int visible,int group=0,int height=0){HWND h=HWND(INT_PTR(ids.size()+10));ids[h]=id;pageControls.push_back({h,page,{20,y,300,height?height:visible},visible,{},group});}
static PageControl&control(int id){return *std::find_if(pageControls.begin(),pageControls.end(),[&](const auto&c){return ids[c.hwnd]==id;});}
int main(){
 for(int id:{9097,9098,9470,9471,9472})check(suite::categories::row(4,&id,1)==suite::categories::Rules);
 add(705,1,0,26);add(9601,1,0,26);add(0,1,40,26,9601);add(9195,1,40,26,9601,240);add(710,1,80,26,9601);
 add(9050,1,120,26);add(9602,1,120,26);add(9056,1,160,26,9602,180);add(9051,1,200,26,9602);
 add(719,1,240,26);add(455,0,0,26);add(456,0,0,26);add(899,4,0,26);
 add(880,2,0,26);add(9603,2,0,26);add(9040,2,40,26,9603);add(9041,2,80,26,9603);add(881,2,120,26,9603);add(804,2,160,26);
 add(476,4,40,26);add(807,4,80,44);add(420,4,120,26);add(9607,4,120,26);add(896,4,160,26);add(421,4,200,26,9607);add(422,4,240,152,9607);add(423,4,240,26,9607);add(424,4,278,26,9607);add(425,4,316,26,9607);add(815,4,410,65,9607);
 add(9604,0,80,26);for(int n=0;n<9;++n)add(870+n,0,120+n*40,26,9604);
 add(9608,0,500,26);add(440,0,540,26,9608,300);add(441,0,540,26,9608);add(467,0,580,26,9608);add(444,0,620,26,9608);add(445,0,660,26,9608);
 add(458,1,280,26);add(9606,1,280,26);add(450,1,320,26,9606);add(787,1,360,26,9606);add(818,1,400,26,9606);add(802,1,440,26,9606,240);
 add(470,2,200,26);add(9605,2,200,26);add(781,2,240,26,9605);add(783,2,280,26,9605);add(782,2,320,26,9605);add(784,2,360,26,9605);
 add(832,4,520,30);add(9609,4,520,26);for(int n=0;n<5;++n)add(9000+n,4,570+n*40,26,9609,240);add(833,4,790,192,9609);
 add(780,2,410,26);
 add(905,5,0,30);add(9610,5,0,26);for(int n=0;n<4;++n){add(9200+n*2,5,42+n*80,26,9610);add(9201+n*2,5,80+n*80,26,9610);}
 add(9611,1,500,26);add(109,1,540,26,9611);add(209,1,540,26,9611);add(410,1,580,26,9611);
 add(860,4,830,26);add(9612,4,830,26);add(9250,4,870,26,9612);add(865,4,910,26,9612);
 add(9196,4,950,26);add(9613,4,950,26);add(9197,4,990,60,9613);add(9187,4,1070,26);add(9614,4,1070,26);add(9188,4,1110,44,9614);
 add(9253,4,1180,26);add(9185,4,1180,26);add(9254,4,1220,26);add(9186,4,1260,52);add(9184,2,450,26);control(9184).feedbackHidden=true;add(9182,2,490,26);add(9194,4,1330,44);add(9190,4,1390,44);add(9192,4,1450,44);
 add(9615,0,1500,26);for(int n=0;n<7;++n)add(9480+n,0,1540+n*40,26,9615);
 add(830,3,0,26);add(9300,3,40,26);add(430,-2,0,26);
 finalizeTacticalLayout();check(layouts==1&&openedSettingGroups.empty());
 for(int id:{705,9050,880,719,804,458,470,420,896,832})check(!control(id).foldHidden);
 for(int id:{9195,710,9056,9051,9040,9041,881,870,878,783,784,450,787,802,421,422,423,425,440,467,9000,9004,815,833})check(control(id).foldHidden);
 check(control(9195).rect.h==240&&control(9195).visibleHeight==26);check(control(807).page==suite::categories::Rules);
 for(int id:{455,456,899})check(control(id).page==suite::categories::Automatic);
 check(control(830).page==-3&&control(9300).page==-3);check(control(422).rect.y==control(423).rect.y);check(control(424).rect.y-control(423).rect.y==38);
 std::vector<int>closedY;for(const auto&c:pageControls)closedY.push_back(c.rect.y);const int closedBottom=pageBottom();
 const int automaticY=control(804).rect.y;openedSettingGroups.insert(9601);applySettingsFolds();check(!control(9195).foldHidden&&!control(710).foldHidden&&control(9051).foldHidden);check(control(804).rect.y==automaticY);check(pageBottom()==closedBottom+80);check(captions[9601]==L"收起设置");
 check(control(780).page==suite::categories::Rules);for(int id:{9253,9254,9185,9186})check(control(id).page==suite::categories::Experimental);
 for(int id:{9200,9201,9206,9207,109,209,410,9250,865,9197,9188})check(control(id).foldHidden);
 const std::set<int>allGroups={9601,9602,9603,9604,9605,9606,9607,9608,9609,9610,9611,9612,9613,9614,9615};
 openedSettingGroups=allGroups;applySettingsFolds();for(const auto&c:pageControls){check(c.foldHidden==c.feedbackHidden);if(!c.feedbackHidden)check(c.rect.y<=c.fullRect.y);}check(control(9195).rect.h==240);
 for(int iteration=0;iteration<20;++iteration){openedSettingGroups.clear();applySettingsFolds();for(size_t n=0;n<pageControls.size();++n)check(pageControls[n].rect.y==closedY[n]);openedSettingGroups=allGroups;applySettingsFolds();}
 openedSettingGroups.clear();applySettingsFolds();check(pageBottom()==closedBottom&&captions[9602]==L"展开设置");
 // Overlapping list/button columns remain one row, even with mixed groups.
 auto mixed=settingsfold::layout({{0,100,80,0},{0,100,26,9601},{0,140,26,9601},{0,194,26,0}},{});
 check(!mixed[0].hidden&&mixed[1].hidden&&mixed[2].hidden&&!mixed[3].hidden&&mixed[3].y==194);
 // Actual click handler is generic: every registered group opens on its own,
 // without changing the child text/key/choice contents or any other group.
 for(const auto&c:pageControls)texts[c.hwnd]=L"saved setting "+std::to_wstring(ids[c.hwnd]);const auto saved=texts;
 for(int group:allGroups){check(toggleSettingsFold(group));check(openedSettingGroups==std::set<int>{group});for(const auto&c:pageControls)check(c.foldHidden==(c.feedbackHidden||(c.fold&&c.fold!=group)));check(texts==saved);check(toggleSettingsFold(group)&&openedSettingGroups.empty());}
 const int priorLayouts=layouts;check(!toggleSettingsFold(420)&&layouts==priorLayouts&&openedSettingGroups.empty());
 // Closing while an edit owns focus moves focus to its visible button. A
 // queued old focus notification must not reopen the group after closing.
 check(toggleSettingsFold(9607));focused=control(421).hwnd;check(toggleSettingsFold(9607)&&focused==control(9607).hwnd&&control(421).foldHidden);focusRoute(ui,LPARAM(control(421).hwnd));check(!openedSettingGroups.count(9607));
 focused=control(421).hwnd;focusRoute(ui,LPARAM(focused));check(openedSettingGroups.count(9607)&&!control(421).foldHidden);check(toggleSettingsFold(9607));
 // Actual search branch opens a matching child, but only on the active page.
 activePage=suite::categories::Rules;auto query=control(421).hwnd;texts[query]=L"unique technology permission";texts[featureSearch]=L"TECHNOLOGY PERMISSION";searchRoute();check(openedSettingGroups.count(9607)&&!control(422).foldHidden);check(!control(420).foldHidden&&!control(896).foldHidden&&!control(815).foldHidden);
 check(toggleSettingsFold(9607));activePage=suite::categories::Automatic;searchRoute();check(!openedSettingGroups.count(9607));
 // Healthy detail lines consume no row. An actual error is immediately
 // visible and closing it restores every coordinate, without touching data.
 const int beforeFeedback=pageBottom();const auto beforeText=texts;
 setConsoleFeedback(control(9184).hwnd,L"native patch conflict");check(!control(9184).foldHidden&&pageBottom()==beforeFeedback+40);check(texts[control(9184).hwnd]==L"native patch conflict");
 setConsoleFeedback(control(9184).hwnd,L"");check(control(9184).foldHidden&&pageBottom()==beforeFeedback);
 for(const auto&p:beforeText)if(p.first!=control(9184).hwnd)check(texts[p.first]==p.second);
 // Actual status sampling forces only failed groups visible, independently
 // of a stale command connection or an unchecked feature. User preference is
 // never overwritten and cannot hide an outstanding apply/restore failure.
 struct Failure {int group,child;int*status;};
 Failure failures[]={{9613,9197,&storage.crcMapShadowStatus},{9614,9188,&storage.networkHouseStatus},
  {9612,865,&storage.playerControlStatus},{9607,815,&snap.techError},{9607,815,&storage.techParameterStatus}};
 for(const auto&failure:failures){
  openedSettingGroups.clear();failedSettingGroups.clear();storage={};snap={};applySettingsFolds();
  const auto savedPreferences=openedSettingGroups;const auto savedEdit=texts[control(421).hwnd];const int healthyBottom=pageBottom();
  *failure.status=-2;freshConnection=false;updateControlModeStatus(100);updateConsoleFoldErrors();
  check(failedSettingGroups==std::set<int>{failure.group});check(openedSettingGroups==savedPreferences);
  check(!control(failure.child).foldHidden&&captions[failure.group]==L"错误 · 已展开");
  check(pageBottom()>=healthyBottom);check(toggleSettingsFold(failure.group));
  check(!control(failure.child).foldHidden&&openedSettingGroups==savedPreferences);
  check(texts[control(421).hwnd]==savedEdit);const int priorLayout=layouts;updateConsoleFoldErrors();check(layouts==priorLayout);
  if(failure.group==9612)check(texts[control(865).hwnd].find(L"还原未完成")!=std::wstring::npos);
  *failure.status=0;updateControlModeStatus(101);updateConsoleFoldErrors();
  check(failedSettingGroups.empty()&&openedSettingGroups.empty());check(control(failure.child).foldHidden);
  check(captions[failure.group]==L"展开设置"&&pageBottom()==healthyBottom);
  check(toggleSettingsFold(failure.group));*failure.status=-2;updateConsoleFoldErrors();
  check(!control(failure.child).foldHidden&&openedSettingGroups==std::set<int>{failure.group});
  *failure.status=0;updateConsoleFoldErrors();check(!control(failure.child).foldHidden);
  check(captions[failure.group]==L"收起设置"&&openedSettingGroups==std::set<int>{failure.group});
  check(toggleSettingsFold(failure.group)&&control(failure.child).foldHidden);
 }
 storage={};snap={};openedSettingGroups.clear();failedSettingGroups.clear();applySettingsFolds();
 storage.crcMapShadowStatus=storage.networkHouseStatus=storage.playerControlStatus=storage.techParameterStatus=-2;
 snap.techError=-44;updateConsoleFoldErrors();check(failedSettingGroups==std::set<int>({9607,9612,9613,9614}));
 for(int id:{815,865,9197,9188})check(!control(id).foldHidden);
 storage.crcMapShadowStatus=0;updateConsoleFoldErrors();check(control(9197).foldHidden&&!control(815).foldHidden&&!control(865).foldHidden&&!control(9188).foldHidden);
 storage={};snap={};storage.crcMapShadowStatus=storage.networkHouseStatus=storage.playerControlStatus=storage.techParameterStatus=1;
 updateConsoleFoldErrors();check(failedSettingGroups.empty());for(int id:{815,865,9197,9188})check(control(id).foldHidden);
 // A failed native low-damage restore remains visible after its toggle is
 // unchecked. A successful restore removes the row and its whitespace.
 storage={};extra={};const int lowHealthyBottom=pageBottom();storage.lowDamageStatus=-2;fixtureLowDamage();
 check(!extra.lowDamageFilter&&!control(9184).foldHidden);check(texts[control(9184).hwnd].find(L"还原未完成")!=std::wstring::npos);
 check(tooltips[control(9182).hwnd].find(L"还原未完成")!=std::wstring::npos);
 storage.lowDamageStatus=0;fixtureLowDamage();check(control(9184).foldHidden&&pageBottom()==lowHealthyBottom);
 for(int mode:{1,2}){extra.lowDamageFilter=true;storage.lowDamageStatus=mode;fixtureLowDamage();check(control(9184).foldHidden);}
 // Extracted actual status block: a disabled adapter or selecting Master
 // cannot make a failed restore look inactive, including stale connection.
 for(bool connected:{false,true})for(bool master:{false,true}){
  storage={};extra={};freshConnection=connected;extra.controlAll=master;extra.controlImplementation=0;
  storage.controlCommandOwnerStatus=-2;fixtureAdaptationStatus();
  check(texts[control(9194).hwnd].find(L"还原失败")!=std::wstring::npos);
  check(texts[control(9194).hwnd].find(L"不请求启用")==std::wstring::npos);
  storage.controlCommandOwnerStatus=0;fixtureAdaptationStatus();
  check(texts[control(9194).hwnd].find(L"还原失败")==std::wstring::npos);
  check(texts[control(9194).hwnd].find(master?L"不请求启用":L"关闭")!=std::wstring::npos);
 }
 // Negative CRC and sender status takes precedence over stale connection,
 // disabled switches, and the Master exclusion. Recovery keeps healthy text.
 storage={};extra={};extra.controlAll=true;freshConnection=false;
 storage.networkHouseStatus=-2;storage.crcMapShadowStatus=-2;fixtureAdaptationStatus();
 check(texts[control(9188).hwnd].find(L"还原失败")!=std::wstring::npos);
 check(texts[control(9197).hwnd].find(L"恢复冲突")!=std::wstring::npos);
 storage.networkHouseStatus=storage.crcMapShadowStatus=0;fixtureAdaptationStatus();
 check(texts[control(9188).hwnd].find(L"不请求启用")!=std::wstring::npos);
 check(texts[control(9197).hwnd].find(L"关闭")!=std::wstring::npos);
 printf("{\"checks\":%d,\"passed\":true,\"groups\":15,\"scope\":\"actual category packing/fold/click/focus/search/Shared errors and recovery/conditional feedback routes; assertions active; simulated Win32 windows\"}\n",checks);
}
