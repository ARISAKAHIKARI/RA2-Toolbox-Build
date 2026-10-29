#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <cwchar>
#include <string>
#include "key_gesture.h"
#include "page_wheel.h"
namespace modernui {
constexpr int BodyPixels=14,HeadingPixels=20;
// Every console uses 96-DPI design coordinates, including embedded panes.
constexpr unsigned panelDpi(unsigned dpi,bool){return dpi>=48&&dpi<=768?dpi:96;}
inline unsigned windowDpi(HWND w){using Fn=UINT(WINAPI*)(HWND);static auto f=(Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");return panelDpi(f?f(w):96,false);}
inline int scale(int value,unsigned dpi){return MulDiv(value,int(panelDpi(dpi,false)),96);}
constexpr UINT DpiMessage=WM_APP+95;
inline HFONT makeFont(int pixels=BodyPixels,unsigned dpi=96,int weight=FW_NORMAL){
 return CreateFontW(-int((pixels*panelDpi(dpi,false)+48)/96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
}
inline LRESULT CALLBACK sharedTipProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY){
  auto tooltip=(HWND)RemovePropW(w,L"RA2.Tip.Shared");
  if(tooltip&&IsWindow(tooltip))DestroyWindow(tooltip);
  RemoveWindowSubclass(w,sharedTipProc,id);
 }
 return DefSubclassProc(w,m,a,b);
}
inline HWND sharedTooltip(HWND parent){
 if(!parent)return nullptr;
 auto tooltip=(HWND)GetPropW(parent,L"RA2.Tip.Shared");if(tooltip&&IsWindow(tooltip))return tooltip;
 tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(tooltip){SetPropW(parent,L"RA2.Tip.Shared",tooltip);SetWindowSubclass(parent,sharedTipProc,0x6222,0);}
 return tooltip;
}
inline void updateTip(HWND control,bool recreate=false){
 auto text=(std::wstring*)GetPropW(control,L"RA2.Tip.Text");auto tooltip=(HWND)GetPropW(control,L"RA2.Tip.Window");if(!text||!tooltip)return;
 bool disabled=!IsWindowEnabled(control),registered=GetPropW(control,L"RA2.Tip.Registered")!=nullptr;
 bool wasDisabled=GetPropW(control,L"RA2.Tip.ParentRect")!=nullptr;
 TOOLINFOW info{sizeof info};info.hwnd=GetParent(control);info.uId=(UINT_PTR)control;
 if(registered&&(recreate||disabled!=wasDisabled)){SendMessageW(tooltip,TTM_DELTOOLW,0,(LPARAM)&info);registered=false;}
 info.uFlags=TTF_SUBCLASS|(disabled?0:TTF_IDISHWND);info.lpszText=const_cast<wchar_t*>(text->c_str());
 if(disabled&&IsWindowVisible(control)){GetWindowRect(control,&info.rect);MapWindowPoints(nullptr,info.hwnd,(POINT*)&info.rect,2);}
 SendMessageW(tooltip,registered?TTM_UPDATETIPTEXTW:TTM_ADDTOOLW,0,(LPARAM)&info);
 if(registered&&disabled)SendMessageW(tooltip,TTM_NEWTOOLRECTW,0,(LPARAM)&info);
 SetPropW(control,L"RA2.Tip.Registered",(HANDLE)1);if(disabled)SetPropW(control,L"RA2.Tip.ParentRect",(HANDLE)1);else RemovePropW(control,L"RA2.Tip.ParentRect");
}
inline LRESULT CALLBACK tipProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY){auto tooltip=(HWND)RemovePropW(w,L"RA2.Tip.Window");
  if(tooltip&&IsWindow(tooltip)){TOOLINFOW info{sizeof info};info.hwnd=GetParent(w);info.uId=(UINT_PTR)w;SendMessageW(tooltip,TTM_DELTOOLW,0,(LPARAM)&info);}
  delete (std::wstring*)RemovePropW(w,L"RA2.Tip.Text");RemovePropW(w,L"RA2.Tip.Registered");RemovePropW(w,L"RA2.Tip.ParentRect");RemoveWindowSubclass(w,tipProc,id);return DefSubclassProc(w,m,a,b);}
 auto result=DefSubclassProc(w,m,a,b);
 if(m==WM_ENABLE||m==WM_SHOWWINDOW||(m==WM_WINDOWPOSCHANGED&&GetPropW(w,L"RA2.Tip.ParentRect")))updateTip(w);
 return result;
}
inline void tip(HWND control,const std::wstring& text){
 if(!control||text.empty())return;auto storage=(std::wstring*)GetPropW(control,L"RA2.Tip.Text");
 HWND tooltip=(HWND)GetPropW(control,L"RA2.Tip.Window");
 if(!storage){storage=new std::wstring;SetPropW(control,L"RA2.Tip.Text",storage);SetWindowSubclass(control,tipProc,0x6220,0);}
 *storage=text;
 if(!tooltip){tooltip=sharedTooltip(GetParent(control));SetPropW(control,L"RA2.Tip.Window",tooltip);}
 if(!tooltip)return;updateTip(control);
 SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,scale(440,windowDpi(control)));SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,450);SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_AUTOPOP,15000);
}
constexpr UINT FocusMessage=WM_APP+94;
constexpr COLORREF Background=RGB(18,24,34),Surface=RGB(26,35,49),Field=RGB(32,43,59),Text=RGB(231,238,247),Muted=RGB(153,170,193),Accent=RGB(104,198,255);
inline void rounded(HDC dc,RECT r,COLORREF color,int radius=10){HBRUSH b=CreateSolidBrush(color);auto old=SelectObject(dc,b);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(b);}
constexpr UINT_PTR ButtonSubclass=0x6215,ParentSubclass=0x6217,StaticSubclass=0x6218;
inline bool toggleType(DWORD_PTR type){return type==BS_AUTOCHECKBOX||type==BS_CHECKBOX;}
inline void paintButton(HWND w,HDC dc,RECT r,DWORD_PTR type,UINT state){
 auto px=[&](int n){return scale(n,windowDpi(w));};int saved=SaveDC(dc);bool enabled=!(state&ODS_DISABLED),focus=(state&ODS_FOCUS)!=0;
 bool toggle=toggleType(type),checked=GetPropW(w,L"RA2.Modern.Checked")!=nullptr;
 bool pressed=(state&ODS_SELECTED)!=0,selected=GetPropW(w,L"RA2.Modern.Selected")!=nullptr;
 COLORREF fill=toggle?Background:selected?RGB(36,67,91):pressed?RGB(46,66,87):Surface;
 HBRUSH base=CreateSolidBrush(Background);FillRect(dc,&r,base);DeleteObject(base);
 if(!toggle)rounded(dc,r,fill);
 RECT text=r;text.left+=px(toggle?44:12);text.right-=px(8);
 if(toggle){int y=r.top+(r.bottom-r.top-px(18))/2;RECT rail{r.left+px(2),y,r.left+px(34),y+px(18)};rounded(dc,rail,enabled&&checked?Accent:RGB(67,80,98),18);RECT dot{r.left+px(checked?18:4),y+px(2),r.left+px(checked?32:18),y+px(16)};rounded(dc,dot,checked?Background:Text,14);}
 wchar_t label[512]{};GetWindowTextW(w,label,512);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,!enabled?RGB(106,121,141):selected?Accent:Text);
 auto font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);if(font)SelectObject(dc,font);
 DrawTextW(dc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|(toggle||selected?DT_LEFT:DT_CENTER));
 if(focus&&!(state&ODS_NOFOCUSRECT)){RECT f=r;InflateRect(&f,-3,-3);DrawFocusRect(dc,&f);}
 if(saved)RestoreDC(dc,saved);
}
inline LRESULT CALLBACK buttonProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR type){
 if(m==WM_NCDESTROY){RemovePropW(w,L"RA2.Modern.Checked");RemovePropW(w,L"RA2.Modern.Selected");RemoveWindowSubclass(w,buttonProc,id);return DefSubclassProc(w,m,wp,lp);}
 if(m==BM_GETCHECK&&toggleType(type))return GetPropW(w,L"RA2.Modern.Checked")?BST_CHECKED:BST_UNCHECKED;
 if(m==BM_SETCHECK&&toggleType(type)){if(wp==BST_CHECKED)SetPropW(w,L"RA2.Modern.Checked",(HANDLE)1);else RemovePropW(w,L"RA2.Modern.Checked");InvalidateRect(w,nullptr,FALSE);return 0;}
 if(!IsWindowEnabled(w)&&(m==BM_CLICK||m==WM_LBUTTONDOWN||m==WM_LBUTTONUP||m==WM_LBUTTONDBLCLK||((m==WM_KEYDOWN||m==WM_KEYUP)&&wp==VK_SPACE)))return 0;
 // Keep dialog-manager default-button updates from restoring native painting.
 if(m==BM_SETSTYLE){type=wp&BS_TYPEMASK;if(type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON)SetWindowSubclass(w,buttonProc,id,type);wp=(wp&~WPARAM(BS_TYPEMASK))|BS_OWNERDRAW;}
 if(m==WM_GETDLGCODE){auto result=DefSubclassProc(w,m,wp,lp);if(!toggleType(type))return (result&~(DLGC_DEFPUSHBUTTON|DLGC_UNDEFPUSHBUTTON))|(type==BS_DEFPUSHBUTTON?DLGC_DEFPUSHBUTTON:DLGC_UNDEFPUSHBUTTON);return result;}
 if(m==WM_PRINTCLIENT){RECT r{};GetClientRect(w,&r);UINT state=IsWindowEnabled(w)?0:ODS_DISABLED;if(GetFocus()==w)state|=ODS_FOCUS;if(SendMessageW(w,BM_GETSTATE,0,0)&BST_PUSHED)state|=ODS_SELECTED;paintButton(w,(HDC)wp,r,type,state);return 0;}
 // BS_OWNERDRAW makes ALL native state changes use the same WM_DRAWITEM renderer.
 // Merely replacing WM_PAINT leaves native WM_SETTEXT/focus/BM_SETSTATE drawing.
 return DefSubclassProc(w,m,wp,lp);
}
// Keep the native hotkey state machine and notifications; own the entire
// painted field so unfocused values never depend on native erase/focus order.
constexpr UINT_PTR HotkeySubclass=0x6219;
enum HotkeyMenu :UINT {KeyPress=0x7101,KeyRelease=0x7102,KeyDoublePress=0x7103,KeyDoubleRelease=0x7104,KeyHold=0x7105,KeyDoubleHold=0x7106,KeyClear=0x7110,KeyMouseLeft=0x7121,KeyMouseRight=0x7122,KeyMouseMiddle=0x7124,KeyMouseX1=0x7125,KeyMouseX2=0x7126};
inline bool hotkeyGestureAllowed(HWND w,keygesture::Mode action){
 if(action==keygesture::Mode::Legacy)action=GetPropW(w,L"RA2.Modern.Hold")?keygesture::Mode::Hold:keygesture::Mode::Press;
 if(GetPropW(w,L"RA2.Modern.Hold"))return action==keygesture::Mode::Hold;
 if(GetPropW(w,L"RA2.Modern.PressOnly"))return action==keygesture::Mode::Press;
 return action==keygesture::Mode::Press||action==keygesture::Mode::Release;
}
inline std::wstring hotkeyName(unsigned key){
 unsigned vk=key&255;switch(vk){case 1:return L"鼠标左键";case 2:return L"鼠标右键";case 4:return L"鼠标中键";case 5:return L"鼠标侧键1";case 6:return L"鼠标侧键2";case 16:return L"Shift";case 17:return L"Ctrl";case 18:return L"Alt";}
 wchar_t name[96]{};unsigned scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC)<<16;if(key&0x0800)scan|=1u<<24;
 return GetKeyNameTextW(static_cast<LONG>(scan),name,96)>0?name:L"VK "+std::to_wstring(vk);
}
inline std::wstring hotkeyDescription(HWND w){
 auto key=unsigned(SendMessageW(w,HKM_GETHOTKEY,0,0));if(!(key&255))return GetPropW(w,L"RA2.Modern.MouseOnly")?L"仅鼠标，无需额外按键":L"未绑定";
 if(GetPropW(w,L"RA2.Modern.KeyboardOnly")&&(key&255)<7)return L"此字段仅支持键盘，请重新选择键盘按键。";
 if(!hotkeyGestureAllowed(w,keygesture::mode(key)))return L"此字段不支持当前手势，请点击右侧模式重新选择。";
 return keygesture::describe(key,hotkeyName(key),GetPropW(w,L"RA2.Modern.Hold")!=nullptr);
}
inline void refreshHotkey(HWND w){
 InvalidateRect(w,nullptr,FALSE);if(IsWindowEnabled(w))tip(w,hotkeyDescription(w)+L"\n点击右侧模式或按 Shift+F10 选择手势、鼠标按键；滚轮只滚动页面。");
}
inline bool hotkeyCommand(HWND w,UINT command){
 if(!w||!IsWindowEnabled(w))return false;
 unsigned key=unsigned(SendMessageW(w,HKM_GETHOTKEY,0,0));keygesture::Mode action=keygesture::Mode::Legacy;bool twice=false,gesture=true;
 switch(command){case KeyPress:action=keygesture::Mode::Press;break;case KeyRelease:action=keygesture::Mode::Release;break;case KeyDoublePress:action=keygesture::Mode::Press;twice=true;break;case KeyDoubleRelease:action=keygesture::Mode::Release;twice=true;break;case KeyHold:action=keygesture::Mode::Hold;break;case KeyDoubleHold:action=keygesture::Mode::Hold;twice=true;break;default:gesture=false;break;}
 if(gesture){if(!hotkeyGestureAllowed(w,action))return false;key=(key&0x0fff)|unsigned(action)|(twice?keygesture::Double:0);}
 else if(command==KeyClear)key=0;
 else if(command==KeyMouseLeft||command==KeyMouseRight||command==KeyMouseMiddle||command==KeyMouseX1||command==KeyMouseX2){
  if(GetPropW(w,L"RA2.Modern.KeyboardOnly"))return false;
  unsigned mods=(GetKeyState(VK_SHIFT)<0?1:0)|(GetKeyState(VK_CONTROL)<0?2:0)|(GetKeyState(VK_MENU)<0?4:0);
  key=(key&0xb000)|((mods&7)<<8)|(command-0x7120);
 }else return false;
 SendMessageW(w,HKM_SETHOTKEY,key,0);refreshHotkey(w);
 SendMessageW(GetParent(w),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(w),EN_CHANGE),(LPARAM)w);return true;
}
inline void showHotkeyMenu(HWND w,LPARAM position=-1){
 if(!IsWindowEnabled(w))return;HMENU menu=CreatePopupMenu();if(!menu)return;
 const auto key=unsigned(SendMessageW(w,HKM_GETHOTKEY,0,0));bool hold=GetPropW(w,L"RA2.Modern.Hold")!=nullptr;
 auto item=[&](UINT command,keygesture::Mode action,bool twice,const wchar_t*text){if(hotkeyGestureAllowed(w,action))AppendMenuW(menu,MF_STRING|((keygesture::effective(key,hold?keygesture::Mode::Hold:keygesture::Mode::Press)==action&&bool(key&keygesture::Double)==twice)?MF_CHECKED:0),command,text);};
 item(KeyPress,keygesture::Mode::Press,false,L"按下时触发一次");item(KeyRelease,keygesture::Mode::Release,false,L"松开时触发一次");
 item(KeyDoublePress,keygesture::Mode::Press,true,L"双击：第二次按下时触发");item(KeyDoubleRelease,keygesture::Mode::Release,true,L"双击：第二次松开时触发");
 item(KeyHold,keygesture::Mode::Hold,false,L"持续按住，松开即结束");item(KeyDoubleHold,keygesture::Mode::Hold,true,L"双击：第二次保持按住");
 if(!GetPropW(w,L"RA2.Modern.KeyboardOnly")){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
 for(const auto value:{KeyMouseLeft,KeyMouseRight,KeyMouseMiddle,KeyMouseX1,KeyMouseX2}){auto name=L"绑定"+hotkeyName(value-0x7120);AppendMenuW(menu,MF_STRING,value,name.c_str());}}
 AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,KeyClear,L"清除此绑定");
 POINT point{};if(position==-1){RECT rect{};GetWindowRect(w,&rect);point={rect.left,rect.bottom};}else point={short(LOWORD(position)),short(HIWORD(position))};
 SetFocus(w);UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,0,w,nullptr);DestroyMenu(menu);if(command)hotkeyCommand(w,command);
}
inline void paintHotkey(HWND w,HDC dc){
 int saved=SaveDC(dc);RECT r{};GetClientRect(w,&r);
 auto brush=CreateSolidBrush(Field);FillRect(dc,&r,brush);DeleteObject(brush);
 const auto key=static_cast<unsigned>(SendMessageW(w,HKM_GETHOTKEY,0,0));
 std::wstring label;const unsigned vk=key&255,mods=(key>>8)&7;
 if(vk){
  if(mods)label+=L"按住 ";
  if(mods&HOTKEYF_CONTROL)label+=L"Ctrl+";
  if(mods&HOTKEYF_ALT)label+=L"Alt+";
  if(mods&HOTKEYF_SHIFT)label+=L"Shift+";
  if(mods){label.pop_back();label+=L" → ";}label+=hotkeyName(key);
 }else label=GetPropW(w,L"RA2.Modern.MouseOnly")?L"仅鼠标":L"未设置";
 auto font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);if(font)SelectObject(dc,font);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,IsWindowEnabled(w)?Text:Muted);
 int modeWidth=scale(72,windowDpi(w));if(modeWidth>(r.right-r.left)/2)modeWidth=(r.right-r.left)/2;
 RECT text=r;text.left+=scale(6,windowDpi(w));text.right-=modeWidth+scale(3,windowDpi(w));
 RECT mode=r;mode.left=mode.right-modeWidth;bool supported=hotkeyGestureAllowed(w,keygesture::mode(key))&&!(vk&&vk<7&&GetPropW(w,L"RA2.Modern.KeyboardOnly"));DrawTextW(dc,supported?keygesture::modeText(key,GetPropW(w,L"RA2.Modern.Hold")!=nullptr):L"不支持",-1,&mode,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
 DrawTextW(dc,label.c_str(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
 if(GetFocus()==w){InflateRect(&r,-2,-2);DrawFocusRect(dc,&r);}
 if(saved)RestoreDC(dc,saved);
}
inline LRESULT CALLBACK hotkeyProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY){for(auto key:{L"RA2.Modern.MouseOnly",L"RA2.Modern.Hold",L"RA2.Modern.PressOnly",L"RA2.Modern.KeyboardOnly",L"RA2.Modern.Modifier",L"RA2.Modern.PendingModifier",L"RA2.Modern.Double",L"RA2.Modern.Gesture"})RemovePropW(w,key);RemoveWindowSubclass(w,hotkeyProc,id);return DefSubclassProc(w,m,wp,lp);}
 if(m==HKM_SETHOTKEY){RemovePropW(w,L"RA2.Modern.PendingModifier");if(wp&0x8000)SetPropW(w,L"RA2.Modern.Double",(HANDLE)1);else RemovePropW(w,L"RA2.Modern.Double");SetPropW(w,L"RA2.Modern.Gesture",(HANDLE)(UINT_PTR)(wp&keygesture::ModeMask));wp&=0x0fff;}
 if(m==HKM_GETHOTKEY){auto held=GetPropW(w,L"RA2.Modern.Modifier");auto value=held?(LRESULT)(UINT_PTR)held:DefSubclassProc(w,m,wp,lp);return (value&255)?value|(GetPropW(w,L"RA2.Modern.Double")?0x8000:0)|(UINT_PTR)GetPropW(w,L"RA2.Modern.Gesture"):0;}
 if(!IsWindowEnabled(w)&&(m==WM_KEYDOWN||m==WM_KEYUP||m==WM_SYSKEYDOWN||m==WM_SYSKEYUP||m==WM_CHAR||m==WM_LBUTTONDOWN||m==WM_RBUTTONUP||m==WM_MBUTTONDOWN||m==WM_XBUTTONDOWN||m==WM_CONTEXTMENU))return 0;
 if(m==WM_KEYDOWN&&(wp==VK_APPS||(wp==VK_F10&&GetKeyState(VK_SHIFT)<0))){showHotkeyMenu(w);return 0;}
 if((m==WM_LBUTTONDOWN||m==WM_RBUTTONUP||m==WM_CONTEXTMENU)&&IsWindowEnabled(w)){RECT r{};GetClientRect(w,&r);int width=scale(72,windowDpi(w));if(width>(r.right-r.left)/2)width=(r.right-r.left)/2;
  if(m!=WM_LBUTTONDOWN||short(LOWORD(lp))>=r.right-width){showHotkeyMenu(w,m==WM_CONTEXTMENU?lp:-1);return 0;}
 }
 if((m==WM_MBUTTONDOWN||m==WM_XBUTTONDOWN)&&IsWindowEnabled(w)){
  SetFocus(w);hotkeyCommand(w,m==WM_MBUTTONDOWN?KeyMouseMiddle:HIWORD(wp)==XBUTTON1?KeyMouseX1:KeyMouseX2);return TRUE;
 }
 // HOTKEY_CLASS treats Ctrl/Shift/Alt as chord prefixes and drops them on
 // release. Hold bindings also need a bare modifier, without a global hotkey.
 {
  auto modifier=[](WPARAM key){return key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU;};
  if(m==HKM_SETHOTKEY){if(modifier(wp&255)){SetPropW(w,L"RA2.Modern.Modifier",(HANDLE)(UINT_PTR)wp);wp=0;}else RemovePropW(w,L"RA2.Modern.Modifier");}
  if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN){RemovePropW(w,L"RA2.Modern.Modifier");
   if(modifier(wp)){if(!(lp&(1L<<30)))SetPropW(w,L"RA2.Modern.PendingModifier",(HANDLE)(UINT_PTR)wp);}
   else RemovePropW(w,L"RA2.Modern.PendingModifier");
  }
  if((m==WM_KEYUP||m==WM_SYSKEYUP)&&modifier(wp)&&GetPropW(w,L"RA2.Modern.PendingModifier")== (HANDLE)(UINT_PTR)wp){
   RemovePropW(w,L"RA2.Modern.PendingModifier");DefSubclassProc(w,m,wp,lp);DefSubclassProc(w,HKM_SETHOTKEY,0,0);
   uint8_t keys[256]{};for(unsigned vk:{16u,17u,18u})keys[vk]=GetKeyState(vk)<0;SetPropW(w,L"RA2.Modern.Modifier",(HANDLE)(UINT_PTR)keygesture::chord(unsigned(wp),keys));refreshHotkey(w);
   SendMessageW(GetParent(w),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(w),EN_CHANGE),(LPARAM)w);return 0;
  }
  if(m==WM_KILLFOCUS)RemovePropW(w,L"RA2.Modern.PendingModifier");
 }
 if(m==WM_ERASEBKGND)return TRUE;
 if(m==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);paintHotkey(w,dc);EndPaint(w,&ps);return 0;}
 if(m==WM_PRINTCLIENT){paintHotkey(w,(HDC)wp);return 0;}
 auto result=DefSubclassProc(w,m,wp,lp);
 if(m==HKM_SETHOTKEY||m==WM_KEYDOWN||m==WM_KEYUP||m==WM_SYSKEYDOWN||m==WM_SYSKEYUP||m==WM_CHAR||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_ENABLE||m==WM_SETFONT||m==WM_SIZE)
  refreshHotkey(w);
 return result;
}
inline LRESULT CALLBACK parentProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,parentProc,id);
 if(m==WM_DRAWITEM&&lp){auto*d=(DRAWITEMSTRUCT*)lp;DWORD_PTR type=0;
  if(d->CtlType==ODT_BUTTON&&GetWindowSubclass(d->hwndItem,buttonProc,ButtonSubclass,&type)){paintButton(d->hwndItem,d->hDC,d->rcItem,type,d->itemState);return TRUE;}}
 if(m==WM_COMMAND&&lp&&HIWORD(wp)==BN_CLICKED){DWORD_PTR type=0;auto ctl=(HWND)lp;
  if(!IsWindowEnabled(ctl))return 0;
  if(GetWindowSubclass(ctl,buttonProc,ButtonSubclass,&type)&&type==BS_AUTOCHECKBOX){
   // Native owner-draw buttons emit BN_CLICKED for both mouse and Space/BM_CLICK.
   // Publish the new check value before the existing parent settings handler runs.
   SendMessageW(ctl,BM_SETCHECK,SendMessageW(ctl,BM_GETCHECK,0,0)==BST_CHECKED?BST_UNCHECKED:BST_CHECKED,0);
  }}
 if(m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORSTATIC){
  wchar_t cls[40]{};GetClassNameW((HWND)lp,cls,40);auto dc=(HDC)wp;
  // HOTKEY_CLASS has no documented custom-background API and may erase with
  // COLOR_WINDOW. Keep its foreground and background a matching system pair.
  if(!_wcsicmp(cls,HOTKEY_CLASSW)){SetBkMode(dc,OPAQUE);SetTextColor(dc,GetSysColor(IsWindowEnabled((HWND)lp)?COLOR_WINDOWTEXT:COLOR_GRAYTEXT));SetBkColor(dc,GetSysColor(COLOR_WINDOW));return (LRESULT)GetSysColorBrush(COLOR_WINDOW);}
  if(!_wcsicmp(cls,L"EDIT")||!_wcsicmp(cls,L"LISTBOX")||!_wcsicmp(cls,L"COMBOBOX")){
   SetBkMode(dc,OPAQUE);SetTextColor(dc,IsWindowEnabled((HWND)lp)?Text:Muted);SetBkColor(dc,Field);SetDCBrushColor(dc,Field);return (LRESULT)GetStockObject(DC_BRUSH);
  }
 }
 return DefSubclassProc(w,m,wp,lp);
}
inline LRESULT CALLBACK staticProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,staticProc,id);
 auto result=DefSubclassProc(w,m,wp,lp);
 if(m==WM_SETTEXT||m==WM_SETFONT||m==WM_ENABLE)InvalidateRect(w,nullptr,TRUE);
 return result;
}
inline void mouseOnly(HWND w){SetPropW(w,L"RA2.Modern.MouseOnly",(HANDLE)1);InvalidateRect(w,nullptr,FALSE);}
inline void holdKey(HWND w){auto key=SendMessageW(w,HKM_GETHOTKEY,0,0);RemovePropW(w,L"RA2.Modern.PressOnly");SetPropW(w,L"RA2.Modern.Hold",(HANDLE)1);SendMessageW(w,HKM_SETHOTKEY,key,0);}
inline void pressKey(HWND w){auto key=SendMessageW(w,HKM_GETHOTKEY,0,0);RemovePropW(w,L"RA2.Modern.Hold");SetPropW(w,L"RA2.Modern.PressOnly",(HANDLE)1);SendMessageW(w,HKM_SETHOTKEY,key,0);}
inline void keyboardOnly(HWND w){SetPropW(w,L"RA2.Modern.KeyboardOnly",(HANDLE)1);refreshHotkey(w);}
inline LRESULT CALLBACK headingProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR){if(m==WM_NCDESTROY){auto f=(HFONT)RemovePropW(w,L"RA2.Heading.Font");if(f)DeleteObject(f);RemoveWindowSubclass(w,headingProc,id);}return DefSubclassProc(w,m,a,b);}
inline void heading(HWND w,unsigned dpi=0){if(!w)return;HFONT next=makeFont(HeadingPixels,dpi?dpi:windowDpi(w),FW_SEMIBOLD);if(!next)return;auto old=(HFONT)GetPropW(w,L"RA2.Heading.Font");SendMessageW(w,WM_SETFONT,(WPARAM)next,FALSE);SetPropW(w,L"RA2.Heading.Font",next);SetWindowSubclass(w,headingProc,0x6221,0);if(old)DeleteObject(old);}
inline LRESULT CALLBACK focusProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,focusProc,id);
 if(m==WM_SETFOCUS)PostMessageW(GetParent(w),FocusMessage,0,(LPARAM)w);
 return DefSubclassProc(w,m,wp,lp);
}
inline void style(HWND w){
 pagewheel::style(w);
 wchar_t cls[40]{};GetClassNameW(w,cls,40);
 if(auto parent=GetParent(w))SetWindowSubclass(parent,parentProc,ParentSubclass,0);
 if(GetWindowLongPtrW(w,GWL_STYLE)&WS_TABSTOP)SetWindowSubclass(w,focusProc,0x6216,0);
 if(!_wcsicmp(cls,HOTKEY_CLASSW)){SetWindowSubclass(w,hotkeyProc,HotkeySubclass,0);InvalidateRect(w,nullptr,FALSE);}
 if(!_wcsicmp(cls,L"STATIC"))SetWindowSubclass(w,staticProc,StaticSubclass,0);
 if(!_wcsicmp(cls,L"BUTTON")){
  DWORD_PTR prior=0;if(GetWindowSubclass(w,buttonProc,ButtonSubclass,&prior))return;
  auto style=GetWindowLongPtrW(w,GWL_STYLE);auto type=style&BS_TYPEMASK;
  // Existing specialized owner-drawn previews retain their own renderer.
  if(type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON||toggleType(type)){
   bool checked=SendMessageW(w,BM_GETCHECK,0,0)==BST_CHECKED;
   if(SetWindowSubclass(w,buttonProc,ButtonSubclass,type)){
    if(checked)SetPropW(w,L"RA2.Modern.Checked",(HANDLE)1);
    SetWindowLongPtrW(w,GWL_STYLE,(style&~LONG_PTR(BS_TYPEMASK))|BS_OWNERDRAW);
    InvalidateRect(w,nullptr,TRUE);
   }
  }
 }
}
inline BOOL CALLBACK styleChild(HWND w,LPARAM){style(w);return TRUE;}
inline void styleChildren(HWND parent){EnumChildWindows(parent,styleChild,0);}
inline void select(HWND w,bool value){if(value)SetPropW(w,L"RA2.Modern.Selected",(HANDLE)1);else RemovePropW(w,L"RA2.Modern.Selected");InvalidateRect(w,nullptr,FALSE);}
}
