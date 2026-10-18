#include "construction_catalog.h"
#include "native_cameos.h"
#include "../../unit_monitor/source/icon_cache.h"
#include "../../unit_monitor/source/cameo_reader.h"
// Host-owned FIFO. GameRuntime remains the only writer/command constructor.
namespace buildreserve {
struct Choice {int slot=0,index=0,tab=0;uint32_t type=0;bool future=false;};
static construction::Book books[2];static std::vector<Choice> choices;static HWND window;
static uint32_t current[2],currentID[2],lastPID=0,house,refreshed=0,nextToken=0;
static int activeTab=-1,scroll=0;static um::IconCache& icons=um::sharedIconCache();static construction::Layout layout;
static construction::NativeFrame nativeFrame;static uint32_t nativeAt=0;
static HFONT panelFont;static int fontHeight=0;static bool geometryReady=false;
template<class T>static T get(uint32_t at,T fallback={}){T v{};return externalReader.readFresh(at,&v,sizeof v)?v:fallback;}
static uint32_t factory(int tab){return get<uint32_t>(house+(tab?0x53CC:0x53BC));}
static uint32_t product(int tab){uint32_t f=factory(tab);return f>=0x10000&&get<uint32_t>(f+0x6C)==house?get<uint32_t>(f+0x58):0;}
static bool nativeCameo(int index,int tab){uint32_t strip=0x87F7E8+0x1544+tab*3988;int n=get<int>(strip+84,-1);if(n<0||n>75)return false;for(int i=0;i<n;++i)if(get<int>(strip+88+i*52)==index&&get<int>(strip+92+i*52)==7)return true;return false;}
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
static void cancel(int tab){auto&b=books[tab];for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==ProduceReserved&&commandQueue.slots[i].token==b.token)commandQueue.discard(i);else ++i;b={};}
static void reset(){cancel(0);cancel(1);choices.clear();current[0]=current[1]=currentID[0]=currentID[1]=0;activeTab=-1;refreshed=0;hide();}
static int typeSlot(int index){for(int i=0;i<snap.typeCount;++i)if(snap.types[i].kind==7&&snap.types[i].index==index)return i;return -1;}
static std::wstring nameOf(int index){int slot=typeSlot(index);return slot>=0?std::wstring((const wchar_t*)snap.types[slot].name):L"未知建筑";}
static void result(int status,int count,uint32_t id,uint32_t token){for(int tab=0;tab<2;++tab){auto&b=books[tab];if(b.id!=id||b.token!=token||!token||b.stage!=1)continue;
 if(status>0&&count>0){b.stage=2;b.sent=GetTickCount();queueLog(tab,L"accepted-awaiting-product");}
 else if(status==-9){b.stage=0;b.retry=GetTickCount()+1000;if(b.attempts==1)queueLog(tab,L"waiting-native-prerequisite-or-factory");}
 else if((status==-6||status==-7||status==-3)&&b.attempts<3){b.stage=0;b.retry=GetTickCount()+250;}
 else{queueLog(tab,L"submission-rejected");cancel(tab);notify(L"预定未提交，已取消后续预定；原版生产不受影响。");}break;
}}
static void collectChoices(){
 choices.clear();if(activeTab<0||activeTab>1||(!current[activeTab]&&books[activeTab].items.empty()))return;
 std::vector<int> future;uint32_t p=current[activeTab];if(p&&!placed(p))future.push_back(get<int>(get<uint32_t>(p+0x520)+0xDF8,-1));
 auto&b=books[activeTab];future.insert(future.end(),b.items.begin(),b.items.end());
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};construction::Catalogue<decltype(read)> catalogue(read);bool previewReady=catalogue.load(house,future);
 uint32_t types=get<uint32_t>(0xA83C68+4);int count=get<int>(0xA83C68+16,-1);if(types<0x10000||count<0||count>4096)return;
 std::vector<int> nativeIndices;uint32_t strip=0x87F7E8+0x1544+activeTab*3988;int cameoCount=get<int>(strip+84,-1);int entries[75*13]{};
 if(cameoCount>0&&cameoCount<=75&&externalReader.readFresh(strip+88,entries,size_t(cameoCount)*52))for(int i=0;i<cameoCount;++i)if(entries[i*13+1]==7)nativeIndices.push_back(entries[i*13]);
 for(int slot=0;slot<snap.typeCount;++slot){const auto&t=snap.types[slot];if(t.kind!=7||t.index<0||t.index>=count)continue;uint32_t type=get<uint32_t>(types+t.index*4);if(type<0x10000)continue;
  int tab=get<int>(type+0xE08,-1)==5?1:0;if(tab!=activeTab)continue;bool native=std::find(nativeIndices.begin(),nativeIndices.end(),t.index)!=nativeIndices.end();
  bool queued=std::find(b.items.begin(),b.items.end(),t.index)!=b.items.end();
  if((native&&queued)||(!native&&(queued||(previewReady&&catalogue.preview(t.index)))))choices.push_back({slot,t.index,tab,type,!native});
 }
}
static void resizePanel(){
 if(!window||!geometryReady)return;POINT origin{};
 if(ClientToScreen(game,&origin))SetWindowPos(window,HWND_TOPMOST,origin.x+layout.left(),origin.y+layout.top(),layout.width(),layout.height(),SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
static bool usable();
static void refreshChoices(){
 collectChoices();layout.content(activeTab>=0&&activeTab<2?int(books[activeTab].items.size()):0,int(choices.size()));
 int maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll,0,maximum);
 construction::InputPolicy policy{};
 if(usable()&&activeTab>=0&&activeTab<2&&current[activeTab]&&!books[activeTab].stage){
  policy={commandMatch(),house,current[activeTab],currentID[activeTab],GetTickCount(),activeTab,uint32_t(construction::Capacity-books[activeTab].items.size())};
  policy.count=int(books[activeTab].items.size());for(int n=0;n<policy.count;++n)policy.queued[n]=books[activeTab].items[n];
 }
 publishReservationPolicy(policy);resizePanel();
}
static bool usable(){return extra.constructionQueue&&enabled&&commandFresh(GetTickCount())&&foregroundGame()&&!snap.chat&&snap.renderStatus==1;}
static void reserveChoice(Choice c,bool fromNative=false,bool nativeCaptured=false){
 if(!usable()||activeTab<0||activeTab>1||c.tab!=activeTab)return;auto&b=books[activeTab];
 if(b.stage){notify(L"生产请求正在提交，请等原版队列更新后再预定。");return;}
 uint32_t p=product(activeTab),identity=get<uint32_t>(p+0x10);if(!p||p!=current[activeTab]||identity!=currentID[activeTab]||get<uint32_t>(0xA83D4C)!=house)return;
 if(!b.items.empty()&&(b.match!=commandMatch()||b.house!=house||b.id!=identity)){cancel(activeTab);return;}
 refreshChoices();bool valid=fromNative&&nativeCameo(c.index,c.tab)&&typeSlot(c.index)==c.slot;for(const auto&choice:choices)if(choice.index==c.index&&choice.type==c.type)valid=true;if(!valid)return;
 if(!b.add(c.index)){notify(L"本类预定已满（5 项）；右键叠放区取消最上层，Shift＋右键清空。");return;}
 if(b.items.size()==1){b.match=commandMatch();b.house=house;b.source=p;b.id=identity;}
 if(!fromNative||nativeCaptured)requestReservationSound();
 queueLog(activeTab,fromNative?L"reserved-native-click":L"reserved-extra-panel");
 refreshChoices();if(window)InvalidateRect(window,nullptr,FALSE);notify(L"已加入预定 "+std::to_wstring(b.items.size())+L"："+nameOf(c.index)+L"；放下当前建筑后依次继续生产。");
}
static void reserve(int item){if(item>=0&&item<int(choices.size()))reserveChoice(choices[item]);}
static bool readNative(){nativeAt=GetTickCount();return construction::readNative([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);},nativeFrame);}
// Cancel only host reservations. Already-issued native Produce events cannot
// be recalled; retain that head until its product is observed, clear the tail.
static void removeItem(int tab,int item,bool all=false){
 if(!usable()||tab<0||tab>1)return;auto&b=books[tab];if(b.items.empty()||item<0||item>=int(b.items.size()))return;
 const size_t before=b.items.size();bool locked=false;if(b.stage){locked=true;for(int i=0;i<commandQueue.size;++i)if(commandQueue.slots[i].command==ProduceReserved&&commandQueue.slots[i].token==b.token)locked=false;}
 if(all){if(locked)b.items.resize(1);else cancel(tab);}
 else if(item==0&&locked){notify(L"这一项已进入原版提交，待生产确认后可在原生侧栏取消。");return;}
 else if(item==0&&b.stage){for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==ProduceReserved&&commandQueue.slots[i].token==b.token)commandQueue.discard(i);else ++i;b.stage=b.attempts=0;b.token=b.sent=b.retry=0;b.items.erase(b.items.begin());if(b.items.empty())cancel(tab);}
 else{b.items.erase(b.items.begin()+item);if(b.items.empty())cancel(tab);}
 if(b.items.size()<before)requestReservationSound();
 queueLog(tab,all?L"user-cleared-reservations":L"user-canceled-reservation");refreshChoices();if(window)InvalidateRect(window,nullptr,FALSE);
 notify(all?(locked?L"后续预定已清空；已提交项交由原版生产确认。":L"本类预定已全部取消。"):L"已取消一项预定。");
}
static void removeTop(bool all=false){if(activeTab>=0&&activeTab<2)removeItem(activeTab,int(books[activeTab].items.size())-1,all);}
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
 std::vector<construction::NativeBadge> out;if(!usable()||get<uint32_t>(0xA83D4C)!=house)return out;
 if(!nativeAt||uint32_t(GetTickCount()-nativeAt)>=16)readNative();
 int tab=nativeFrame.tab;if(tab<0||tab>1)return out;auto&b=books[tab];if(b.match!=commandMatch()||b.house!=house)return out;
 for(const auto&c:nativeFrame.cells){int mask=0;for(int i=0;i<int(b.items.size());++i)if(b.items[i]==c.index)mask|=1<<i;
  if(mask)out.push_back({c.x,c.y+c.h-16,c.w,16,mask});}
 return out;
}
static RECT rect(int x,int y,int r,int bottom){return {layout.x(x),layout.y(y),layout.x(r),layout.y(bottom)};}
static void paintIcon(HDC dc,int index,int x,int y,bool gray){
 int slot=typeSlot(index);if(slot<0)return;const auto&t=snap.types[slot];
 uint32_t types=get<uint32_t>(0xA83C68+4);int count=get<int>(0xA83C68+16,-1);uint32_t ptr=types>=0x10000&&index>=0&&index<count&&count<=4096?get<uint32_t>(types+index*4):0;
 um::Type type{};type.category=3;memcpy(type.id,t.id,24);type.id[23]=0;if(ptr>=0x10000)externalReader.readFresh(ptr+1750,type.cameo,24);type.cameo[24]=0;type.ptr=ptr;
 static um::Snapshot palette{};palette.paletteReady=0;bool ready=icons.restore(type,palette);
 if(!ready&&ptr>=0x10000){static uint8_t buffer[32+256*1024];auto read=[](uint32_t at,void*out,size_t size){return externalReader.readFresh(at,out,size);};auto resolve=[](uint32_t){return 0u;};if(um::readCameo(read,resolve,type,buffer,sizeof buffer)&&um::readCameoPalette(read,palette)){ready=true;icons.remember(type,palette);}}
 RECT icon=rect(x,y,x+60,y+48);
 if(ready){uint32_t pixels[60*48];for(int i=0;i<60*48;++i){uint32_t color=palette.palette[type.icon[i]];if(gray){unsigned value=(((color>>16)&255)*3+((color>>8)&255)*6+(color&255))/10;color=(value<<16)|(value<<8)|value;}pixels[i]=color&0xFFFFFF;}
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=60;info.bmiHeader.biHeight=-48;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,icon.left,icon.top,icon.right-icon.left,icon.bottom-icon.top,0,0,60,48,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
 }else{HBRUSH empty=CreateSolidBrush(gray?RGB(54,58,53):RGB(67,78,58));FillRect(dc,&icon,empty);DeleteObject(empty);SetTextColor(dc,RGB(228,223,188));DrawTextW(dc,(const wchar_t*)t.name,-1,&icon,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_MOUSEWHEEL){if(!geometryReady)return 0;int delta=short(HIWORD(wp)),maximum=std::max(0,(int(choices.size())+1)/2-layout.rows)*2;scroll=std::clamp(scroll+(delta<0?2:-2),0,maximum);InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP){if(!geometryReady||!usable())return 0;int x=short(LOWORD(lp)),y=short(HIWORD(lp));
  if(msg==WM_RBUTTONUP){if(activeTab<0||activeTab>1)return 0;bool all=(wp&MK_SHIFT)!=0;int slot=layout.queueAt(x,y,int(books[activeTab].items.size()));if(slot>=0)removeTop(all);else{int n=layout.choiceAt(x,y,scroll,int(choices.size()));if(n>=0){auto&items=books[activeTab].items;for(int i=int(items.size())-1;i>=0;--i)if(items[i]==choices[n].index){removeItem(activeTab,i,all);break;}}}}
  else reserve(layout.choiceAt(x,y,scroll,int(choices.size())));return 0;
 }
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);HBRUSH bg=CreateSolidBrush(RGB(24,31,27));FillRect(dc,&client,bg);DeleteObject(bg);
  if(!geometryReady){EndPaint(w,&ps);return 0;}
  auto previous=SelectObject(dc,panelFont?panelFont:font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(237,219,151));RECT title=rect(3,0,141,23);
  DrawTextW(dc,activeTab==1?L"防御预定 5 格":L"建筑预定 5 格",-1,&title,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  const auto&b=books[activeTab];
  // Paint oldest first. Each later 48 px card overlaps the preceding card by
  // 24 px, so the latest reservation is physically on top.
  for(int i=0;i<int(b.items.size());++i){int y=construction::StackTop+i*construction::StackStep;RECT card=rect(5,y,139,y+48);HBRUSH bg=CreateSolidBrush(i%2?RGB(50,61,46):RGB(41,51,38));FillRect(dc,&card,bg);DeleteObject(bg);
   paintIcon(dc,b.items[i],42,y,false);RECT number=rect(8,y+2,40,y+23);SetTextColor(dc,RGB(255,231,145));std::wstring label=std::to_wstring(i+1);if(i==0&&b.stage)label+=L"…";DrawTextW(dc,label.c_str(),-1,&number,DT_SINGLELINE|DT_VCENTER);
  }
  if(b.items.empty()){RECT idle=rect(8,24,136,56);SetTextColor(dc,RGB(166,176,157));DrawTextW(dc,L"尚无预定\n点击建造图标追加",-1,&idle,DT_CENTER|DT_WORDBREAK);}
  RECT hint=rect(4,layout.stackBottom(),140,layout.headerHeight()-construction::SectionGap-1);SetTextColor(dc,RGB(180,188,167));DrawTextW(dc,L"右键退顶层 · Shift 清空",-1,&hint,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
  int divider=layout.headerHeight()-construction::SectionGap/2;
  RECT separator=rect(5,divider,139,divider+1);HBRUSH line=CreateSolidBrush(RGB(104,116,86));FillRect(dc,&separator,line);DeleteObject(line);
  for(int n=scroll;n<int(choices.size())&&n<scroll+layout.rows*2;++n){const auto&c=choices[n];if(c.slot<0||c.slot>=snap.typeCount||snap.types[c.slot].kind!=7||snap.types[c.slot].index!=c.index)continue;
   const auto&t=snap.types[c.slot];int x=(n-scroll)%2*72+6,y=(n-scroll)/2*66+layout.headerHeight();
   paintIcon(dc,c.index,x,y,c.future);
   RECT name=rect(x-3,y+48,x+63,y+64);SetTextColor(dc,c.future?RGB(151,157,147):RGB(231,226,194));DrawTextW(dc,(const wchar_t*)t.name,-1,&name,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   std::wstring numbers;for(int i=0;i<int(b.items.size());++i)if(b.items[i]==c.index){if(!numbers.empty())numbers+=L" ";numbers+=std::to_wstring(i+1);}
   if(!numbers.empty()){RECT badge=rect(x,y+30,x+60,y+48);HBRUSH tag=CreateSolidBrush(RGB(123,87,20));FillRect(dc,&badge,tag);DeleteObject(tag);SetTextColor(dc,RGB(255,243,156));DrawTextW(dc,numbers.c_str(),-1,&badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  }SelectObject(dc,previous);EndPaint(w,&ps);return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
static void service(){
 consumeNativeClick();uint32_t now=GetTickCount();if(refreshed&&uint32_t(now-refreshed)<100)return;refreshed=now;
 if(!extra.constructionQueue){reset();return;}if(!usable()){hide();return;}
 house=get<uint32_t>(0xA83D4C);if(house<0x10000){hide();return;}
 if(lastPID!=pid){reset();lastPID=pid;wchar_t image[32768]{};DWORD length=32768;if(externalReader.imagePath(image,&length))icons.open(image,std::filesystem::path(root)/L"cache"/L"icons");}
 for(int tab=0;tab<2;++tab){auto&b=books[tab];
  if(!b.items.empty()&&(b.match!=commandMatch()||b.house!=house)){queueLog(tab,L"match-or-owner-changed");cancel(tab);}
  uint32_t f=0,p=0;if(!factoryState(tab,f,p))continue;
  uint32_t identity=0;if(p&&!externalReader.readFresh(p+0x10,&identity,4))continue;
  current[tab]=p;currentID[tab]=identity;if(b.items.empty())continue;
  if(current[tab]&&currentID[tab]!=b.id){int made=get<int>(get<uint32_t>(current[tab]+0x520)+0xDF8,-1);
   if(b.stage==2&&made==b.head()){b.consume(current[tab],currentID[tab]);if(b.items.empty())cancel(tab);continue;}
   if(b.stage!=1){queueLog(tab,L"native-product-replaced");cancel(tab);notify(L"原生新生产已接管，后续预定已取消。");continue;}
  }
  if(b.stage==2){if(uint32_t(now-b.sent)>5000){queueLog(tab,L"acceptance-observation-timeout");cancel(tab);notify(L"预定已提交但尚未观察到生产，已停止跟踪，避免重复下单。");}continue;}
  if(b.stage==1){if(uint32_t(now-b.sent)>15000){queueLog(tab,L"submission-timeout");cancel(tab);notify(L"预定请求等待超时，已取消后续预定。");}continue;}
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
 activeTab=get<int>(0x87F7E8+0x539C,-1);if(activeTab<0||activeTab>1||(!current[activeTab]&&books[activeTab].items.empty())){hide();return;}
 RECT client{};POINT origin{};int width=get<int>(0xA8EB84),height=get<int>(0xA8EB88);
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
static void destroy(){if(window)DestroyWindow(window);window=nullptr;if(panelFont)DeleteObject(panelFont);panelFont=nullptr;fontHeight=0;reset();}
}
