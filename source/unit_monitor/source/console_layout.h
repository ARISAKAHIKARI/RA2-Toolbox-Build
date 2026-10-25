#pragma once
struct ConsoleControl {HWND window;RECT rect;};
static std::vector<ConsoleControl> consoleControls;
static unsigned consoleDpi=96;static int consoleX=0,consoleY=0;static bool consoleArranging=false;
static void layoutConsole(){
 if(!controlsReady||consoleArranging)return;consoleArranging=true;RECT client{};GetClientRect(ui,&client);
 auto px=[](int n){return modernui::scale(n,consoleDpi);};int width=px(700),height=px(440);
 for(const auto&c:consoleControls){width=std::max(width,px(c.rect.left+c.rect.right+20));height=std::max(height,px(c.rect.top+c.rect.bottom+12));}
 consoleX=std::clamp(consoleX,0,std::max(0,width-int(client.right)));consoleY=std::clamp(consoleY,0,std::max(0,height-int(client.bottom)));
 HDWP batch=BeginDeferWindowPos(int(consoleControls.size()));
 const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW;
 for(const auto&c:consoleControls){auto r=c.rect;if(batch)batch=DeferWindowPos(batch,c.window,nullptr,px(r.left)-consoleX,px(r.top)-consoleY,px(r.right),px(r.bottom),flags);}
 bool placed=batch&&EndDeferWindowPos(batch);if(!placed)for(const auto&c:consoleControls){auto r=c.rect;SetWindowPos(c.window,nullptr,px(r.left)-consoleX,px(r.top)-consoleY,px(r.right),px(r.bottom),flags);}
 SCROLLINFO info{sizeof info,SIF_RANGE|SIF_PAGE|SIF_POS};info.nMax=width-1;info.nPage=std::max(1L,client.right);info.nPos=consoleX;SetScrollInfo(ui,SB_HORZ,&info,TRUE);
 info.nMax=height-1;info.nPage=std::max(1L,client.bottom);info.nPos=consoleY;SetScrollInfo(ui,SB_VERT,&info,TRUE);
 consoleArranging=false;RedrawWindow(ui,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void consoleScale(unsigned dpi){
 dpi=modernui::panelDpi(dpi,false);consoleX=MulDiv(consoleX,int(dpi),int(consoleDpi));consoleY=MulDiv(consoleY,int(dpi),int(consoleDpi));consoleDpi=dpi;
 HFONT next=modernui::makeFont(modernui::BodyPixels,dpi);if(next){for(const auto&c:consoleControls){SendMessageW(c.window,WM_SETFONT,(WPARAM)next,FALSE);if(GetPropW(c.window,L"RA2.Heading.Font"))modernui::heading(c.window,dpi);}auto old=font;font=next;if(old)DeleteObject(old);}layoutConsole();
}
