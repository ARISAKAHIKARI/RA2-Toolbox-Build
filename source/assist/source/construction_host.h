#include "construction_catalog.h"
#include "native_cameos.h"
#include "construction_buffer.h"
#include "../../unit_monitor/source/icon_cache.h"
#include "../../unit_monitor/source/cameo_reader.h"
// Host-owned FIFO. GameRuntime remains the only writer/command constructor.
namespace buildreserve {
struct Choice {int slot=0,index=0,tab=0;uint32_t type=0;bool future=false;};
static construction::Book books[4];static std::vector<Choice> choices;static HWND window;
static uint32_t current[4],currentID[4],lastPID=0,house,refreshed=0,nextToken=0;
static int activeTab=-1,selectedTab=-1,lastNativeTab=-1,scroll=0,hoverChoice=-1,hoverQueue=-1;static um::IconCache& icons=um::sharedIconCache();static construction::Layout layout;
static construction::NativeFrame nativeFrame;static uint32_t nativeAt=0;
static HFONT panelFont;static int fontHeight=0;static bool geometryReady=false;
static construction::PanelBuffer panelBuffer;
template<class T>static T get(uint32_t at,T fallback={}){T v{};return externalReader.readFresh(at,&v,sizeof v)?v:fallback;}
static int encode(int kind,int index){return kind==7?index:(kind<<16)|index;}
static int absKind(int index){return index<65536?7:index>>16;}
static int typeIndex(int index){return index&65535;}
static uint32_t typeArray(int index){switch(absKind(index)){case 7:return 0xA83C68;case 16:return 0xA8E348;case 40:return 0xA83CE0;case 3:return 0xA8B218;default:return 0;}}
static uint32_t typePointer(int index){uint32_t a=typeArray(index),v=get<uint32_t>(a+4);int count=get<int>(a+16,-1),i=typeIndex(index);return a&&v>=0x10000&&count>0&&count<=4096&&i<count?get<uint32_t>(v+i*4):0;}
static uint32_t factoryOffset(int index,int tab){int kind=absKind(index);return kind==3?0x53AC:kind==16?0x53B0:kind==40?(get<uint8_t>(typePointer(index)+0xCCE)?0x53B8:0x53B4):(tab?0x53CC:0x53BC);}
static uint32_t factory(int tab){return get<uint32_t>(house+(tab?0x53CC:0x53BC));}
static uint32_t product(int tab){uint32_t f=factory(tab);return f>=0x10000&&get<uint32_t>(f+0x6C)==house?get<uint32_t>(f+0x58):0;}
static bool nativeCameo(int index,int tab){uint32_t strip=0x87F7E8+0x1544+tab*3988;int n=get<int>(strip+84,-1);if(n<0||n>75)return false;for(int i=0;i<n;++i)if(get<int>(strip+88+i*52)==typeIndex(index)&&get<int>(strip+92+i*52)==absKind(index))return true;return false;}
static bool factoryState(int tab,uint32_t&f,uint32_t&p){
 f=p=0;if(!externalReader.readFresh(house+(tab?0x53CC:0x53BC),&f,4))return false;if(!f)return true;
 if(f<0x10000)return false;uint32_t owner=0;
 uint32_t after=0,productAfter=0;
 return externalReader.readFresh(f+0x6C,&owner,4)&&owner==house&&externalReader.readFresh(f+0x58,&p,4)&&
  externalReader.readFresh(house+(tab?0x53CC:0x53BC),&after,4)&&after==f&&externalReader.readFresh(f+0x58,&productAfter,4)&&productAfter==p;
}
static void queueLog(int tab,const wchar_t*reason){const auto&b=books[tab];critical(L"Construction queue: "+std::wstring(reason)+L" tab="+std::to_wstring(tab)+L" match="+std::to_wstring(b.match)+L" parent="+std::to_wstring(b.id)+L" stage="+std::to_wstring(b.stage)+L" head="+std::to_wstring(b.head())+L" count="+std::to_wstring(b.items.size()));}
static bool placed(uint32_t p){return !get<uint8_t>(p+0x81,1)&&get<uint8_t>(p+0x90)&&get<uint8_t>(p+0x74);}
static void hide(){publishReservationPolicy({});geometryReady=false;nativeFrame={};nativeAt=0;if(window)ShowWindow(window,SW_HIDE);}
static void cancel(int tab){auto&b=books[tab];for(int i=0;i<commandQueue.size;)if((commandQueue.slots[i].command==ProduceReserved||commandQueue.slots[i].command==ProduceDeferred)&&commandQueue.slots[i].token==b.token)commandQueue.discard(i);else ++i;b={};}
static void reset(){for(int tab=0;tab<4;++tab){cancel(tab);current[tab]=currentID[tab]=0;}choices.clear();activeTab=selectedTab=lastNativeTab=-1;refreshed=0;hide();}
static int typeSlot(int index){for(int i=0;i<snap.typeCount;++i)if(snap.types[i].kind==absKind(index)&&snap.types[i].index==typeIndex(index))return i;return -1;}
static std::wstring nameOf(int index){int slot=typeSlot(index);return slot>=0?std::wstring((const wchar_t*)snap.types[slot].name):L"未知类型";}
static void result(int status,int count,uint32_t id,uint32_t token){for(int tab=0;tab<4;++tab){auto&b=books[tab];if(b.id!=id||b.token!=token||!token||b.stage!=1)continue;
 if(status>0&&count>0&&tab>=2){b.consume(0,0);if(b.items.empty())cancel(tab);else b.retry=GetTickCount()+100;queueLog(tab,L"accepted-unit-production");}
 else if(status>0&&count>0){b.stage=2;b.sent=GetTickCount();queueLog(tab,L"accepted-awaiting-product");}
 else if(status==-9){b.stage=0;b.retry=GetTickCount()+1000;if(b.attempts==1)queueLog(tab,L"waiting-native-prerequisite-or-factory");}
 else if((status==-6||status==-7||status==-3)&&b.attempts<3){b.stage=0;b.retry=GetTickCount()+250;}
 else{queueLog(tab,L"submission-rejected");cancel(tab);notify(L"预定未提交，已取消后续预定；原版生产不受影响。");}break;
}}
template<class C>static bool loadForecast(C&catalogue){
 std::vector<int> building;
 for(int tab=0;tab<2;++tab){uint32_t p=current[tab];if(p&&!placed(p)){int i=get<int>(get<uint32_t>(p+0x520)+0xDF8,-1);if(i>=0)building.push_back(i);}}
 if(!catalogue.load(house,building))return false;
 // FIFO prerequisites may cross building/defense tabs; only extend reachable
 // chains. Never count a blocked reservation as its own prerequisite.
 size_t at[2]{};for(int pass=0;pass<10;++pass){bool changed=false;
  for(int tab=0;tab<2;++tab){auto&b=books[tab];if(b.match!=commandMatch()||b.house!=house)continue;
   if(at[tab]<b.items.size()&&catalogue.append(b.items[at[tab]])){++at[tab];changed=true;}}
  if(!changed)break;
 }return true;
}
static void collectChoices(){
 choices.clear();if(activeTab<0||activeTab>3)return;
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};construction::Catalogue<decltype(read)> catalogue(read);
 bool previewReady=loadForecast(catalogue);auto&b=books[activeTab];
 for(int slot=0;slot<snap.typeCount;++slot){const auto&t=snap.types[slot];int index=encode(t.kind,t.index);uint32_t type=typePointer(index);if(type<0x10000)continue;
  int tab=t.kind==7?(get<int>(type+0xE08,-1)==5?1:0):t.kind==16?2:(t.kind==40||t.kind==3)?3:-1;
  if(tab!=activeTab)continue;bool native=nativeCameo(index,tab);
  bool queued=std::find(b.items.begin(),b.items.end(),index)!=b.items.end();
  bool future=previewReady&&catalogue.possible(type,t.kind,t.naval!=0)&&!catalogue.possible(type,t.kind,t.naval!=0,false);
  if(queued||(!native&&future)||(native&&tab<2&&current[tab]&&get<int>(factory(tab)+0x24)==54))choices.push_back({slot,index,tab,type,!native});
 }
}
static void resizePanel(){
 if(!window||!geometryReady)return;POINT origin{};RECT previous{};
 if(!ClientToScreen(game,&origin))return;
 const int x=origin.x+layout.left(),y=origin.y+layout.top(),w=layout.width(),h=layout.height();
 const bool visible=IsWindowVisible(window)!=0;
 if(visible&&GetWindowRect(window,&previous)&&previous.left==x&&previous.top==y&&previous.right-previous.left==w&&previous.bottom-previous.top==h)return;
 // Do not repeatedly raise a stationary panel above sibling overlays. On a
 // resize discard copied client pixels; the back buffer supplies a full frame.
 overlaystack::place(window,overlaystack::Layer::Construction,x,y,w,h,SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS|(visible?SWP_NOZORDER:SWP_SHOWWINDOW));
}
static bool usable();
static bool displayReady(){return extra.constructionQueue&&enabled&&foregroundGame()&&snap.renderStatus==1;}
static void refreshChoices(){
 collectChoices();layout.content(activeTab>=0&&activeTab<4?int(books[activeTab].items.size()):0,int(choices.size()));
 int maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll,0,maximum);
 construction::InputPolicy policy{};
 if(usable()&&activeTab>=0&&activeTab<2&&current[activeTab]&&!books[activeTab].stage){
  policy={commandMatch(),house,current[activeTab],currentID[activeTab],GetTickCount(),activeTab,uint32_t(construction::Capacity-books[activeTab].items.size())};
  policy.count=int(books[activeTab].items.size());for(int n=0;n<policy.count;++n)policy.queued[n]=books[activeTab].items[n];
 }
 publishReservationPolicy(policy);resizePanel();
}
static bool usable(){return displayReady()&&!snap.chat&&commandFresh(GetTickCount());}
static void reserveChoice(Choice c,bool fromNative=false,bool nativeCaptured=false){
 if(!usable()||activeTab<0||activeTab>3||c.tab!=activeTab)return;auto&b=books[activeTab];
 if(b.stage){notify(L"生产请求正在提交，请等原版队列更新后再预定。");return;}
 uint32_t p=activeTab<2?product(activeTab):0,identity=p?get<uint32_t>(p+0x10):0;
 if((p&&(p!=current[activeTab]||identity!=currentID[activeTab]))||get<uint32_t>(0xA83D4C)!=house)return;
 if(!b.items.empty()&&(b.match!=commandMatch()||b.house!=house||(b.source&&p&&b.id!=identity))){cancel(activeTab);return;}
 refreshChoices();bool valid=fromNative&&nativeCameo(c.index,c.tab)&&typeSlot(c.index)==c.slot;for(const auto&choice:choices)if(choice.index==c.index&&choice.type==c.type)valid=true;if(!valid)return;
 if(!b.add(c.index)){notify(L"本类预定已满（5 项）；右键叠放区取消最上层，Shift＋右键清空。");return;}
 if(b.items.size()==1){b.match=commandMatch();b.house=house;b.source=p;b.id=identity;}
 if(!fromNative||nativeCaptured)requestReservationSound();
 queueLog(activeTab,fromNative?L"reserved-native-click":L"reserved-extra-panel");
 refreshChoices();if(window)InvalidateRect(window,nullptr,FALSE);
}
static void reserve(int item){if(item>=0&&item<int(choices.size()))reserveChoice(choices[item]);}
static bool readNative(){nativeAt=GetTickCount();return construction::readNative([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);},nativeFrame);}
// Cancel only host reservations. Already-issued native Produce events cannot
// be recalled; retain that head until its product is observed, clear the tail.
static void removeItem(int tab,int item,bool all=false){
 if(!usable()||tab<0||tab>3)return;auto&b=books[tab];if(b.items.empty()||item<0||item>=int(b.items.size()))return;
 const size_t before=b.items.size();bool locked=false;if(b.stage){locked=true;for(int i=0;i<commandQueue.size;++i)if((commandQueue.slots[i].command==ProduceReserved||commandQueue.slots[i].command==ProduceDeferred)&&commandQueue.slots[i].token==b.token)locked=false;}
 if(all){if(locked)b.items.resize(1);else cancel(tab);}
 else if(item==0&&locked){notify(L"这一项已进入原版提交，待生产确认后可在原生侧栏取消。");return;}
 else if(item==0&&b.stage){for(int i=0;i<commandQueue.size;)if((commandQueue.slots[i].command==ProduceReserved||commandQueue.slots[i].command==ProduceDeferred)&&commandQueue.slots[i].token==b.token)commandQueue.discard(i);else ++i;b.stage=b.attempts=0;b.token=b.sent=b.retry=0;b.items.erase(b.items.begin());if(b.items.empty())cancel(tab);}
 else{b.items.erase(b.items.begin()+item);if(b.items.empty())cancel(tab);}
 if(b.items.size()<before)requestReservationSound();
 queueLog(tab,all?L"user-cleared-reservations":L"user-canceled-reservation");refreshChoices();if(window)InvalidateRect(window,nullptr,FALSE);

}
static void removeTop(bool all=false){if(activeTab>=0&&activeTab<4)removeItem(activeTab,int(books[activeTab].items.size())-1,all);}
static bool nativePointer(const gameinput::PointerPress&e){
 if(!usable()||e.match!=commandMatch()||uint32_t(GetTickCount()-e.tick)>100)return false;
 RECT client{};if(!GetClientRect(game,&client)||client.right!=e.width||client.bottom!=e.height||e.width<1||e.height<1||!readNative())return false;
 int x=int(int64_t(e.point.x)*nativeFrame.width/e.width),y=int(int64_t(e.point.y)*nativeFrame.height/e.height);
 // Native reservation and cancellation are captured only by Runtime. Polling
 // merely excludes the sidebar from enhanced ground orders, never duplicates.
 return nativeFrame.hit(x,y)>=0;
}
static void consumeNativeClick(){
 construction::InputClick c;if(!takeReservationClick(c))return;
 if(!usable()||c.match!=commandMatch()||uint32_t(GetTickCount()-c.tick)>300||c.house!=house||c.tab!=activeTab||c.tab<0||c.tab>1||
    c.parent!=current[c.tab]||c.parentID!=currentID[c.tab])return;
 if(c.operation==2){auto&items=books[c.tab].items;for(int n=int(items.size())-1;n>=0;--n)if(items[n]==c.index){removeItem(c.tab,n,c.all!=0);break;}return;}
 if(c.operation!=1)return;
 int slot=typeSlot(c.index);uint32_t types=get<uint32_t>(0xA83C68+4);int count=get<int>(0xA83C68+16,-1);
 if(slot>=0&&types>=0x10000&&c.index>=0&&c.index<count&&count<=4096){uint32_t type=get<uint32_t>(types+c.index*4);if(type>=0x10000)reserveChoice({slot,c.index,c.tab,type,false},true,true);}
}
static std::vector<construction::NativeBadge> nativeBadges(){
 std::vector<construction::NativeBadge> out;if(!displayReady()||get<uint32_t>(0xA83D4C)!=house)return out;
 if(!nativeAt||uint32_t(GetTickCount()-nativeAt)>=16)readNative();
 int tab=nativeFrame.tab;if(tab<0||tab>1)return out;auto&b=books[tab];if(b.match!=commandMatch()||b.house!=house)return out;
 for(const auto&c:nativeFrame.cells){int mask=0;for(int i=0;i<int(b.items.size());++i)if(b.items[i]==c.index)mask|=1<<i;
  if(mask)out.push_back({c.x,c.y+c.h-16,c.w,16,mask});}
 return out;
}
static external::ReadSession paintReader;static external::cameraassets::Context<external::ReadSession> paintAssets(paintReader);
static bool loadPanelAsset(const char*n,std::vector<uint8_t>&b){return paintAssets.cameo(n,b);}
struct PaintRequest {uint32_t pid,match,tick;int activeTab,scroll,hoverChoice,hoverQueue,width,height;construction::Layout layout;construction::Book book;std::vector<Choice> choices;std::map<int,assist::Type> types;std::map<int,uint32_t> pointers;LOGFONTW font{};};
struct Painted {PaintRequest request;std::vector<uint32_t> pixels;};
struct PanelPainter {
 const PaintRequest&request;construction::Layout layout;external::ReadSession&externalReader;external::cameraassets::Context<external::ReadSession>&assets;
RECT rect(int x,int y,int r,int bottom){return {layout.x(x),layout.y(y),layout.x(r),layout.y(bottom)};}
void paintIcon(HDC dc,int index,int x,int y,bool gray){
 auto found=request.types.find(index);if(found==request.types.end())return;const auto&t=found->second;
 uint32_t ptr=request.pointers.at(index);
 um::Type type{};type.category=t.kind==7?3:t.kind==16?1:t.kind==40?2:0;memcpy(type.id,t.id,24);type.id[23]=0;if(ptr>=0x10000)externalReader.readFresh(ptr+1750,type.cameo,24);type.cameo[24]=0;type.ptr=ptr;assets.resolveCameo(type);
 static thread_local um::Snapshot palette{};palette.paletteReady=0;bool ready=icons.restore(type,palette);
 if(!ready&&ptr>=0x10000){static thread_local uint8_t buffer[32+256*1024];auto read=[&](uint32_t at,void*out,size_t size){return externalReader.readFresh(at,out,size);};auto resolve=[](uint32_t){return 0u;};if(um::readCameo(read,resolve,type,buffer,sizeof buffer,loadPanelAsset)&&um::readCameoPalette(read,palette,loadPanelAsset)){ready=true;icons.remember(type,palette);}}
 RECT icon=rect(x,y,x+60,y+48);
 if(ready){uint32_t pixels[60*48];for(int i=0;i<60*48;++i){uint32_t color=palette.palette[type.icon[i]];if(gray){unsigned value=(((color>>16)&255)*3+((color>>8)&255)*6+(color&255))/10;color=(value<<16)|(value<<8)|value;}pixels[i]=color&0xFFFFFF;}
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=60;info.bmiHeader.biHeight=-48;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,icon.left,icon.top,icon.right-icon.left,icon.bottom-icon.top,0,0,60,48,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
 }else{HBRUSH empty=CreateSolidBrush(gray?RGB(35,42,50):RGB(40,61,79));FillRect(dc,&icon,empty);DeleteObject(empty);SetTextColor(dc,RGB(217,229,237));DrawTextW(dc,(const wchar_t*)t.name,-1,&icon,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
}
void panelBox(HDC dc,RECT r,COLORREF fill,COLORREF edge,int radius=4){
 HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,std::max(1,layout.x(1)),edge);
 auto oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,pen);
 RoundRect(dc,r.left,r.top,r.right,r.bottom,layout.x(radius),layout.y(radius));
 SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
}
};
static asyncwork::Latest<PaintRequest,Painted> paintWorker([](const PaintRequest&request)->std::shared_ptr<const Painted>{
 if(!paintReader.open(request.pid))return {};paintReader.begin();paintAssets.identity(request.pid,request.match);
 int width=request.width,height=request.height;if(width<1||height<1||uint64_t(width)*height>4*1024*1024)return {};
 auto out=std::make_shared<Painted>();out->request=request;out->pixels.resize(size_t(width)*height);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void*bits=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return {};}
 auto old=SelectObject(dc,bitmap);HFONT font=CreateFontIndirectW(&request.font);RECT client{0,0,width,height};auto layout=request.layout;const auto&choices=request.choices;int activeTab=request.activeTab,scroll=request.scroll,hoverChoice=request.hoverChoice,hoverQueue=request.hoverQueue;
 PanelPainter painter{request,layout,paintReader,paintAssets};
  HBRUSH background=CreateSolidBrush(RGB(17,26,37));FillRect(dc,&client,background);DeleteObject(background);
  painter.panelBox(dc,client,RGB(17,26,37),RGB(58,75,94),8);
  auto previous=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
  const wchar_t* tabs[]={L"建筑",L"防御",L"步兵",L"载具"};
  for(int tab=0;tab<4;++tab){
   RECT r=painter.rect(tab*36+2,2,(tab+1)*36-2,21);
   if(tab==activeTab)painter.panelBox(dc,r,RGB(35,60,76),RGB(70,145,164));
   SetTextColor(dc,tab==activeTab?RGB(220,246,247):RGB(143,162,179));DrawTextW(dc,tabs[tab],-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  }
  const auto&b=request.book;
  // Paint oldest first so the newest reservation remains on top.
  for(int i=0;i<int(b.items.size());++i){int y=construction::StackTop+i*construction::StackStep;
   painter.panelBox(dc,painter.rect(5,y,139,y+48),i==hoverQueue?RGB(35,55,70):RGB(26,40,54),i==hoverQueue?RGB(97,190,199):RGB(53,75,94));
   painter.paintIcon(dc,b.items[i],42,y,false);RECT number=painter.rect(10,y+3,34,y+22);SetTextColor(dc,RGB(147,220,224));
   std::wstring label=std::to_wstring(i+1);DrawTextW(dc,label.c_str(),-1,&number,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
   if(i==0&&b.stage)for(int dot=0;dot<3;++dot){RECT r=painter.rect(112+dot*5,y+10,115+dot*5,y+13);HBRUSH h=CreateSolidBrush(RGB(229,186,100));FillRect(dc,&r,h);DeleteObject(h);}
  }
  if(b.items.empty()){
   painter.panelBox(dc,painter.rect(5,26,139,54),RGB(20,32,44),RGB(43,62,80));
   HPEN plus=CreatePen(PS_SOLID,std::max(1,layout.x(1)),RGB(104,133,153));auto old=SelectObject(dc,plus);
   MoveToEx(dc,layout.x(67),layout.y(40),nullptr);LineTo(dc,layout.x(77),layout.y(40));
   MoveToEx(dc,layout.x(72),layout.y(35),nullptr);LineTo(dc,layout.x(72),layout.y(45));SelectObject(dc,old);DeleteObject(plus);
  }
  for(int i=0;i<construction::Capacity;++i){RECT dot=painter.rect(55+i*7,layout.stackBottom()+3,59+i*7,layout.stackBottom()+5);HBRUSH h=CreateSolidBrush(i<int(b.items.size())?RGB(105,196,197):RGB(47,65,82));FillRect(dc,&dot,h);DeleteObject(h);}
  for(int n=scroll;n<int(choices.size())&&n<scroll+layout.rows*2;++n){const auto&c=choices[n];auto found=request.types.find(c.index);if(found==request.types.end())continue;
   const auto&t=found->second;int x=(n-scroll)%2*72+6,y=(n-scroll)/2*66+layout.headerHeight();
   painter.panelBox(dc,painter.rect(x-3,y-1,x+63,y+65),n==hoverChoice?RGB(36,56,72):RGB(23,36,49),n==hoverChoice?RGB(109,200,205):RGB(43,62,80));
   painter.paintIcon(dc,c.index,x,y,c.future);
   RECT name=painter.rect(x-3,y+48,x+63,y+64);SetTextColor(dc,c.future?RGB(131,146,157):RGB(217,229,237));DrawTextW(dc,(const wchar_t*)t.name,-1,&name,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   std::wstring numbers;for(int i=0;i<int(b.items.size());++i)if(b.items[i]==c.index){if(!numbers.empty())numbers+=L" ";numbers+=std::to_wstring(i+1);}
   if(!numbers.empty()){RECT badge=painter.rect(x,y+30,x+60,y+48);HBRUSH tag=CreateSolidBrush(RGB(29,89,99));FillRect(dc,&badge,tag);DeleteObject(tag);SetTextColor(dc,RGB(208,247,240));DrawTextW(dc,numbers.c_str(),-1,&badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  }SelectObject(dc,previous);GdiFlush();
 memcpy(out->pixels.data(),bits,out->pixels.size()*4);SelectObject(dc,old);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);return out;
});
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_MOUSEMOVE&&geometryReady){
  int x=short(LOWORD(lp)),y=short(HIWORD(lp));int choice=layout.choiceAt(x,y,scroll,int(choices.size()));
  int queue=activeTab>=0&&activeTab<4?layout.queueAt(x,y,int(books[activeTab].items.size())):-1;
  if(choice!=hoverChoice||queue!=hoverQueue){hoverChoice=choice;hoverQueue=queue;InvalidateRect(w,nullptr,FALSE);}
  TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,w,0};TrackMouseEvent(&track);return 0;
 }
 if(msg==WM_MOUSELEAVE){hoverChoice=hoverQueue=-1;InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_MOUSEWHEEL){if(!geometryReady)return 0;int delta=short(HIWORD(wp)),maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll+(delta<0?2:-2),0,maximum);InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP){if(!geometryReady||!usable())return 0;int x=short(LOWORD(lp)),y=short(HIWORD(lp));
  if(msg==WM_LBUTTONUP&&y>=0&&y<layout.y(24)&&x>=0&&x<layout.width()){
   selectedTab=std::min(3,int(int64_t(x)*4/layout.width()));activeTab=selectedTab;scroll=0;refreshChoices();InvalidateRect(w,nullptr,FALSE);return 0;}
  if(msg==WM_RBUTTONUP){if(activeTab<0||activeTab>3)return 0;bool all=(wp&MK_SHIFT)!=0;int slot=layout.queueAt(x,y,int(books[activeTab].items.size()));if(slot>=0)removeTop(all);else{int n=layout.choiceAt(x,y,scroll,int(choices.size()));if(n>=0){auto&items=books[activeTab].items;for(int i=int(items.size())-1;i>=0;--i)if(items[i]==choices[n].index){removeItem(activeTab,i,all);break;}}}}
  else reserve(layout.choiceAt(x,y,scroll,int(choices.size())));return 0;
 }
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC target=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);
  if(!geometryReady||activeTab<0||activeTab>3){EndPaint(w,&ps);return 0;}
  PaintRequest request{pid,commandMatch(),GetTickCount(),activeTab,scroll,hoverChoice,hoverQueue,int(client.right),int(client.bottom),layout,books[activeTab],choices,{},{},{}};
  GetObjectW(panelFont?panelFont:font,sizeof request.font,&request.font);
  auto add=[&](int index){int slot=typeSlot(index);if(slot>=0){request.types[index]=snap.types[slot];request.pointers[index]=typePointer(index);}};
  for(int index:request.book.items)add(index);for(int n=scroll;n<int(choices.size())&&n<scroll+layout.rows*2;++n)add(choices[n].index);
  paintWorker.submit(std::move(request));auto completed=paintWorker.get();
  if(completed&&completed->request.pid==pid&&completed->request.match==commandMatch()&&completed->request.width==client.right&&completed->request.height==client.bottom&&completed->request.activeTab==activeTab&&completed->request.scroll==scroll){
   BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=client.right;info.bmiHeader.biHeight=-client.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
   SetDIBitsToDevice(target,0,0,client.right,client.bottom,0,0,0,client.bottom,completed->pixels.data(),&info,DIB_RGB_COLORS);
  }else{HBRUSH background=CreateSolidBrush(RGB(17,26,37));FillRect(target,&client,background);DeleteObject(background);}
  EndPaint(w,&ps);return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
static void service(){
 // Observe every service tick, before the slower catalogue refresh. Native
 // unavailable tabs may ignore the key, but the reservation tab still follows.
 static bool previous[4]{};uint8_t keys[256]{};nativekeyhost::down(keys);int requested=-1;
 const auto&bindings=nativekeyhost::current();
 for(int tab=0;tab<4;++tab){bool down=bindings.held(nativekeys::Command(tab),keys);if(extra.constructionQueue&&usable()&&down&&!previous[tab])requested=tab;previous[tab]=down;}
 if(requested>=0){selectedTab=requested;activeTab=requested;lastNativeTab=get<int>(0x87F7E8+0x539C,-1);scroll=0;refreshed=0;}
 consumeNativeClick();uint32_t now=GetTickCount();if(refreshed&&uint32_t(now-refreshed)<100)return;refreshed=now;
 if(!extra.constructionQueue){reset();return;}if(!displayReady()){hide();return;}
 house=get<uint32_t>(0xA83D4C);if(house<0x10000){hide();return;}
 if(lastPID!=pid){reset();lastPID=pid;wchar_t image[32768]{};DWORD length=32768;if(externalReader.imagePath(image,&length))icons.open(image,std::filesystem::path(root)/L"cache"/L"icons");}
 for(int tab=0;tab<4;++tab){auto&b=books[tab];
  if(!b.items.empty()&&(b.match!=commandMatch()||b.house!=house)){queueLog(tab,L"match-or-owner-changed");cancel(tab);}
  uint32_t f=0,p=0;
  if(tab<2){if(!factoryState(tab,f,p))continue;}
  else if(!b.items.empty()){f=get<uint32_t>(house+factoryOffset(b.head(),tab));if(f>=0x10000&&get<uint32_t>(f+0x6C)==house)p=get<uint32_t>(f+0x58);}

  uint32_t identity=0;if(p&&!externalReader.readFresh(p+0x10,&identity,4))continue;
  current[tab]=p;currentID[tab]=identity;
  // Display follows read-only state. Command suspension closes input policy
  // and freezes submission/timeout handling without hiding the existing FIFO.
  if(b.items.empty()||!usable())continue;
  if(tab<2&&current[tab]&&currentID[tab]!=b.id){int made=get<int>(get<uint32_t>(current[tab]+0x520)+0xDF8,-1);
   if(b.stage==2&&made==b.head()){b.consume(current[tab],currentID[tab]);if(b.items.empty())cancel(tab);continue;}
   if(b.stage!=1&&b.source){queueLog(tab,L"native-product-replaced");cancel(tab);notify(L"原生新生产已接管，后续预定已取消。");continue;}
  }
  if(b.stage==2){if(uint32_t(now-b.sent)>5000){queueLog(tab,L"acceptance-observation-timeout");cancel(tab);notify(L"预定已提交但尚未观察到生产，已停止跟踪，避免重复下单。");}continue;}
  if(b.stage==1){if(uint32_t(now-b.sent)>15000){queueLog(tab,L"submission-timeout");cancel(tab);notify(L"预定请求等待超时，已取消后续预定。");}continue;}
  if(!b.source){
   if(int32_t(now-b.retry)<0||current[tab]||!nativeCameo(b.head(),tab))continue;
   if(!b.afterPlacement(get<int>(0xA8ED84,-1),true))continue;
   int slot=typeSlot(b.head());if(slot<0){cancel(tab);continue;}
   if(!b.token){if(!++nextToken)++nextToken;b.token=nextToken;}
   if(queueCommand(ProduceDeferred,slot,0,b.match,0,0,b.token)){b.stage=1;b.sent=now;++b.attempts;}continue;
  }
  uint8_t state[0x220]{};if(!externalReader.readFresh(b.source,state,sizeof state))continue;
  uint32_t sourceID=0,owner=0;int health=0;memcpy(&sourceID,state+0x10,4);memcpy(&owner,state+0x21C,4);memcpy(&health,state+0x6C,4);
  if(sourceID!=b.id||owner!=house){queueLog(tab,L"parent-identity-changed");cancel(tab);continue;}
  // Factory products are in limbo, not live map objects. Do not cancel a
  // future prerequisite reservation because IsAlive/Health is not yet live.
  const bool inFactory=current[tab]==b.source&&currentID[tab]==b.id;
  const bool onMap=!state[0x81]&&state[0x90]&&state[0x74]&&health>0;
  // A paused/completed product stays in its factory. Cancellation removes it.
  // Allow a short placement transition; never replay the canceled chain.
  if(!inFactory&&!onMap){if(!b.missing)b.missing=now;else if(uint32_t(now-b.missing)>=250){queueLog(tab,L"parent-canceled-or-lost");cancel(tab);notify(L"当前建筑已取消，后续预定已自动取消。");}continue;}b.missing=0;
  if(!b.afterPlacement(get<int>(0xA8ED84,-1),onMap&&!current[tab]))continue;
  if(int32_t(now-b.retry)<0||!nativeCameo(b.head(),tab))continue;
  int slot=typeSlot(b.head());if(slot<0){queueLog(tab,L"type-removed");cancel(tab);continue;}
  if(!b.token){if(!++nextToken)++nextToken;b.token=nextToken;}
  if(queueCommand(ProduceReserved,slot,int(b.id),b.match,0,0,b.token)){b.stage=1;b.sent=now;b.attempts=std::min(b.attempts+1,1000000);queueLog(tab,L"submitted-after-parent-placement");}
 }
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};construction::Catalogue<decltype(read)> forecast(read);
 if(usable()&&loadForecast(forecast))for(int tab=0;tab<4;++tab){auto&b=books[tab];if(b.items.empty()||b.stage||b.source)continue;
  int slot=typeSlot(b.head());if(slot<0||!forecast.possible(typePointer(b.head()),absKind(b.head()),get<uint8_t>(typePointer(b.head())+0xCCE)!=0)){
   if(!b.missing)b.missing=now;else if(uint32_t(now-b.missing)>=500){queueLog(tab,L"future-prerequisite-canceled");cancel(tab);notify(L"前置科技已取消或失效，对应预定已取消。");}
  }else b.missing=0;
 }
 int nativeTab=get<int>(0x87F7E8+0x539C,-1);if(nativeTab!=lastNativeTab){lastNativeTab=nativeTab;selectedTab=-1;}
 activeTab=selectedTab>=0?selectedTab:nativeTab;
 if(activeTab<0||activeTab>3){hide();return;}
 bool context=current[0]||current[1];for(const auto&b:books)context=context||!b.items.empty();
 if(!context){hide();return;}

 RECT client{};POINT origin{};int width=get<int>(0x8A00A4),height=get<int>(0x8A00A8);
 if(!GetClientRect(game,&client)||!ClientToScreen(game,&origin)||!layout.set(width,height,int(client.right-client.left),int(client.bottom-client.top),constructionScalePercent)){hide();return;}
 refreshChoices(); // Keep the panel while the last reserved product is still in production.
 static int oldW=0,oldH=0,oldCW=0,oldCH=0,oldScale=0;
 if(width!=oldW||height!=oldH||layout.clientWidth!=oldCW||layout.clientHeight!=oldCH||oldScale!=constructionScalePercent){oldW=width;oldH=height;oldCW=layout.clientWidth;oldCH=layout.clientHeight;oldScale=constructionScalePercent;
  critical(L"Construction geometry: game="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L" client="+std::to_wstring(oldCW)+L"x"+std::to_wstring(oldCH)+L" panel="+std::to_wstring(layout.width())+L"x"+std::to_wstring(layout.height())+L" scale="+std::to_wstring(oldScale));
 }
 int desired=std::max(8,layout.y(13));if(!panelFont||fontHeight!=desired){HFONT next=CreateFontW(-desired,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");if(next){if(panelFont)DeleteObject(panelFont);panelFont=next;fontHeight=desired;}}
 if(!window){WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"RA2ConstructionReserve110";cls.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"建筑预定",WS_POPUP,0,0,layout.width(),layout.height(),nullptr,nullptr,cls.hInstance,nullptr);if(window)SetLayeredWindowAttributes(window,0,construction::PanelOpacity,LWA_ALPHA);}
 if(window){geometryReady=true;resizePanel();InvalidateRect(window,nullptr,FALSE);}
}
static void destroy(){paintWorker.shutdown();if(window)DestroyWindow(window);window=nullptr;panelBuffer.clear();if(panelFont)DeleteObject(panelFont);panelFont=nullptr;fontHeight=0;reset();}
}

