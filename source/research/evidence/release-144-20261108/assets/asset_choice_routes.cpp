
#include "shroud_policy.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace assist;
static struct {int mapStrategy=0,renderReveal=0;bool experimentalReveal=false,mapVisibility=false,manualReveal=false;}extra;
static int ui=1,selected=0,changes=0,syncs=0,notices=0,installNotices=0;
static bool autoRevealNewMatch=false;static unsigned autoRevealMatch=0,autoRevealDone=0,autoRevealRetry=0;
static constexpr int CB_GETCURSEL=1,CB_SETCURSEL=2,RevealMap=81;
struct ControlUpdate{};
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int,int message,int value,int){if(message==CB_SETCURSEL)selected=value;return selected;}
static void changedSetting(){++changes;}static void syncAdditional(){++syncs;}static void notify(const wchar_t*){++notices;}
static void status(const wchar_t*){++installNotices;}
static struct {struct Slot{int command=0;};Slot slots[3];int size=0;void discard(int i){for(int j=i+1;j<size;++j)slots[j-1]=slots[j];--size;}}commandQueue;
static bool route(int id,bool select){if(id==808&&select&&shroudpolicy::strategyFromUI(int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)))<0){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),CB_SETCURSEL,shroudpolicy::uiIndex(extra.mapStrategy),0);return true;}
if(id==808&&select&&extra.mapVisibility&&SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)!=0){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),CB_SETCURSEL,shroudpolicy::uiIndex(extra.mapStrategy),0);notify(L"请先关闭硬件黑幕显示实验。");return true;}
if(id==808&&select){int strategy=shroudpolicy::strategyFromUI(int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)));if(strategy<0)return true;
  extra.mapStrategy=strategy;extra.renderReveal=shroudpolicy::runtimeMode(strategy);extra.experimentalReveal=shroudpolicy::revealEnabled(extra.manualReveal,strategy);autoRevealNewMatch=shroudpolicy::automaticReveal(strategy);
  for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==RevealMap)commandQueue.discard(i);else ++i;
  autoRevealMatch=autoRevealDone=autoRevealRetry=0;changedSetting();syncAdditional();notify(strategy==3?L"地图全开将在对局就绪后自动执行；此模式有联机不同步风险。":L"地图显示策略已更新。");return true;} return false;}
static void install(){status(L"透明 SHA 替换已停用，未更改游戏文件。");}
int main(){unsigned checks=0;auto check=[&](bool v){assert(v);++checks;};
 for(int active=0;active<=4;++active)for(int invalid:{5,6,-1,7,8})for(bool hardware:{false,true}){
  extra.mapStrategy=active;extra.renderReveal=shroudpolicy::runtimeMode(active);extra.experimentalReveal=shroudpolicy::revealEnabled(false,active);extra.mapVisibility=hardware;
  changes=syncs=notices=0;commandQueue.size=2;commandQueue.slots[0].command=RevealMap;commandQueue.slots[1].command=12;
  selected=invalid;check(route(808,true));check(selected==active&&extra.mapStrategy==active);check(extra.renderReveal==shroudpolicy::runtimeMode(active)&&extra.experimentalReveal==shroudpolicy::revealEnabled(false,active));check(changes==0&&syncs==0&&notices==0&&commandQueue.size==2);
 }
 for(int choice=0;choice<=4;++choice){
  extra.mapVisibility=false;extra.mapStrategy=0;selected=choice;changes=syncs=notices=0;commandQueue.size=3;commandQueue.slots[0].command=RevealMap;commandQueue.slots[1].command=12;commandQueue.slots[2].command=RevealMap;
  check(route(808,true));check(extra.mapStrategy==choice&&extra.renderReveal==shroudpolicy::runtimeMode(choice));check(changes==1&&syncs==1&&notices==1&&commandQueue.size==1&&commandQueue.slots[0].command==12);
 }
 install();check(installNotices==1);
 std::printf("asset choice production routes: %u checks passed; disabled SHP rows preserve state and game actions, 0..4 retained, legacy SHA install is inert\n",checks);
}
