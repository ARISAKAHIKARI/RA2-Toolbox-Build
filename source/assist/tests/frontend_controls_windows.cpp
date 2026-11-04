#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include "../../shared/source/modern_ui.h"
#include "../../shared/source/page_wheel.h"
#include "../source/dependency_controls.h"

// Actual Win32 controls exercise the production subclasses. This is a
// controlled window fixture, not a game, visual smoothness or FPS test.
static unsigned commands=0,wheels=0;
static int wheelSum=0,pagePixels=0,secondPixels=0;
static HWND secondPage=nullptr;
static void require(bool ok,const char*message){
 if(!ok){std::fprintf(stderr,"FAIL: %s (Win32=%lu)\n",message,(unsigned long)GetLastError());ExitProcess(1);}
}
static void pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
static LRESULT CALLBACK pageProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_MOUSEWHEEL){++wheels;wheelSum+=GET_WHEEL_DELTA_WPARAM(wp);(w==secondPage?secondPixels:pagePixels)+=pagewheel::delta(w,wp,84);return 0;}
 if(message==WM_COMMAND){++commands;return 0;}
 return DefWindowProcW(w,message,wp,lp);
}
static std::wstring text(HWND w){wchar_t value[512]{};GetWindowTextW(w,value,512);return value;}
static HWND control(HWND page,const wchar_t*kind,const wchar_t*label,DWORD style,int id,int y){
 HWND w=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,10,y,550,110,page,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
 require(w!=nullptr,"create actual native control");return w;
}
static void wheel(HWND target,int delta){
 POINT point{5,5};ClientToScreen(target,&point);
 SendMessageW(target,WM_MOUSEWHEEL,MAKEWPARAM(0,WORD(SHORT(delta))),MAKELPARAM(SHORT(point.x),SHORT(point.y)));pump();
}
static void wheelPreserves(HWND target,HWND combo,HWND edit,HWND hotkey,const char*message){
 const auto selection=SendMessageW(combo,CB_GETCURSEL,0,0),key=SendMessageW(hotkey,HKM_GETHOTKEY,0,0);
 const auto value=text(edit);const unsigned before=wheels;const int previousSum=wheelSum;
 SetFocus(target);wheel(target,120);wheel(target,-120);
 require(wheels==before+2&&wheelSum==previousSum,"wheel forwarded once per message to the settings page");
 require(SendMessageW(combo,CB_GETCURSEL,0,0)==selection&&SendMessageW(hotkey,HKM_GETHOTKEY,0,0)==key&&text(edit)==value,message);
}
static void verifyGestures(HWND page,HWND action){
 const WORD chord=WORD(VK_F6|(HOTKEYF_CONTROL<<8));
 // Legacy values retain their exact storage representation.
 for(WORD old:{chord,WORD(chord|keygesture::Double),WORD(VK_INSERT|(HOTKEYF_EXT<<8))}){
  SendMessageW(action,HKM_SETHOTKEY,old,0);
  require(SendMessageW(action,HKM_GETHOTKEY,0,0)==old,"legacy and extended-key binding round trip");
 }
 for(UINT command:{modernui::KeyPress,modernui::KeyRelease,modernui::KeyDoublePress,modernui::KeyDoubleRelease}){
  SendMessageW(action,HKM_SETHOTKEY,chord,0);modernui::hotkeyCommand(action,command);
  auto saved=SendMessageW(action,HKM_GETHOTKEY,0,0);SendMessageW(action,HKM_SETHOTKEY,saved,0);
  require(SendMessageW(action,HKM_GETHOTKEY,0,0)==saved,"explicit action gesture survives settings reload");
  require(keygesture::physical(WORD(saved))==chord,"gesture does not replace chord modifiers or primary key");
  auto description=modernui::hotkeyDescription(action);
  require(description.find(L"Ctrl")!=std::wstring::npos&&description.find(L"F6")!=std::wstring::npos,"chord description names modifier and main key");
  require(description.find(L"按住")!=std::wstring::npos,"chord description explicitly requires holding its modifier");
  if(command==modernui::KeyRelease||command==modernui::KeyDoubleRelease)
   require(description.find(L"松开")!=std::wstring::npos,"release gesture description is explicit");
  if(command==modernui::KeyDoublePress||command==modernui::KeyDoubleRelease)
   require(description.find(L"双击")!=std::wstring::npos,"double gesture description is explicit");
 }
 HWND held=control(page,HOTKEY_CLASSW,L"",0,110,180);modernui::style(held);modernui::holdKey(held);
 SendMessageW(held,HKM_SETHOTKEY,chord,0);modernui::hotkeyCommand(held,modernui::KeyDoubleHold);
 auto stored=SendMessageW(held,HKM_GETHOTKEY,0,0);
 require((stored&keygesture::Double)!=0,"continuous authorization supports second-press hold");
 modernui::hotkeyCommand(held,modernui::KeyRelease);
 require(SendMessageW(held,HKM_GETHOTKEY,0,0)==stored,"continuous authorization rejects unsupported release-only mode");
 require(modernui::hotkeyDescription(held).find(L"按住")!=std::wstring::npos,"continuous authorization is described as held");
 HWND pressed=control(page,HOTKEY_CLASSW,L"",0,111,240);modernui::style(pressed);modernui::pressKey(pressed);
 SendMessageW(pressed,HKM_SETHOTKEY,chord,0);modernui::hotkeyCommand(pressed,modernui::KeyDoublePress);
 stored=SendMessageW(pressed,HKM_GETHOTKEY,0,0);modernui::hotkeyCommand(pressed,modernui::KeyDoubleRelease);
 require(SendMessageW(pressed,HKM_GETHOTKEY,0,0)==stored,"press-only feature rejects unsupported double release");
 EnableWindow(action,FALSE);stored=SendMessageW(action,HKM_GETHOTKEY,0,0);unsigned before=commands;
 modernui::hotkeyCommand(action,modernui::KeyClear);
 require(SendMessageW(action,HKM_GETHOTKEY,0,0)==stored&&commands==before,"disabled hotkey refuses menu mutation and notification");
 EnableWindow(action,TRUE);
 struct MouseChoice{UINT command;unsigned vk;};
 for(auto choice:{MouseChoice{modernui::KeyMouseLeft,VK_LBUTTON},MouseChoice{modernui::KeyMouseRight,VK_RBUTTON},MouseChoice{modernui::KeyMouseMiddle,VK_MBUTTON},MouseChoice{modernui::KeyMouseX1,VK_XBUTTON1},MouseChoice{modernui::KeyMouseX2,VK_XBUTTON2}}){
  SendMessageW(action,HKM_SETHOTKEY,chord,0);modernui::hotkeyCommand(action,modernui::KeyDoubleRelease);modernui::hotkeyCommand(action,choice.command);
  auto value=SendMessageW(action,HKM_GETHOTKEY,0,0);
  require((value&255)==choice.vk&&(value&keygesture::Double)!=0,"mouse primary selection preserves explicit double gesture");
 }
 HWND keyboard=control(page,HOTKEY_CLASSW,L"",0,112,380);modernui::style(keyboard);modernui::pressKey(keyboard);modernui::keyboardOnly(keyboard);
 SendMessageW(keyboard,HKM_SETHOTKEY,chord,0);stored=SendMessageW(keyboard,HKM_GETHOTKEY,0,0);before=commands;
 for(UINT command:{modernui::KeyMouseLeft,modernui::KeyMouseRight,modernui::KeyMouseMiddle,modernui::KeyMouseX1,modernui::KeyMouseX2})
  require(!modernui::hotkeyCommand(keyboard,command),"keyboard-only fields reject mouse selection commands");
 SendMessageW(keyboard,WM_MBUTTONDOWN,MK_MBUTTON,MAKELPARAM(10,10));SendMessageW(keyboard,WM_XBUTTONDOWN,MAKEWPARAM(0,XBUTTON1),MAKELPARAM(10,10));
 require(SendMessageW(keyboard,HKM_GETHOTKEY,0,0)==stored&&commands==before,"keyboard-only rejection preserves value and emits no setting notification");
 require(modernui::hotkeyCommand(keyboard,modernui::KeyDoublePress),"keyboard-only fields still accept a supported keyboard gesture");
 SendMessageW(keyboard,HKM_SETHOTKEY,VK_LBUTTON,0);
 require((SendMessageW(keyboard,HKM_GETHOTKEY,0,0)&255)==VK_LBUTTON&&modernui::hotkeyDescription(keyboard).find(L"仅支持键盘")!=std::wstring::npos,"old unsupported mouse value remains visible with an explicit keyboard-only reason");
 SetFocus(action);SendMessageW(action,HKM_SETHOTKEY,0,0);
 SendMessageW(action,WM_KEYDOWN,VK_CONTROL,MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16);
 SendMessageW(action,WM_KEYUP,VK_CONTROL,(MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16)|(1L<<30)|(1UL<<31));
 require(keygesture::physical(WORD(SendMessageW(action,HKM_GETHOTKEY,0,0)))==VK_CONTROL,"bare modifier can be captured without an imaginary main key");
 BYTE originalState[256]{},captureState[256]{};require(GetKeyboardState(originalState)!=FALSE,"capture fixture keyboard state");
 SendMessageW(action,HKM_SETHOTKEY,0,0);captureState[VK_CONTROL]=captureState[VK_LCONTROL]=0x80;
 require(SetKeyboardState(captureState)!=FALSE,"set controlled modifier state");
 SendMessageW(action,WM_KEYDOWN,VK_CONTROL,MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16);
 captureState['K']=0x80;require(SetKeyboardState(captureState)!=FALSE,"set controlled main-key state");SendMessageW(action,WM_KEYDOWN,'K',MapVirtualKeyW('K',MAPVK_VK_TO_VSC)<<16);
 captureState['K']=0;require(SetKeyboardState(captureState)!=FALSE,"set controlled main-key release");SendMessageW(action,WM_KEYUP,'K',(MapVirtualKeyW('K',MAPVK_VK_TO_VSC)<<16)|(1L<<30)|(1UL<<31));
 captureState[VK_CONTROL]=captureState[VK_LCONTROL]=0;require(SetKeyboardState(captureState)!=FALSE,"set controlled modifier release");SendMessageW(action,WM_KEYUP,VK_CONTROL,(MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16)|(1L<<30)|(1UL<<31));
 require(SetKeyboardState(originalState)!=FALSE,"restore fixture keyboard state");
 require(keygesture::physical(WORD(SendMessageW(action,HKM_GETHOTKEY,0,0)))==WORD('K'|(HOTKEYF_CONTROL<<8)),"modifier then main-key capture survives releasing both keys");
 for(WORD replacement:{WORD(chord|unsigned(keygesture::Mode::Release)),WORD(0)}){
  SendMessageW(action,HKM_SETHOTKEY,0,0);SendMessageW(action,WM_KEYDOWN,VK_CONTROL,MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16);
  SendMessageW(action,HKM_SETHOTKEY,replacement,0);SendMessageW(action,WM_KEYUP,VK_CONTROL,(MapVirtualKeyW(VK_CONTROL,MAPVK_VK_TO_VSC)<<16)|(1L<<30)|(1UL<<31));
  require(SendMessageW(action,HKM_GETHOTKEY,0,0)==replacement,"programmatic reload or clear cancels pending modifier capture before release");
 }
 std::puts("PASS: actual Win32 hotkey legacy/gesture storage, clear modifier/action descriptions, action-kind restrictions and disabled command rejection");
}
static void verifyDependencies(HWND check,HWND combo,HWND edit){
 namespace ds=assist::settingsdeps;
 ds::State state;state.set(ds::Need::Render,true);
 for(HWND w:{check,combo,edit})EnableWindow(w,TRUE);
 SendMessageW(check,BM_SETCHECK,BST_CHECKED,0);SendMessageW(combo,CB_SETCURSEL,2,0);SetWindowTextW(edit,L"61");
 ds::ControlBinding checkBinding(check),comboBinding(combo),editBinding(edit);
 auto verifyTip=[&](bool enabled){
  HWND tooltip=(HWND)GetPropW(check,L"RA2.Tip.Window");require(tooltip!=nullptr,"dependent control has a native tooltip");
  wchar_t content[512]{};TOOLINFOW info{sizeof info};info.hwnd=GetParent(check);info.uId=(UINT_PTR)check;info.lpszText=content;
  require(SendMessageW(tooltip,TTM_GETTOOLINFOW,0,(LPARAM)&info)!=FALSE,"native tooltip tool is registered");
  require(bool(info.uFlags&TTF_IDISHWND)==enabled&&(info.uFlags&TTF_SUBCLASS),"disabled reason tooltip follows the parent rectangle instead of disabled mouse input");
  return info.rect;
 };
 modernui::tip(check,ds::reason(ds::Need::Render));verifyTip(true);
 auto allowed=ds::decision(9304,state);require(allowed.enabled(),"render-dependent control starts enabled when parent is enabled");
 checkBinding.refresh(allowed);comboBinding.refresh(allowed);editBinding.refresh(allowed);
 state.set(ds::Need::Render,false);auto blocked=ds::decision(9304,state);
 require(!blocked.enabled()&&blocked.missing==ds::Need::Render,"render dependency returns a specific missing parent");
 checkBinding.refresh(blocked);comboBinding.refresh(blocked);editBinding.refresh(blocked);
 require(!IsWindowEnabled(check)&&!IsWindowEnabled(combo)&&!IsWindowEnabled(edit),"dependency disables actual checkbox, combo and edit HWNDs");
 RECT beforeTip=verifyTip(false);require(beforeTip.right>beforeTip.left&&beforeTip.bottom>beforeTip.top,"disabled dependency tooltip has a nonempty target rectangle");
 RECT moved{};GetWindowRect(check,&moved);MapWindowPoints(nullptr,GetParent(check),(POINT*)&moved,2);SetWindowPos(check,nullptr,moved.left+13,moved.top+7,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
 RECT afterTip=verifyTip(false);require(afterTip.left==beforeTip.left+13&&afterTip.top==beforeTip.top+7,"disabled dependency reason target follows page layout");
 ShowWindow(check,SW_HIDE);RECT hiddenTip=verifyTip(false);require(hiddenTip.right<=hiddenTip.left||hiddenTip.bottom<=hiddenTip.top,"hidden dependency control has no active reason target");
 SetWindowPos(check,nullptr,afterTip.left+17,afterTip.top+11,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(check,SW_SHOWNOACTIVATE);
 RECT shownTip=verifyTip(false);require(shownTip.left==afterTip.left+17&&shownTip.top==afterTip.top+11&&shownTip.right>shownTip.left,"reason target is refreshed after a hidden page control is laid out and shown");
 require(SendMessageW(check,BM_GETCHECK,0,0)==BST_CHECKED&&SendMessageW(combo,CB_GETCURSEL,0,0)==2&&text(edit)==L"61","parent disable preserves existing child preferences");
 // Synthetic WM_COMMAND is not a permission grant: the same production
 // adapter used by the real console restores the last accepted value.
 SendMessageW(check,BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(combo,CB_SETCURSEL,0,0);SetWindowTextW(edit,L"999");
 require(checkBinding.rejectChange(blocked)&&comboBinding.rejectChange(blocked)&&editBinding.rejectChange(blocked),"blocked dependency rejects synthetic setting notifications");
 require(SendMessageW(check,BM_GETCHECK,0,0)==BST_CHECKED&&SendMessageW(combo,CB_GETCURSEL,0,0)==2&&text(edit)==L"61","rejected setting restores accepted values for all native kinds");
 state.set(ds::Need::Render,true);allowed=ds::decision(9304,state);
 checkBinding.refresh(allowed);comboBinding.refresh(allowed);editBinding.refresh(allowed);
 require(IsWindowEnabled(check)&&IsWindowEnabled(combo)&&IsWindowEnabled(edit),"restoring parent re-enables the actual child controls");
 verifyTip(true);
 require(!checkBinding.rejectChange(allowed)&&!comboBinding.rejectChange(allowed)&&!editBinding.rejectChange(allowed),"enabled dependency allows intentional setting edits");
 std::puts("PASS: actual HWND dependencies disable controls, preserve child values, reject synthetic mutations and restore availability");
}
int main(){
 INITCOMMONCONTROLSEX common{sizeof common,ICC_WIN95_CLASSES|ICC_UPDOWN_CLASS};require(InitCommonControlsEx(&common)!=FALSE,"initialize common controls");
 WNDCLASSW cls{};cls.lpfnWndProc=pageProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2FrontendControlsFixture";require(RegisterClassW(&cls)!=0,"register fixture page");
 HWND page=CreateWindowExW(0,cls.lpszClassName,L"RA2 controlled frontend test",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_VISIBLE,40,40,650,650,nullptr,nullptr,cls.hInstance,nullptr);
 require(page!=nullptr,"create settings page");
 HWND combo=control(page,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,101,10);
 for(auto value:{L"First",L"Second",L"Third"})SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)value);
 SendMessageW(combo,CB_SETCURSEL,1,0);
 HWND edit=control(page,L"EDIT",L"42",ES_NUMBER|WS_BORDER,102,50);
 HWND updown=control(page,UPDOWN_CLASSW,L"",UDS_SETBUDDYINT|UDS_ALIGNRIGHT,103,50);SendMessageW(updown,UDM_SETBUDDY,(WPARAM)edit,0);SendMessageW(updown,UDM_SETRANGE32,0,100);SendMessageW(updown,UDM_SETPOS32,0,42);
 HWND hotkey=control(page,HOTKEY_CLASSW,L"",0,104,100);SendMessageW(hotkey,HKM_SETHOTKEY,VK_F6|(HOTKEYF_CONTROL<<8),0);
 HWND check=control(page,L"BUTTON",L"Dependent setting",BS_AUTOCHECKBOX,105,150);
 modernui::styleChildren(page);pump();
 for(HWND target:{combo,edit,updown,hotkey,check})wheelPreserves(target,combo,edit,hotkey,"wheel must not mutate selected config, numeric edit or key binding");
 COMBOBOXINFO listInfo{sizeof listInfo};require(GetComboBoxInfo(combo,&listInfo)!=FALSE,"dropdown-list popup handle");
 SendMessageW(combo,CB_SHOWDROPDOWN,TRUE,0);pump();wheelPreserves(listInfo.hwndList,combo,edit,hotkey,"opened dropdown-list wheel preserves selected config");
 SendMessageW(combo,CB_SHOWDROPDOWN,FALSE,0);
 // The editable combo's inner EDIT and the opened popup list are separate
 // HWNDs, so both need the same production route.
 HWND editable=control(page,L"COMBOBOX",L"",CBS_DROPDOWN|WS_VSCROLL,106,300);
 for(auto value:{L"Alpha",L"Beta",L"Gamma"})SendMessageW(editable,CB_ADDSTRING,0,(LPARAM)value);
 SendMessageW(editable,CB_SETCURSEL,1,0);modernui::style(editable);modernui::styleChildren(editable);
 COMBOBOXINFO info{sizeof info};require(GetComboBoxInfo(editable,&info)!=FALSE,"editable combo children");
 const auto editableSelection=SendMessageW(editable,CB_GETCURSEL,0,0);const auto editableText=text(editable);
 wheelPreserves(info.hwndItem,combo,edit,hotkey,"editable combo wheel preserves settings");
 require(SendMessageW(editable,CB_GETCURSEL,0,0)==editableSelection&&text(editable)==editableText,"editable combo's inner edit wheel preserves its own config");
 SendMessageW(editable,CB_SHOWDROPDOWN,TRUE,0);pump();wheelPreserves(info.hwndList,combo,edit,hotkey,"opened combo wheel preserves other settings");
 require(SendMessageW(editable,CB_GETCURSEL,0,0)==editableSelection&&text(editable)==editableText,"opened combo wheel must not choose another value");
 SendMessageW(editable,CB_SHOWDROPDOWN,FALSE,0);
 const int originalPixels=pagePixels;
 secondPage=CreateWindowExW(0,cls.lpszClassName,L"Second controlled settings page",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_VISIBLE,700,40,650,400,nullptr,nullptr,cls.hInstance,nullptr);
 require(secondPage!=nullptr,"create independent settings page");
 HWND secondEdit=control(secondPage,L"EDIT",L"88",ES_NUMBER,120,10);modernui::style(secondEdit);
 wheel(edit,60);require(pagePixels==originalPixels,"first partial notch is retained");
 wheel(secondEdit,60);require(secondPixels==0,"partial wheel remainder is per page");
 wheel(edit,60);require(pagePixels==originalPixels+84&&secondPixels==0,"same-page partial notches combine once");
 wheel(secondEdit,60);require(secondPixels==84,"second page retains its own partial notch");
 wheel(edit,-60);wheel(edit,-60);wheel(secondEdit,-120);
 require(pagePixels==originalPixels&&secondPixels==0,"reverse wheel direction consumes accumulated notches correctly");
 DestroyWindow(secondPage);secondPage=nullptr;
 SendMessageW(combo,CB_SETCURSEL,2,0);SetWindowTextW(edit,L"61");
 require(SendMessageW(combo,CB_GETCURSEL,0,0)==2&&text(edit)==L"61","intentional configuration edits remain possible");
 auto checkValue=SendMessageW(check,BM_GETCHECK,0,0);unsigned before=commands;EnableWindow(check,FALSE);
 SendMessageW(check,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(10,10));SendMessageW(check,WM_LBUTTONUP,0,MAKELPARAM(10,10));pump();
 require(SendMessageW(check,BM_GETCHECK,0,0)==checkValue&&commands==before,"disabled modern checkbox ignores user clicks");
 std::puts("PASS: actual Win32 combo/edit/spin/hotkey/checkbox wheel forwarding, combo child/popup protection, intentional editing and disabled click behavior");
 verifyDependencies(check,combo,edit);verifyGestures(page,hotkey);DestroyWindow(page);pump();
 std::puts("PASS: frontend controlled-window fixture; no live-game, visual smoothness or FPS certification");return 0;
}
