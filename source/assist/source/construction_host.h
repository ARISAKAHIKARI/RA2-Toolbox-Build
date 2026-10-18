#include "construction_catalog.h"
#include "../../unit_monitor/source/icon_cache.h"
#include "../../unit_monitor/source/cameo_reader.h"
// Host-owned FIFO. GameRuntime remains the only writer/command constructor.
namespace buildreserve {
struct Choice {int slot=0,index=0,tab=0;uint32_t type=0;bool future=false;};
static construction::Book books[2];static std::vector<Choice> choices;static HWND window;
static uint32_t current[2],currentID[2],lastPID=0,house,refreshed=0,nextToken=0;
static int activeTab=-1,scroll=0;static um::IconCache icons;static construction::Layout layout;
static HFONT panelFont;static int fontHeight=0;static bool geometryReady=false;
template<class T>static T get(uint32_t at,T fallback={}){T v{};return externalReader.readFresh(at,&v,sizeof v)?v:fallback;}
static uint32_t factory(int tab){return get<uint32_t>(house+(tab?0x53CC:0x53BC));}
static uint32_t product(int tab){uint32_t f=factory(tab);return f>=0x10000&&get<uint32_t>(f+0x6C)==house?get<uint32_t>(f+0x58):0;}
static bool nativeCameo(int index,int tab){uint32_t strip=0x87F7E8+0x1544+tab*3988;int n=get<int>(strip+84,-1);if(n<0||n>75)return false;for(int i=0;i<n;++i)if(get<int>(strip+88+i*52)==index&&get<int>(strip+92+i*52)==7)return true;return false;}
static bool placed(uint32_t p){return !get<uint8_t>(p+0x81,1)&&get<uint8_t>(p+0x90)&&get<uint8_t>(p+0x74);}
static void hide(){geometryReady=false;if(window)ShowWindow(window,SW_HIDE);}
static void cancel(int tab){auto&b=books[tab];for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==ProduceReserved&&commandQueue.slots[i].token==b.token)commandQueue.discard(i);else ++i;b={};}
static void reset(){cancel(0);cancel(1);choices.clear();current[0]=current[1]=currentID[0]=currentID[1]=0;activeTab=-1;refreshed=0;hide();}
static int typeSlot(int index){for(int i=0;i<snap.typeCount;++i)if(snap.types[i].kind==7&&snap.types[i].index==index)return i;return -1;}
static std::wstring nameOf(int index){int slot=typeSlot(index);return slot>=0?std::wstring((const wchar_t*)snap.types[slot].name):L"未知建筑";}
static void result(int status,int count,uint32_t id,uint32_t token){for(int tab=0;tab<2;++tab){auto&b=books[tab];if(b.id!=id||b.token!=token||!token||b.stage!=1)continue;
 if(status>0&&count>0){b.stage=2;b.sent=GetTickCount();}
 else if((status==-6||status==-7||status==-9||status==-3)&&b.attempts<3){b.stage=0;b.retry=GetTickCount()+250;}
 else{cancel(tab);notify(L"预定未提交，已取消后续预定；原版生产不受影响。");}break;
}}
static void refreshChoices(){
 choices.clear();if(activeTab<0||activeTab>1||!current[activeTab])return;
 std::vector<int> future;uint32_t p=current[activeTab];if(!placed(p))future.push_back(get<int>(get<uint32_t>(p+0x520)+0xDF8,-1));
 auto&b=books[activeTab];future.insert(future.end(),b.items.begin(),b.items.end());
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};construction::Catalogue<decltype(read)> catalogue(read);bool previewReady=catalogue.load(house,future);
 uint32_t types=get<uint32_t>(0xA83C68+4);int count=get<int>(0xA83C68+16,-1);if(types<0x10000||count<0||count>4096)return;
 std::vector<int> nativeIndices;uint32_t strip=0x87F7E8+0x1544+activeTab*3988;int cameoCount=get<int>(strip+84,-1);int entries[75*13]{};
 if(cameoCount>0&&cameoCount<=75&&externalReader.readFresh(strip+88,entries,size_t(cameoCount)*52))for(int i=0;i<cameoCount;++i)if(entries[i*13+1]==7)nativeIndices.push_back(entries[i*13]);
 for(int slot=0;slot<snap.typeCount;++slot){const auto&t=snap.types[slot];if(t.kind!=7||t.index<0||t.index>=count)continue;uint32_t type=get<uint32_t>(types+t.index*4);if(type<0x10000)continue;
  int tab=get<int>(type+0xE08,-1)==5?1:0;if(tab!=activeTab)continue;bool native=std::find(nativeIndices.begin(),nativeIndices.end(),t.index)!=nativeIndices.end();
  if(native||(previewReady&&catalogue.preview(t.index)))choices.push_back({slot,t.index,tab,type,!native});
 }
 int maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll,0,maximum);
}
static bool usable(){return extra.constructionQueue&&enabled&&commandFresh(GetTickCount())&&foregroundGame()&&!snap.chat&&snap.renderStatus==1;}
static void reserve(int item){
 if(!usable()||item<0||item>=int(choices.size())||activeTab<0||activeTab>1)return;Choice c=choices[item];auto&b=books[activeTab];
 if(b.stage){notify(L"生产请求正在提交，请等原版队列更新后再预定。");return;}
 uint32_t p=product(activeTab),identity=get<uint32_t>(p+0x10);if(!p||p!=current[activeTab]||identity!=currentID[activeTab]||get<uint32_t>(0xA83D4C)!=house)return;
 if(!b.items.empty()&&(b.match!=commandMatch()||b.house!=house||b.id!=identity)){cancel(activeTab);return;}
 refreshChoices();bool valid=false;for(const auto&choice:choices)if(choice.index==c.index&&choice.type==c.type)valid=true;if(!valid)return;
 if(!b.add(c.index)){notify(L"本类预定已满（5 项）；右键预定编号可取消该项及后续项。");return;}
 if(b.items.size()==1){b.match=commandMatch();b.house=house;b.source=p;b.id=identity;}
 refreshChoices();InvalidateRect(window,nullptr,FALSE);notify(L"已加入预定 "+std::to_wstring(b.items.size())+L"："+nameOf(c.index)+L"；放下当前建筑后依次继续生产。");
}
static void removeFrom(int item){if(!usable()||activeTab<0||activeTab>1)return;auto&b=books[activeTab];if(item<0||item>=int(b.items.size()))return;
 if(item==0&&b.stage){bool unsent=false;for(int i=0;i<commandQueue.size;++i)if(commandQueue.slots[i].command==ProduceReserved&&commandQueue.slots[i].token==b.token)unsent=true;if(unsent){cancel(activeTab);refreshChoices();InvalidateRect(window,nullptr,FALSE);return;}notify(L"第一项已提交，等待原版生产确认；后续项仍可取消。");return;}
 b.items.resize(item);if(!item)cancel(activeTab);refreshChoices();InvalidateRect(window,nullptr,FALSE);notify(L"已取消该项及后续预定。");
}
static RECT rect(int x,int y,int r,int bottom){return {layout.x(x),layout.y(y),layout.x(r),layout.y(bottom)};}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_MOUSEWHEEL){if(!geometryReady)return 0;int delta=short(HIWORD(wp)),maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll+(delta<0?2:-2),0,maximum);InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP){if(!geometryReady||!usable())return 0;int x=short(LOWORD(lp)),y=short(HIWORD(lp));
  if(msg==WM_RBUTTONUP){int slot=layout.queueAt(x,y);if(slot>=0)removeFrom(slot);else{int n=layout.choiceAt(x,y,scroll,int(choices.size()));if(n>=0&&activeTab>=0){auto&items=books[activeTab].items;for(int i=int(items.size())-1;i>=0;--i)if(items[i]==choices[n].index){removeFrom(i);break;}}}}
  else reserve(layout.choiceAt(x,y,scroll,int(choices.size())));return 0;
 }
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);HBRUSH bg=CreateSolidBrush(RGB(24,31,27));FillRect(dc,&client,bg);DeleteObject(bg);
  if(!geometryReady){EndPaint(w,&ps);return 0;}
  auto previous=SelectObject(dc,panelFont?panelFont:font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(237,219,151));RECT title=rect(3,0,141,23);
  DrawTextW(dc,activeTab==1?L"防御预定 5 格":L"建筑预定 5 格",-1,&title,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  const auto&b=books[activeTab];for(int i=0;i<construction::Capacity;++i){RECT line=rect(5,24+i*18,140,42+i*18);
   std::wstring text=std::to_wstring(i+1)+L"  "+(i<int(b.items.size())?nameOf(b.items[i]):L"—");if(i==0&&b.stage)text+=L" …";
   SetTextColor(dc,i<int(b.items.size())?RGB(255,231,145):RGB(105,117,104));DrawTextW(dc,text.c_str(),-1,&line,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  }
  for(int n=scroll;n<int(choices.size())&&n<scroll+layout.rows*2;++n){const auto&c=choices[n];if(c.slot<0||c.slot>=snap.typeCount||snap.types[c.slot].kind!=7||snap.types[c.slot].index!=c.index)continue;
   const auto&t=snap.types[c.slot];int x=(n-scroll)%2*72+6,y=(n-scroll)/2*66+construction::HeaderHeight;
   um::Type type{};type.category=3;memcpy(type.id,t.id,24);externalReader.readFresh(c.type+1750,type.cameo,24);type.cameo[24]=0;static um::Snapshot palette{};type.ptr=c.type;bool ready=icons.restore(type,palette);
   if(!ready){static uint8_t buffer[32+256*1024];auto read=[](uint32_t at,void*out,size_t size){return externalReader.readFresh(at,out,size);};auto resolve=[](uint32_t){return 0u;};if(um::readCameo(read,resolve,type,buffer,sizeof buffer)&&um::readCameoPalette(read,palette)){ready=true;icons.remember(type,palette);}}
   RECT icon=rect(x,y,x+60,y+48);
   if(ready){uint32_t pixels[60*48];for(int i=0;i<60*48;++i){uint32_t color=palette.palette[type.icon[i]];if(c.future){unsigned gray=(((color>>16)&255)*3+((color>>8)&255)*6+(color&255))/10;color=(gray<<16)|(gray<<8)|gray;}pixels[i]=color&0xFFFFFF;}
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=60;info.bmiHeader.biHeight=-48;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,icon.left,icon.top,icon.right-icon.left,icon.bottom-icon.top,0,0,60,48,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
   }else{HBRUSH empty=CreateSolidBrush(c.future?RGB(54,58,53):RGB(67,78,58));FillRect(dc,&icon,empty);DeleteObject(empty);SetTextColor(dc,c.future?RGB(145,150,142):RGB(228,223,188));DrawTextW(dc,(const wchar_t*)t.name,-1,&icon,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
   RECT name=rect(x-3,y+48,x+63,y+64);SetTextColor(dc,c.future?RGB(151,157,147):RGB(231,226,194));DrawTextW(dc,(const wchar_t*)t.name,-1,&name,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   std::wstring numbers;for(int i=0;i<int(b.items.size());++i)if(b.items[i]==c.index){if(!numbers.empty())numbers+=L" ";numbers+=std::to_wstring(i+1);}
   if(!numbers.empty()){RECT badge=rect(x,y+30,x+60,y+48);HBRUSH tag=CreateSolidBrush(RGB(123,87,20));FillRect(dc,&badge,tag);DeleteObject(tag);SetTextColor(dc,RGB(255,243,156));DrawTextW(dc,numbers.c_str(),-1,&badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  }SelectObject(dc,previous);EndPaint(w,&ps);return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
static void service(){
 uint32_t now=GetTickCount();if(refreshed&&uint32_t(now-refreshed)<100)return;refreshed=now;
 if(!extra.constructionQueue){reset();return;}if(!usable()){hide();return;}
 house=get<uint32_t>(0xA83D4C);if(house<0x10000){hide();return;}
 if(lastPID!=pid){reset();lastPID=pid;wchar_t image[32768]{};DWORD length=32768;if(externalReader.imagePath(image,&length))icons.open(image,std::filesystem::path(root)/L"cache"/L"production-icons");}
 for(int tab=0;tab<2;++tab){current[tab]=product(tab);currentID[tab]=current[tab]?get<uint32_t>(current[tab]+0x10):0;auto&b=books[tab];if(b.items.empty())continue;
  if(b.match!=commandMatch()||b.house!=house||get<uint32_t>(b.source+0x10)!=b.id||get<uint32_t>(b.source+0x21C)!=house||get<int>(b.source+0x6C)<=0||!get<uint8_t>(b.source+0x74)){cancel(tab);continue;}
  if(current[tab]&&currentID[tab]!=b.id){int made=get<int>(get<uint32_t>(current[tab]+0x520)+0xDF8,-1);
   if(b.stage==2&&made==b.head()){b.consume(current[tab],currentID[tab]);if(b.items.empty())cancel(tab);continue;}
   if(b.stage!=1){cancel(tab);notify(L"原生新生产已接管，后续预定已取消。");continue;}
  }
  if(b.stage==2){if(uint32_t(now-b.sent)>5000){cancel(tab);notify(L"预定已提交但尚未观察到生产，已停止跟踪，避免重复下单。");}continue;}
  if(b.stage==1){if(uint32_t(now-b.sent)>15000){cancel(tab);notify(L"预定请求等待超时，已取消后续预定。");}continue;}
  bool onMap=placed(b.source);
  // A paused/completed product stays in its factory. Cancellation removes it.
  // Allow a short placement transition; never replay the canceled chain.
  if(!current[tab]&&!onMap){if(!b.missing)b.missing=now;else if(uint32_t(now-b.missing)>=250){cancel(tab);notify(L"当前建筑已取消，后续预定已自动取消。");}continue;}b.missing=0;
  if(!onMap||int32_t(now-b.retry)<0||!nativeCameo(b.head(),tab)||current[tab])continue;
  int slot=typeSlot(b.head());if(slot<0){cancel(tab);continue;}
  if(!b.token){if(!++nextToken)++nextToken;b.token=nextToken;}
  if(queueCommand(ProduceReserved,slot,int(b.id),b.match,0,0,b.token)){b.stage=1;b.sent=now;++b.attempts;}
 }
 activeTab=get<int>(0x87F7E8+0x539C,-1);if(activeTab<0||activeTab>1||!current[activeTab]){hide();return;}
 RECT client{};POINT origin{};int width=get<int>(0xA8EB84),height=get<int>(0xA8EB88);
 if(!GetClientRect(game,&client)||!ClientToScreen(game,&origin)||!layout.set(width,height,int(client.right-client.left),int(client.bottom-client.top))){hide();return;}
 static int oldW=0,oldH=0,oldCW=0,oldCH=0;
 if(width!=oldW||height!=oldH||layout.clientWidth!=oldCW||layout.clientHeight!=oldCH){oldW=width;oldH=height;oldCW=layout.clientWidth;oldCH=layout.clientHeight;
  critical(L"Construction geometry: game="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L" client="+std::to_wstring(oldCW)+L"x"+std::to_wstring(oldCH)+L" panel="+std::to_wstring(layout.width())+L"x"+std::to_wstring(layout.height()));
 }
 refreshChoices();if(choices.empty()){hide();return;}
 int desired=std::max(8,layout.y(13));if(!panelFont||fontHeight!=desired){HFONT next=CreateFontW(-desired,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");if(next){if(panelFont)DeleteObject(panelFont);panelFont=next;fontHeight=desired;}}
 if(!window){WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"RA2ConstructionReserve108";cls.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,cls.lpszClassName,L"建筑预定",WS_POPUP,0,0,layout.width(),layout.height(),nullptr,nullptr,cls.hInstance,nullptr);}
 if(window){geometryReady=true;SetWindowPos(window,HWND_TOPMOST,origin.x+layout.left(),origin.y+layout.top(),layout.width(),layout.height(),SWP_NOACTIVATE|SWP_SHOWWINDOW);InvalidateRect(window,nullptr,FALSE);}
}
static void destroy(){if(window)DestroyWindow(window);window=nullptr;if(panelFont)DeleteObject(panelFont);panelFont=nullptr;fontHeight=0;reset();}
}
