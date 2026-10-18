#include "construction_policy.h"
#include "../../unit_monitor/source/icon_cache.h"
#include "../../unit_monitor/source/cameo_reader.h"
#include "../../unit_monitor/source/owned_counter.h"
// Host-only preview and one reservation per native building/defence category.
// Prerequisites are sampled, never changed; Runtime requires the real cameo.
namespace buildreserve {
struct Choice {int slot=0,index=0,tab=0;uint32_t type=0;bool future=false;};
struct Book {uint32_t match=0,house=0,source=0,id=0;int wanted=-1,stage=0,attempts=0;DWORD sent=0,retry=0;};
static Book books[2];static std::vector<Choice> choices;static HWND window;
static uint32_t current[2],currentID[2],lastPID=0,house;static int activeTab=-1,scroll=0,rows=1;
static um::IconCache icons;static uint32_t refreshed=0;static std::vector<int> owned;
template<class T>static T get(uint32_t at,T fallback={}){T v{};return externalReader.readFresh(at,&v,sizeof v)?v:fallback;}
static std::vector<int> list(uint32_t at){uint32_t p=get<uint32_t>(at+4);int n=get<int>(at+16,-1),cap=get<int>(at+8,-1);if(n<0||n>256||cap<n||cap>65536||(n&&p<0x10000))return {};std::vector<int> out(size_t(n),0);if(n&&!externalReader.readFresh(p,out.data(),size_t(n)*4))out.clear();return out;}
static uint32_t factory(int tab){return get<uint32_t>(house+(tab?0x53CC:0x53BC));}
static uint32_t product(int tab){uint32_t f=factory(tab);return f>=0x10000&&get<uint32_t>(f+0x6C)==house?get<uint32_t>(f+0x58):0;}
static bool nativeCameo(int index,int tab){uint32_t strip=0x87F7E8+0x1544+tab*3988;int n=get<int>(strip+84,-1);if(n<0||n>75)return false;for(int i=0;i<n;++i)if(get<int>(strip+88+i*52)==index&&get<int>(strip+92+i*52)==7)return true;return false;}
static void hide(){if(window)ShowWindow(window,SW_HIDE);}
static void reset(){for(auto&b:books)b={};choices.clear();current[0]=current[1]=currentID[0]=currentID[1]=0;activeTab=-1;refreshed=0;hide();}
static void result(int status,int count,uint32_t id){for(auto&b:books)if(b.id==id&&b.stage==1){if(status>0&&count>0){b.stage=2;b.sent=GetTickCount();}else if((status==-6||status==-7||status==-9||status==-3)&&b.attempts<3){b.stage=0;b.retry=GetTickCount()+250;}else{b={};notify(L"预定未提交，已取消；原版生产不受影响。");}break;}}
static void reserve(int item){if(item<0||item>=int(choices.size())||activeTab<0||activeTab>1)return;auto&b=books[activeTab];auto&c=choices[item];
 if(b.stage){notify(L"生产请求正在提交，请等原版队列更新后再预定。");return;}
 if(b.id&&b.wanted==c.index){b={};InvalidateRect(window,nullptr,FALSE);notify(L"已取消这一项预定。");return;}
 uint32_t p=current[activeTab];if(!p||!currentID[activeTab])return;
 b={commandMatch(),house,p,currentID[activeTab],c.index,0,0,0,0};InvalidateRect(window,nullptr,FALSE);notify(L"已预定 "+std::wstring((wchar_t*)snap.types[c.slot].name)+L"；放下当前建筑后继续生产。");
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_MOUSEWHEEL){int delta=short(HIWORD(wp));scroll=std::clamp(scroll+(delta<0?2:-2),0,std::max(0,int(choices.size())-rows*2));InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP){if(!foregroundGame()||snap.chat)return 0;int x=short(LOWORD(lp)),y=short(HIWORD(lp));if(y>=24){int n=scroll+(y-24)/66*2+x/72;if(msg==WM_LBUTTONUP)reserve(n);else if(activeTab>=0&&!books[activeTab].stage){books[activeTab]={};InvalidateRect(w,nullptr,FALSE);}}return 0;}
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);HBRUSH bg=CreateSolidBrush(RGB(24,31,27));FillRect(dc,&client,bg);DeleteObject(bg);auto previous=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(237,219,151));RECT title{3,0,141,23};const wchar_t* heading=activeTab==1?L"防御预定 1 格":L"建筑预定 1 格";DrawTextW(dc,heading,-1,&title,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  for(int n=scroll;n<int(choices.size())&&n<scroll+rows*2;++n){const auto&c=choices[n];const auto&t=snap.types[c.slot];int x=(n-scroll)%2*72+6,y=(n-scroll)/2*66+24;um::Type type{};type.category=3;memcpy(type.id,t.id,24);externalReader.readFresh(c.type+1750,type.cameo,24);type.cameo[24]=0;static um::Snapshot palette{};type.ptr=c.type;bool ready=icons.restore(type,palette);
   if(!ready){static uint8_t buffer[32+256*1024];auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};auto resolve=[](uint32_t){return 0u;};if(um::readCameo(read,resolve,type,buffer,sizeof buffer)&&um::readCameoPalette(read,palette)){ready=true;icons.remember(type,palette);}}
   if(ready){uint32_t pixels[60*48];for(int i=0;i<60*48;++i){uint32_t color=palette.palette[type.icon[i]];if(c.future){unsigned gray=(((color>>16)&255)*3+((color>>8)&255)*6+(color&255))/10;color=(gray<<16)|(gray<<8)|gray;}pixels[i]=color&0xFFFFFF;}
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=60;info.bmiHeader.biHeight=-48;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;SetDIBitsToDevice(dc,x,y,60,48,0,0,0,48,pixels,&info,DIB_RGB_COLORS);
   }else{HBRUSH empty=CreateSolidBrush(c.future?RGB(54,58,53):RGB(67,78,58));RECT icon{x,y,x+60,y+48};FillRect(dc,&icon,empty);DeleteObject(empty);SetTextColor(dc,c.future?RGB(145,150,142):RGB(228,223,188));DrawTextW(dc,(const wchar_t*)t.name,-1,&icon,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
   RECT name{x-3,y+48,x+63,y+64};SetTextColor(dc,c.future?RGB(151,157,147):RGB(231,226,194));DrawTextW(dc,(const wchar_t*)t.name,-1,&name,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   if(activeTab>=0&&books[activeTab].id&&books[activeTab].wanted==c.index){RECT badge{x,y+30,x+60,y+48};HBRUSH tag=CreateSolidBrush(RGB(123,87,20));FillRect(dc,&badge,tag);DeleteObject(tag);SetTextColor(dc,RGB(255,243,156));DrawTextW(dc,L"预定",-1,&badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  }SelectObject(dc,previous);EndPaint(w,&ps);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
static void refreshChoices(){choices.clear();if(activeTab<0||activeTab>1||!current[activeTab])return;
 uint32_t sourceType=get<uint32_t>(current[activeTab]+0x520);int future=get<int>(sourceType+0xDF8,-1);uint32_t country=get<uint32_t>(house+0x34);int countryIndex=get<int>(country+0xB8,-1),tech=get<int>(house+0x1D4,-1);
 uint32_t types=get<uint32_t>(0xA83C68+4);int typeCount=get<int>(0xA83C68+16,-1);if(countryIndex<0||countryIndex>=32||typeCount<0||typeCount>4096||types<0x10000)return;
 owned.assign(size_t(typeCount),0);int errors=0;auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};um::readCounterPrefix(read,house+0x5550,typeCount,owned.data(),errors);if(errors)return;
 std::vector<int> groups[6];uint32_t rules=get<uint32_t>(0x8871E0);if(rules>=0x10000)for(int g=0;g<6;++g)groups[g]=list(rules+0x358+g*28);
 for(int slot=0;slot<snap.typeCount;++slot){const auto&t=snap.types[slot];if(t.kind!=7||t.index<0||t.index>=typeCount)continue;uint32_t type=get<uint32_t>(types+t.index*4);if(type<0x10000)continue;
  int tab=get<int>(type+0xE08,-1)==5?1:0;if(tab!=activeTab)continue;bool native=nativeCameo(t.index,tab),preview=false;
  if(!native){int level=get<int>(type+0x634,-1);uint32_t owner=get<uint32_t>(type+0x6CC),required=get<uint32_t>(type+0xDA0),forbidden=get<uint32_t>(type+0xDA4),bit=1u<<countryIndex;
   if(level<0||level>tech||!(owner&bit)||(required&&!(required&bit))||(forbidden&bit)||get<uint8_t>(type+0xD9D)||get<uint8_t>(type+0xD9E)||get<uint8_t>(type+0xD9F))continue;
   auto needs=list(type+0x638),overrides=list(type+0x654);if(needs.empty()&&overrides.empty())continue;
   if(construction::available(needs,overrides,owned,groups)||!construction::available(needs,overrides,owned,groups,future))continue;
   int limit=get<int>(type+0x3B8);if(limit>0&&limit<10000&&owned[t.index]>=limit)continue;preview=true;
  }choices.push_back({slot,t.index,tab,type,preview});
 }
 scroll=std::clamp(scroll,0,std::max(0,int(choices.size())-rows*2));
}
static void service(){
 uint32_t now=GetTickCount();if(refreshed&&uint32_t(now-refreshed)<100)return;refreshed=now;
 if(!extra.constructionQueue){reset();for(int n=0;n<commandQueue.size;)if(commandQueue.slots[n].command==ProduceReserved)commandQueue.discard(n);else ++n;return;}
 if(!enabled||!commandFresh(now)||!foregroundGame()||snap.chat||snap.renderStatus!=1){hide();return;}
 house=get<uint32_t>(0xA83D4C);if(!house)return;
 if(lastPID!=pid){lastPID=pid;wchar_t image[32768]{};DWORD length=32768;if(externalReader.imagePath(image,&length))icons.open(image,std::filesystem::path(root)/L"cache"/L"production-icons");}
 for(int tab=0;tab<2;++tab){current[tab]=product(tab);currentID[tab]=current[tab]?get<uint32_t>(current[tab]+0x10):0;auto&b=books[tab];if(!b.id)continue;
  if(b.match!=commandMatch()||b.house!=house||get<uint32_t>(b.source+0x10)!=b.id||get<uint32_t>(b.source+0x21C)!=house||get<int>(b.source+0x6C)<=0){b={};continue;}
  if(current[tab]&&currentID[tab]!=b.id){int made=get<int>(get<uint32_t>(current[tab]+0x520)+0xDF8,-1);if(b.stage==2&&made==b.wanted){b={};continue;}if(b.stage==0){b={};notify(L"原生新生产已接管，本项预定已取消。");continue;}}
  if(b.stage==2){if(uint32_t(now-b.sent)>5000){b={};notify(L"预定已提交但尚未观察到生产，已停止跟踪，避免重复下单。");}continue;}
  if(b.stage==1){if(uint32_t(now-b.sent)>15000){b={};notify(L"预定请求等待超时，已取消。");}continue;}
  bool placed=!get<uint8_t>(b.source+0x81)&&get<uint8_t>(b.source+0x90)&&get<uint8_t>(b.source+0x74);
  if(!placed||int32_t(now-b.retry)<0||!nativeCameo(b.wanted,tab)||current[tab])continue;
  int slot=-1;for(int n=0;n<snap.typeCount;++n)if(snap.types[n].kind==7&&snap.types[n].index==b.wanted){slot=n;break;}
  if(slot>=0&&queueCommand(ProduceReserved,slot,int(b.id),b.match)){b.stage=1;b.sent=now;++b.attempts;}
 }
 activeTab=get<int>(0x87F7E8+0x539C,-1);if(activeTab<0||activeTab>1||!current[activeTab]){hide();return;}
 RECT client{};GetClientRect(game,&client);POINT origin{};ClientToScreen(game,&origin);rows=std::clamp((int(client.bottom)-220)/66,1,6);refreshChoices();if(choices.empty()){hide();return;}
 if(!window){WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"RA2ConstructionReserve103";cls.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,cls.lpszClassName,L"建筑预定",WS_POPUP,0,0,144,420,nullptr,nullptr,cls.hInstance,nullptr);}
 if(window){int x=std::max(0,int((snap.width-160)*double(client.right)/std::max(1,snap.width))-152);int y=std::max(0,int(200.*client.bottom/std::max(1,snap.height)));SetWindowPos(window,HWND_TOPMOST,origin.x+x,origin.y+y,144,24+rows*66,SWP_NOACTIVATE|SWP_SHOWWINDOW);InvalidateRect(window,nullptr,FALSE);}
}
static void destroy(){if(window)DestroyWindow(window);window=nullptr;reset();}
}
