// Host-only visible radar mirror. No game-state change, extra hook or synthetic
// input. The source stays unobstructed; the enlarged window is click-through.
namespace minimap {
static HWND window;static HDC captureDC;static HBITMAP image,oldImage;static int imageW,imageH;static DWORD capturedAt;
static void hide(){if(window)ShowWindow(window,SW_HIDE);}
static void clear(){hide();if(captureDC){SelectObject(captureDC,oldImage);DeleteObject(image);DeleteDC(captureDC);}captureDC=nullptr;image=oldImage=nullptr;imageW=imageH=0;capturedAt=0;}
static void destroy(){clear();if(window)DestroyWindow(window);window=nullptr;}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCHITTEST)return HTTRANSPARENT;
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
  if(captureDC&&imageW>0){SetStretchBltMode(dc,COLORONCOLOR);StretchBlt(dc,1,1,r.right-2,r.bottom-2,captureDC,0,0,imageW,imageH,SRCCOPY);}
  HBRUSH b=CreateSolidBrush(RGB(157,161,115));FrameRect(dc,&r,b);DeleteObject(b);EndPaint(w,&ps);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
static void update(POINT origin,const RECT&client){
 if(!minimapEnabled||!enabled||!foregroundGame()||snap.renderStatus!=1||snap.chat||snap.width<320||snap.height<200||client.right<320||client.bottom<200){hide();return;}
 // Native YR sidebar/radar is 160 game pixels wide. Use actual game and client
 // sizes, never desktop/INI resolution. Entire radar frame is mirrored.
 const int sw=std::max(1,int(int64_t(160)*client.right/snap.width));
 const int sh=std::max(1,int(int64_t(140)*client.bottom/snap.height));
 const int sx=origin.x+client.right-sw,sy=origin.y;
 int dw=int(int64_t(sw)*minimapScale/100),dh=int(int64_t(sh)*minimapScale/100);
 const int gap=std::max(6,int(int64_t(10)*client.right/snap.width));
 // Never overlap the source (avoids self-capture feedback). Fit small clients.
 const double fit=std::min(1.0,std::min(double(client.right-sw-gap-4)/std::max(1,dw),double(client.bottom-44)/std::max(1,dh)));
 if(fit<=0){hide();return;}dw=std::max(1,int(dw*fit));dh=std::max(1,int(dh*fit));
 if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2RadarMirror116";RegisterClassW(&cls);
  window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED,cls.lpszClassName,L"小地图放大",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);}
 if(!window)return;SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
 DWORD now=GetTickCount();if(!captureDC||imageW!=sw||imageH!=sh){clear();HDC screen=GetDC(nullptr);captureDC=CreateCompatibleDC(screen);image=CreateCompatibleBitmap(screen,sw,sh);ReleaseDC(nullptr,screen);
  if(!captureDC||!image){if(image)DeleteObject(image);if(captureDC)DeleteDC(captureDC);captureDC=nullptr;image=nullptr;return;}oldImage=(HBITMAP)SelectObject(captureDC,image);imageW=sw;imageH=sh;}
 if(!capturedAt||DWORD(now-capturedAt)>=uint32_t(std::max(1,1000/std::min(fps,60)))){
  HDC screen=GetDC(nullptr);BOOL ok=BitBlt(captureDC,0,0,sw,sh,screen,sx,sy,SRCCOPY|CAPTUREBLT);ReleaseDC(nullptr,screen);
  if(!ok){hide();return;}capturedAt=now;InvalidateRect(window,nullptr,FALSE);
 }
 SetWindowPos(window,HWND_TOPMOST,sx-gap-dw-2,origin.y+40,dw+2,dh+2,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
}
