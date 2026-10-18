#include "../../shared/source/click_burst.h"
// Tactical host only. Physical input uses the common manager's SendInput pair.
static clickinput::Burst shiftBurst;
static void shiftClickTick(){
 static bool initialized=false,previous=false,known=false,nativeFive=false;
 static DWORD detectedPID=0;static uint32_t context=0;static bool visible=false;static int width=0,height=0;
 const uint32_t now=GetTickCount();bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
 POINT point{};GetCursorPos(&point);
 if(!initialized){initialized=true;previous=down;return;}
 if(pid!=detectedPID){detectedPID=pid;known=nativeFive=false;shiftBurst.cancel();
  HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
  if(modules!=INVALID_HANDLE_VALUE){MODULEENTRY32W m{sizeof m};if(Module32FirstW(modules,&m)){known=true;do{std::wstring name=m.szModule;for(auto&c:name)c=towlower(c);if(name.find(L"ares")!=std::wstring::npos)nativeFive=true;}while(Module32NextW(modules,&m));}CloseHandle(modules);}
 }
 bool eligible=enabled&&shiftClickEnabled&&shiftClickMode!=2&&pid&&foregroundGame()&&!(snap.chat)&&
  (shiftClickMode==1||(known&&!nativeFive));
 if(eligible&&(!context||uint32_t(now-context)>=32)){
  context=now;visible=external::display::inspect([](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);}).visible();
  width=height=0;externalReader.readFresh(0xA8EB84,&width,4);externalReader.readFresh(0xA8EB88,&height,4);
 }
 eligible=eligible&&visible&&width>=320&&height>=200&&
  (GetAsyncKeyState(VK_SHIFT)&0x8000)&&!((GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_MENU))&0x8000);
 RECT client{};POINT p=point;
 if(eligible){eligible=GetClientRect(game,&client)&&client.right>0&&client.bottom>0&&ScreenToClient(game,&p);
  if(eligible){int x=int(int64_t(p.x)*width/client.right),y=int(int64_t(p.y)*height/client.bottom);eligible=x>=width-shiftSidebarWidth&&x<width&&y>=shiftSidebarTop&&y<height;}
 }
 if(down!=previous){previous=down;if(down)shiftBurst.down(point.x,point.y,eligible);else shiftBurst.up(point.x,point.y,eligible,now,unsigned(shiftClickGap));}
 if(!eligible||((GetAsyncKeyState(VK_RBUTTON)|GetAsyncKeyState(VK_MBUTTON)|GetAsyncKeyState(VK_XBUTTON1)|GetAsyncKeyState(VK_XBUTTON2))&0x8000)||std::abs(point.x-shiftBurst.x)>4||std::abs(point.y-shiftBurst.y)>4){shiftBurst.cancel();return;}
 if(down)return;
 for(int n=0;n<4&&shiftBurst.ready(GetTickCount());++n){auto sent=gameinput::clickPair(game,pid);if(sent.sent!=2){shiftBurst.cancel();critical(L"Shift5 SendInput failed Win32="+std::to_wstring(sent.error));break;}shiftBurst.sent(GetTickCount(),unsigned(shiftClickGap));if(shiftClickGap)break;}
}
