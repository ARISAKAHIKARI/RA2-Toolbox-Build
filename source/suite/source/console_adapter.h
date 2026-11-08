#pragma once
// Compatibility ports own all legacy HWND access. The visible console sees
// immutable values and opaque lifetime tokens, never windows or native messages.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include "console_model.h"
#include "../../shared/source/modern_ui.h"
#include "../../shared/source/disabled_choices.h"

namespace consoleadapter {
constexpr UINT ChangedMessage = WM_APP + 120;
constexpr UINT ServiceMessage = WM_APP + 121;
constexpr UINT CategoryMessage = WM_APP + 86;
constexpr UINT PrepareCloseMessage = WM_APP + 87;
constexpr UINT_PTR SampleTimer = 0x52414341;
constexpr UINT_PTR PaneSubclass = 0x636f6e70, ControlSubclass = 0x636f6e63;
constexpr DWORD SamplePeriod = 200;
class Host;
namespace detail {
inline uint64_t utcMilliseconds(){
 FILETIME time{};GetSystemTimeAsFileTime(&time);
 const uint64_t ticks=(uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime;
 return ticks/10000-11644473600000ull;
}
inline std::string utf8(const std::wstring& value) {
 if(value.empty())return {};
 int n=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
 if(n<=0)return {};std::string result(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),result.data(),n,nullptr,nullptr);return result;
}
inline bool wide(const std::string& value,std::wstring& result) {
 result.clear();if(value.empty())return true;
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
 if(n<=0)return false;result.resize(size_t(n));return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),n)==n;
}
inline std::wstring text(HWND w) {
 int n=std::clamp(GetWindowTextLengthW(w),0,32767);std::vector<wchar_t> value(size_t(n)+1);
 GetWindowTextW(w,value.data(),n+1);return value.data();
}
inline bool status(HWND w,std::string& value) {
 if(!GetPropW(w,L"RA2.Console.Status"))return false;
 // Read on the owning thread, including hidden compatibility labels. An
 // optional stable string object keeps native hover help out of real status.
 const auto* source=reinterpret_cast<const std::wstring*>(GetPropW(w,L"RA2.Console.StatusText"));
 value=utf8(source?*source:text(w));return true;
}
inline std::wstring className(HWND w) {wchar_t value[128]{};GetClassNameW(w,value,128);return value;}
inline bool sameClass(const std::wstring& a,const wchar_t* b){return _wcsicmp(a.c_str(),b)==0;}
inline bool visibleStyle(HWND w,HWND pane) {
 for(HWND at=w;at&&at!=pane;at=GetParent(at))if(!(GetWindowLongPtrW(at,GWL_STYLE)&WS_VISIBLE))return false;
 return w!=pane;
}
inline bool enabled(HWND w,HWND pane) {
 for(HWND at=w;at&&at!=pane;at=GetParent(at))if(GetWindowLongPtrW(at,GWL_STYLE)&WS_DISABLED)return false;
 return true;
}
inline DWORD_PTR buttonType(HWND w){DWORD_PTR type=DWORD_PTR(GetWindowLongPtrW(w,GWL_STYLE))&BS_TYPEMASK,original=0;if(GetWindowSubclass(w,modernui::buttonProc,modernui::ButtonSubclass,&original))type=original;return type;}
inline bool toggle(DWORD_PTR type){return type==BS_CHECKBOX||type==BS_AUTOCHECKBOX||type==BS_3STATE||type==BS_AUTO3STATE;}
inline bool complex(const std::wstring& cls){return sameClass(cls,L"COMBOBOX")||sameClass(cls,L"LISTBOX")||sameClass(cls,WC_LISTVIEWW)||sameClass(cls,WC_TABCONTROLW)||sameClass(cls,HOTKEY_CLASSW)||sameClass(cls,TRACKBAR_CLASSW);}
inline bool nestedImplementation(HWND w,HWND pane) {
 for(HWND parent=GetParent(w);parent&&parent!=pane;parent=GetParent(parent))if(complex(className(parent)))return true;return false;
}
inline std::vector<std::string> options(HWND w,const std::wstring& cls,std::string* identity=nullptr) {
 std::vector<std::string> result;const bool combo=sameClass(cls,L"COMBOBOX"),list=sameClass(cls,L"LISTBOX"),tabs=sameClass(cls,WC_TABCONTROLW),view=sameClass(cls,WC_LISTVIEWW);
 int count=combo?int(SendMessageW(w,CB_GETCOUNT,0,0)):list?int(SendMessageW(w,LB_GETCOUNT,0,0)):tabs?TabCtrl_GetItemCount(w):view?ListView_GetItemCount(w):0;
 count=std::clamp(count,0,65536);result.reserve(size_t(count));
 for(int i=0;i<count;++i){std::wstring label;LPARAM item=0;
  if(combo||list){const auto style=GetWindowLongPtrW(w,GWL_STYLE);const bool strings=combo?!(style&(CBS_OWNERDRAWFIXED|CBS_OWNERDRAWVARIABLE))||(style&CBS_HASSTRINGS):!(style&(LBS_OWNERDRAWFIXED|LBS_OWNERDRAWVARIABLE))||(style&LBS_HASSTRINGS);if(strings){UINT length=combo?CB_GETLBTEXTLEN:LB_GETTEXTLEN,get=combo?CB_GETLBTEXT:LB_GETTEXT;int n=int(SendMessageW(w,length,i,0));if(n<0||n>32767)n=0;std::vector<wchar_t> value(size_t(n)+1);if(n)SendMessageW(w,get,i,reinterpret_cast<LPARAM>(value.data()));label=value.data();}else label=L"原生绘制选项 "+std::to_wstring(i+1);item=SendMessageW(w,combo?CB_GETITEMDATA:LB_GETITEMDATA,i,0);}
  else if(tabs){wchar_t value[2048]{};TCITEMW field{};field.mask=TCIF_TEXT|TCIF_PARAM;field.pszText=value;field.cchTextMax=2048;if(TabCtrl_GetItem(w,i,&field)){label=value;item=field.lParam;}}
  else if(view){wchar_t value[2048]{};LVITEMW field{};field.mask=LVIF_TEXT|LVIF_PARAM;field.iItem=i;field.pszText=value;field.cchTextMax=2048;if(SendMessageW(w,LVM_GETITEMW,0,reinterpret_cast<LPARAM>(&field))){label=value;item=field.lParam;}}
  result.push_back(utf8(label));if(identity){*identity+=std::to_string(result.back().size())+":"+result.back()+":"+std::to_string(int64_t(item))+";";}
 }return result;
}
inline bool dirtyMessage(UINT m){return m==WM_SETTEXT||m==WM_ENABLE||m==WM_SHOWWINDOW||m==WM_WINDOWPOSCHANGED||m==BM_SETCHECK||m==HKM_SETHOTKEY||m==CB_SETCURSEL||m==CB_RESETCONTENT||m==CB_ADDSTRING||m==CB_INSERTSTRING||m==CB_DELETESTRING||m==CB_SETITEMDATA||m==LB_SETCURSEL||m==LB_SETSEL||m==LB_RESETCONTENT||m==LB_ADDSTRING||m==LB_INSERTSTRING||m==LB_DELETESTRING||m==LB_SETITEMDATA||m==TBM_SETPOS||m==TCM_SETCURSEL||m==LVM_SETITEMSTATE||m==LVM_INSERTITEMW||m==LVM_DELETEITEM||m==LVM_DELETEALLITEMS||m==LVM_SETITEMW;}
}

class Host {
 struct Navigation {unsigned module=0;int category=-1;std::string page="video";uint64_t serial=1;};
 struct Entry {uint32_t token=0;HWND window=nullptr;std::wstring cls;std::string type,presentation,optionIdentity;bool monitored=false,published=false;};
 struct Port {
  Host* host=nullptr;unsigned module=0;std::atomic<HWND> window{nullptr};DWORD owner=0;WNDPROC direct=nullptr;
  consolemodel::ActionMailbox actions{256};std::mutex submissionMutex;std::atomic<bool> wake{false},dirty{true};
  bool applying=false,capturing=false,servicing=false,deferredPrepare=false;WPARAM prepareWparam=0;LPARAM prepareLparam=0;uint64_t navigation=0,generation=0,revision=0,lastSequence=0,lastCapture=0,lastPeriodic=0;
  std::unordered_map<HWND,Entry> entries;std::string lastContent;
 };
 struct NativeControl {HWND window=nullptr;RECT rect{};consolemodel::Control control;std::string optionIdentity;};
 std::array<Port,3> ports_{};
 std::atomic<HWND> shell_{nullptr};std::atomic<bool> notificationPending_{false},closing_{false};
 std::atomic<uint32_t> nextToken_{1};std::atomic<uint64_t> nextGeneration_{1};std::atomic<unsigned> outstanding_{0};
 consolemodel::BoundedMailbox<consolemodel::Ack> acknowledgements_{512};
 mutable std::mutex navigationMutex_;Navigation selected_{};
 std::shared_ptr<const consolemodel::Snapshot> latest_;bool snapshotPending_=false;
 Navigation navigation()const{std::lock_guard<std::mutex> lock(navigationMutex_);return selected_;}
 void notify(){auto w=shell_.load();if(w&&!notificationPending_.exchange(true)&&!PostMessageW(w,ChangedMessage,0,0))notificationPending_=false;}
 void wake(Port& p){if(auto w=p.window.load())if(!p.wake.exchange(true)&&!PostMessageW(w,ServiceMessage,0,0))p.wake=false;}
 void acknowledge(const consolemodel::Action& a,bool okay,const std::string& error={},uint64_t revision=0) {
  consolemodel::Ack ack{a.module,a.generation,a.id,a.seq,okay,error};
  ack.revision=okay?revision:0;
  // Admission reserves one of these 512 slots until its receipt is consumed.
  if(acknowledgements_.tryPush(std::move(ack)))notify();
 }
 static LRESULT CALLBACK controlProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR data) {
  auto& p=*reinterpret_cast<Port*>(data);
  if(m==WM_NCDESTROY){p.entries.erase(w);p.dirty=true;RemoveWindowSubclass(w,controlProc,id);return DefSubclassProc(w,m,a,b);}
  auto result=DefSubclassProc(w,m,a,b);if(detail::dirtyMessage(m))p.dirty=true;return result;
 }
 static LRESULT CALLBACK paneProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR data) {
  auto& p=*reinterpret_cast<Port*>(data);auto& host=*p.host;
  if(m==WM_WINDOWPOSCHANGING&&b){auto* pos=reinterpret_cast<WINDOWPOS*>(b);pos->flags=(pos->flags&~SWP_SHOWWINDOW)|SWP_HIDEWINDOW;}
  if(m==ServiceMessage){p.wake=false;host.service(p);return 0;}
  if(m==WM_TIMER&&a==SampleTimer){host.service(p);return 0;}
  if(m==PrepareCloseMessage&&p.servicing){if(!p.deferredPrepare){p.prepareWparam=a;p.prepareLparam=b;}p.deferredPrepare=true;return 0;}
  if(p.applying&&(m==WM_COMMAND||m==WM_NOTIFY||m==WM_HSCROLL||m==WM_VSCROLL))return 0;
  if(m==WM_NCDESTROY){KillTimer(w,SampleTimer);{std::lock_guard<std::mutex> admission(p.submissionMutex);p.window=nullptr;consolemodel::Action action;while(p.actions.tryPop(action))host.acknowledge(action,false,"后台模块已退出");}p.owner=0;RemovePropW(w,L"RA2.Console.Adapter");RemovePropW(w,L"RA2.Suite.Managed");RemoveWindowSubclass(w,paneProc,id);p.entries.clear();host.notify();return DefSubclassProc(w,m,a,b);}
  auto result=DefSubclassProc(w,m,a,b);
  if(m==WM_COMMAND||m==WM_NOTIFY||m==WM_PARENTNOTIFY)p.dirty=true;
  return result;
 }
 static BOOL CALLBACK enumerate(HWND w,LPARAM context) {
  auto& p=*reinterpret_cast<Port*>(context);if(GetWindowThreadProcessId(w,nullptr)!=p.owner)return TRUE;
  HWND pane=p.window.load();if(detail::nestedImplementation(w,pane))return TRUE;
  if(p.entries.find(w)==p.entries.end()){
   uint32_t token=p.host->nextToken_.fetch_add(1);if(!token)return FALSE;
   Entry entry;entry.token=token;entry.window=w;entry.cls=detail::className(w);entry.monitored=SetWindowSubclass(w,controlProc,ControlSubclass,reinterpret_cast<DWORD_PTR>(&p))!=FALSE;p.entries.emplace(w,std::move(entry));
  }return TRUE;
 }
 static bool numericText(const std::wstring& text){if(text.empty())return true;size_t at=text[0]==L'-'||text[0]==L'+'?1:0;for(;at<text.size();++at)if(text[at]<L'0'||text[at]>L'9')return false;return true;}
 NativeControl read(Port& p,Entry& entry) {
  NativeControl out;out.window=entry.window;auto& c=out.control;HWND w=entry.window;const auto& cls=entry.cls;
  c.id=entry.token;c.enabled=detail::enabled(w,p.window.load());c.visible=detail::visibleStyle(w,p.window.load());c.text=detail::utf8(detail::text(w));c.label=c.text;
  GetWindowRect(w,&out.rect);MapWindowPoints(nullptr,p.window.load(),reinterpret_cast<POINT*>(&out.rect),2);c.row=out.rect.top;c.column=out.rect.left;
  // These are lifetime-stable metadata supplied on the owning module thread.
  // The compatibility tree's old folds/scroll offsets do not own DOM layout.
  if(auto group=reinterpret_cast<const wchar_t*>(GetPropW(w,L"RA2.Console.Group")))c.group=detail::utf8(group);
  if(auto debug=reinterpret_cast<const consolemodel::DebugEntry*>(GetPropW(w,L"RA2.Console.DebugEntry")))c.debug=*debug;
   c.runtimeError=GetPropW(w,L"RA2.Console.RuntimeError")!=nullptr;
   c.groupHeader=GetPropW(w,L"RA2.Console.GroupHeader")!=nullptr;
   c.collapsible=GetPropW(w,L"RA2.Console.Collapsible")!=nullptr;
   c.collapsedDefault=GetPropW(w,L"RA2.Console.CollapsedDefault")!=nullptr;
   c.immediate=GetPropW(w,L"RA2.Console.Immediate")!=nullptr;
  if(GetPropW(w,L"RA2.Console.Skip"))c.visible=false;
  if(auto row=GetPropW(w,L"RA2.Console.Row")){const LONG top=LONG(reinterpret_cast<INT_PTR>(row)-1),height=MulDiv(out.rect.bottom-out.rect.top,96,int(modernui::windowDpi(w)));out.rect.top=top;out.rect.bottom=top+height;c.row=int(top);}
  if(auto column=GetPropW(w,L"RA2.Console.Column")){const LONG left=LONG(reinterpret_cast<INT_PTR>(column)-1),width=MulDiv(out.rect.right-out.rect.left,96,int(modernui::windowDpi(w)));out.rect.left=left;out.rect.right=left+width;c.column=int(left);}
  if(auto hint=reinterpret_cast<std::wstring*>(GetPropW(w,L"RA2.Tip.Text")))c.hint=detail::utf8(*hint);
  const auto style=GetWindowLongPtrW(w,GWL_STYLE);
  if(detail::sameClass(cls,L"STATIC")){c.type=GetPropW(w,L"RA2.Heading.Font")?"heading":"label";c.readonly=true;}
  else if(detail::sameClass(cls,L"BUTTON")){auto type=detail::buttonType(w);c.type=detail::toggle(type)?"toggle":type==BS_GROUPBOX?"heading":"button";c.checked=SendMessageW(w,BM_GETCHECK,0,0)==BST_CHECKED;c.readonly=type==BS_GROUPBOX;if(type==BS_RADIOBUTTON||type==BS_AUTORADIOBUTTON){c.enabled=false;c.readonly=true;c.hint+=" 原生单选组需要专用适配器，当前不可修改。";}}
  else if(detail::sameClass(cls,L"EDIT")||cls.find(L"RichEdit")==0||cls.find(L"RICHEDIT")==0){c.type=(style&ES_NUMBER)?"number":"text";c.readonly=(style&ES_READONLY)!=0;c.min=std::numeric_limits<int>::min();c.max=std::numeric_limits<int>::max();}
  else if(detail::sameClass(cls,HOTKEY_CLASSW)){c.type="hotkey";c.selected=int(SendMessageW(w,HKM_GETHOTKEY,0,0));bool hold=GetPropW(w,L"RA2.Modern.Hold")!=nullptr;c.text=detail::utf8(keygesture::describe(unsigned(c.selected),modernui::hotkeyName(unsigned(c.selected)),hold));c.options=hold?std::vector<std::string>{"hold"}:GetPropW(w,L"RA2.Modern.PressOnly")?std::vector<std::string>{"press"}:std::vector<std::string>{"press","release"};if(GetPropW(w,L"RA2.Modern.KeyboardOnly"))c.options.push_back("keyboardOnly");}
  else if(detail::sameClass(cls,L"COMBOBOX")||detail::sameClass(cls,L"LISTBOX")||detail::sameClass(cls,WC_TABCONTROLW)||detail::sameClass(cls,WC_LISTVIEWW)){
   const bool combo=detail::sameClass(cls,L"COMBOBOX"),list=detail::sameClass(cls,L"LISTBOX"),tabs=detail::sameClass(cls,WC_TABCONTROLW);
   c.type="choice";c.readonly=!combo||(style&3)==CBS_DROPDOWNLIST;c.presentation=list?"list":!c.readonly?"editable":"";
   c.options=detail::options(w,cls,&out.optionIdentity);
   if(auto mask=reinterpret_cast<uintptr_t>(GetPropW(w,disabledchoices::Property)))for(size_t i=0;i<c.options.size();++i)c.optionEnabled.push_back(i<32&&((mask>>i)&1));
   c.selected=combo?int(SendMessageW(w,CB_GETCURSEL,0,0)):list?int(SendMessageW(w,LB_GETCURSEL,0,0)):tabs?TabCtrl_GetCurSel(w):ListView_GetNextItem(w,-1,LVNI_SELECTED);
   if(list&&(style&(LBS_MULTIPLESEL|LBS_EXTENDEDSEL))){c.enabled=false;c.hint+=" 原生多选列表需要集合适配器，当前不可修改。";}
  }
  else if(detail::sameClass(cls,TRACKBAR_CLASSW)){c.type="range";c.min=int(SendMessageW(w,TBM_GETRANGEMIN,0,0));c.max=int(SendMessageW(w,TBM_GETRANGEMAX,0,0));c.step=std::max(1,int(SendMessageW(w,TBM_GETLINESIZE,0,0)));c.selected=int(SendMessageW(w,TBM_GETPOS,0,0));}
  else{c.type="label";c.enabled=false;c.readonly=true;c.label=c.label.empty()?detail::utf8(cls):c.label;c.hint+=" 未支持的原生控件："+detail::utf8(cls)+"；保留设置，未执行替代操作。";}
  if(!entry.monitored){c.enabled=false;c.readonly=true;c.hint+=" 无法监视此控件的生命周期，当前不可修改。";}entry.type=c.type;entry.presentation=c.presentation;return out;
 }
 void publish(Port& p,const Navigation& nav,consolemodel::Snapshot snapshot) {
  {std::lock_guard<std::mutex> lock(navigationMutex_);if(selected_.serial!=nav.serial||selected_.module!=p.module)return;latest_=std::make_shared<const consolemodel::Snapshot>(std::move(snapshot));snapshotPending_=true;}notify();
 }
 void capture(Port& p,const Navigation& nav,uint64_t now) {
  if(p.capturing||p.owner!=GetCurrentThreadId())return;p.capturing=true;p.lastCapture=p.lastPeriodic=now;p.dirty=false;
  EnumChildWindows(p.window.load(),enumerate,reinterpret_cast<LPARAM>(&p));
  std::vector<NativeControl> controls;controls.reserve(p.entries.size());std::string status;
  for(auto& pair:p.entries){if(!IsWindow(pair.first))continue;
   if(detail::status(pair.first,status))continue;
   if(!detail::visibleStyle(pair.first,p.window.load())||GetPropW(pair.first,L"RA2.Console.Skip"))continue;auto& entry=pair.second;auto previousType=entry.type;auto c=read(p,entry);
   // Only a replaced control/choice changes token. Other fields retain their
   // page generation and pending drafts when a dynamic catalogue refreshes.
   if(entry.published&&(previousType!=c.control.type||(c.control.type=="choice"&&entry.optionIdentity!=c.optionIdentity))){entry.token=nextToken_.fetch_add(1);c.control.id=entry.token;}
   entry.optionIdentity=c.optionIdentity;entry.published=true;
   if(c.control.visible&&(!c.control.text.empty()||c.control.type!="label"))controls.push_back(std::move(c));}
  std::sort(controls.begin(),controls.end(),[](const NativeControl&a,const NativeControl&b){if(a.rect.top!=b.rect.top)return a.rect.top<b.rect.top;if(a.rect.left!=b.rect.left)return a.rect.left<b.rect.left;return a.control.id<b.control.id;});
  std::vector<uint32_t> usedLabels;std::string group;
  for(auto& item:controls){auto& c=item.control;if(c.type=="heading"){group=c.group.empty()?c.text:c.group;if(c.group.empty())c.group=group;continue;}if(c.group.empty())c.group=group;
   if(c.type=="label"||c.type=="button"||c.type=="toggle")continue;
   const NativeControl* best=nullptr;long score=std::numeric_limits<long>::max();
   // A combo's native rectangle can include its dropdown height; use the
   // aligned row origin, not the centre of that tall rectangle, for labels.
   for(const auto& label:controls)if((label.control.type=="label"||label.control.type=="button"||label.control.type=="toggle")&&!label.control.text.empty()&&label.rect.right<=item.rect.left+6){long dy=std::abs(label.rect.top-item.rect.top);if(dy>24)continue;long candidate=dy*1000+item.rect.left-label.rect.right;if(candidate<score){score=candidate;best=&label;}}
   if(best){c.label=best->control.text;if(best->control.type=="label")usedLabels.push_back(best->control.id);}else if(c.label.empty())c.label="设置 "+std::to_string(GetDlgCtrlID(item.window));
  }
  consolemodel::Snapshot snapshot;snapshot.module=p.module;snapshot.page=nav.page;snapshot.status=std::move(status);snapshot.title=p.module==0?"视频":p.module==1?"监控视图":nav.category==1?"控制":nav.category==2?"自动功能":nav.category==3?"游戏规则":nav.category==4?"实验性功能":nav.category==5?"调试":"设置";
  for(auto& item:controls){auto& c=item.control;
   if(c.type=="label"&&std::find(usedLabels.begin(),usedLabels.end(),c.id)!=usedLabels.end())continue;
   snapshot.controls.push_back(std::move(c));
  }
  if(!p.generation){std::lock_guard<std::mutex> admission(p.submissionMutex);p.generation=nextGeneration_.fetch_add(1);}snapshot.generation=p.generation;snapshot.revision=1;
  // A quiet debug page still publishes the periodic capture: ageMs is relative
  // to this snapshot, so suppressing an unchanged sample would imply staleness.
  auto content=consolemodel::encodeSnapshot(snapshot);if(content!=p.lastContent||(p.module==2&&nav.category==5)){p.lastContent=std::move(content);snapshot.revision=++p.revision;publish(p,nav,std::move(snapshot));}
  p.capturing=false;
 }
 std::string apply(Port& p,const consolemodel::Action& action) {
  // Navigation is a later operation than actions already admitted to this
  // owner's FIFO. Complete those edits before replacing its page/session.
  if(action.generation!=p.generation)return "设置页面已经更新，请重新操作";
  if(action.seq<=p.lastSequence)return "重复或过期的操作序号";p.lastSequence=action.seq;
  Entry* entry=nullptr;for(auto& item:p.entries)if(item.second.token==action.id){entry=&item.second;break;}
  if(!entry||!IsWindow(entry->window))return "设置控件已经失效";HWND w=entry->window;
  if(!detail::visibleStyle(w,p.window.load())||!detail::enabled(w,p.window.load()))return "此设置当前不可用";
  auto current=read(p,*entry);auto& c=current.control;if(!c.visible)return "此控件不属于当前设置页面";if(!c.enabled)return c.hint.empty()?"此设置当前不可用":c.hint;
   // Immediate game actions resolve selection only at the current input
   // edge. Owner-thread stalls and browser delivery cannot renew that edge.
   if(c.immediate&&(action.action!="press"||!consolemodel::immediateFresh(action,GetTickCount64(),detail::utcMilliseconds())))return "本次即时操作已过期，请重新选择后点击；不会延后执行";
  if(c.type=="choice"&&current.optionIdentity!=entry->optionIdentity){entry->token=nextToken_.fetch_add(1);entry->optionIdentity=current.optionIdentity;p.lastContent.clear();p.dirty=true;return "选项清单已经更新，请重新选择";}
  std::wstring value;if(!detail::wide(action.text,value)||value.find(L'\0')!=std::wstring::npos)return "无效的设置文本";
  UINT notification=0;bool notifyCommand=true;const auto& verb=action.action;const int nativeId=GetDlgCtrlID(w);
  struct Applying {Port& p;explicit Applying(Port& v):p(v){p.applying=true;}~Applying(){p.applying=false;}};
  {
   Applying guard(p);
   if(c.type=="toggle"&&verb=="toggle"){SendMessageW(w,BM_SETCHECK,action.checked?BST_CHECKED:BST_UNCHECKED,0);notification=BN_CLICKED;}
   else if(c.type=="button"&&verb=="press")notification=BN_CLICKED;
   else if((c.type=="text"||c.type=="number")&&(verb=="text"||verb=="range")){
    if(c.readonly)return "此字段只读";if(verb=="range")value=std::to_wstring(action.value);if(c.type=="number"&&!numericText(value))return "请输入整数";SetWindowTextW(w,value.c_str());notification=EN_CHANGE;
   }
   else if(c.type=="hotkey"&&verb=="hotkey"){
    const unsigned key=unsigned(action.value);if(action.value<0||key>65535||(key&~unsigned(keygesture::Stored)))return "无效的快捷键编码";
    if((key&255)&&!modernui::hotkeyGestureAllowed(w,keygesture::mode(key)))return "此快捷键不支持所选手势";
    if((key&255)>0&&(key&255)<7&&GetPropW(w,L"RA2.Modern.KeyboardOnly"))return "此字段仅支持键盘";
    SendMessageW(w,HKM_SETHOTKEY,key,0);notification=EN_CHANGE;
   }
   else if(c.type=="choice"&&(verb=="select"||verb=="activate")){
    if(action.value<0||size_t(action.value)>=c.options.size())return "选择索引已经失效";
    if(size_t(action.value)<c.optionEnabled.size()&&!c.optionEnabled[action.value])return "此选项当前不可用";
    if(detail::sameClass(entry->cls,L"COMBOBOX")){if(verb=="activate")return "此字段不支持双击操作";SendMessageW(w,CB_SETCURSEL,action.value,0);notification=CBN_SELCHANGE;}
    else if(detail::sameClass(entry->cls,L"LISTBOX")){SendMessageW(w,LB_SETCURSEL,action.value,0);notification=verb=="activate"?LBN_DBLCLK:LBN_SELCHANGE;}
    else if(detail::sameClass(entry->cls,WC_TABCONTROLW)){if(verb=="activate")return "此字段不支持双击操作";TabCtrl_SetCurSel(w,action.value);NMHDR notice{w,UINT_PTR(nativeId),TCN_SELCHANGE};p.applying=false;p.direct(p.window.load(),WM_NOTIFY,nativeId,reinterpret_cast<LPARAM>(&notice));p.applying=true;notifyCommand=false;}
    else {ListView_SetItemState(w,-1,0,LVIS_SELECTED|LVIS_FOCUSED);ListView_SetItemState(w,action.value,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);NMITEMACTIVATE notice{};notice.hdr={w,UINT_PTR(nativeId),verb=="activate"?UINT(NM_DBLCLK):UINT(LVN_ITEMCHANGED)};notice.iItem=action.value;p.applying=false;p.direct(p.window.load(),WM_NOTIFY,nativeId,reinterpret_cast<LPARAM>(&notice));p.applying=true;notifyCommand=false;}
   }
   else if(c.type=="choice"&&!c.readonly&&verb=="text"){SetWindowTextW(w,value.c_str());notification=CBN_EDITCHANGE;}
   else if(c.type=="range"&&verb=="range"){
    if(action.value<c.min||action.value>c.max)return "数值超出允许范围";SendMessageW(w,TBM_SETPOS,TRUE,action.value);p.applying=false;p.direct(p.window.load(),WM_HSCROLL,MAKEWPARAM(TB_THUMBPOSITION,WORD(action.value)),reinterpret_cast<LPARAM>(w));p.direct(p.window.load(),WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,WORD(action.value)),reinterpret_cast<LPARAM>(w));p.applying=true;notifyCommand=false;
   }
   else return "此控件不支持该操作";
  }
  if(notifyCommand)p.direct(p.window.load(),WM_COMMAND,MAKEWPARAM(nativeId,notification),reinterpret_cast<LPARAM>(w));
  // A successful command also needs an authoritative state when the native
  // handler clamps it, or a rapid edit returns to the previously sent value.
  p.lastContent.clear();p.dirty=true;return {};
 }
 void service(Port& p) {
  if(p.owner!=GetCurrentThreadId()||!p.window.load())return;if(p.servicing){p.dirty=true;return;}
  struct ServiceGuard {Port& p;explicit ServiceGuard(Port& value):p(value){p.servicing=true;}~ServiceGuard(){p.servicing=false;if(p.deferredPrepare){auto pane=p.window.load();p.deferredPrepare=false;if(pane&&!PostMessageW(pane,PrepareCloseMessage,p.prepareWparam,p.prepareLparam))p.deferredPrepare=true;}}} guard(p);
  for(unsigned count=0;count<16;++count){consolemodel::Action action;if(!p.actions.tryPop(action))break;auto error=closing_.load()?std::string("工具箱正在退出"):apply(p,action);acknowledge(action,error.empty(),error,error.empty()?p.revision+1:0);}
  // A native modal loop may have processed WM_CLOSE before returning here.
  if(p.owner!=GetCurrentThreadId()||!p.window.load())return;
  Navigation nav;bool more=false,change=false;
  // A brief admission barrier makes "FIFO empty -> replace page generation"
  // indivisible with post(). Never hold it across a native window callback.
  {std::lock_guard<std::mutex> admission(p.submissionMutex);more=p.actions.size()!=0;if(!more){nav=navigation();if(nav.module==p.module&&nav.serial!=p.navigation){p.navigation=nav.serial;p.generation=nextGeneration_.fetch_add(1);p.lastSequence=0;p.lastContent.clear();p.dirty=true;change=true;}}}
  if(more){wake(p);return;}
  if(change&&p.module==2&&nav.category>=0&&p.direct)p.direct(p.window.load(),CategoryMessage,WPARAM(nav.category),0);
  nav=navigation();if(nav.module==p.module&&nav.serial!=p.navigation)wake(p);auto now=GetTickCount64();if(nav.module==p.module&&nav.serial==p.navigation&&(!p.lastCapture||now-p.lastCapture>=SamplePeriod)&&(p.dirty||!p.lastPeriodic||now-p.lastPeriodic>=1000))capture(p,nav,now);
  bool pending=false;{std::lock_guard<std::mutex> lock(navigationMutex_);pending=snapshotPending_;}if(pending||acknowledgements_.size())notify();
 }
public:
 Host(){for(unsigned i=0;i<ports_.size();++i){ports_[i].host=this;ports_[i].module=i;}}
 Host(const Host&)=delete;Host& operator=(const Host&)=delete;
 void shell(HWND w){shell_=w;}
 HWND shell()const{return shell_.load();}
 HWND window(unsigned module)const{return module<ports_.size()?ports_[module].window.load():nullptr;}
 bool managed(HWND w)const{for(const auto& p:ports_)if(w&&p.window.load()==w)return true;return false;}
 bool contains(HWND w)const{if(!w)return false;auto root=GetAncestor(w,GA_ROOT),owner=GetAncestor(w,GA_ROOTOWNER);auto shell=shell_.load();if(shell&&(w==shell||root==shell||owner==shell))return true;for(const auto& p:ports_)if(auto pane=p.window.load())if(w==pane||root==pane||owner==pane)return true;return false;}
 bool adopt(unsigned module,HWND pane,WNDPROC directProc=nullptr){
  // An actual owner procedure is required: GWLP_WNDPROC may be ComCtl's
  // subclass thunk and would re-run the automatic checkbox parent handler.
  if(module>=ports_.size()||!pane||!directProc||GetWindowThreadProcessId(pane,nullptr)!=GetCurrentThreadId())return false;auto& p=ports_[module];if(p.window.load()&&p.window.load()!=pane)return false;
  p.owner=GetCurrentThreadId();p.direct=directProc;p.navigation=0;p.lastCapture=0;p.dirty=true;
  if(!p.direct||!SetWindowSubclass(pane,paneProc,PaneSubclass,reinterpret_cast<DWORD_PTR>(&p)))return false;{std::lock_guard<std::mutex> admission(p.submissionMutex);p.window=pane;}
  SetPropW(pane,L"RA2.Console.Adapter",reinterpret_cast<HANDLE>(&p));SetPropW(pane,L"RA2.Suite.Managed",reinterpret_cast<HANDLE>(1));ShowWindow(pane,SW_HIDE);SetTimer(pane,SampleTimer,SamplePeriod,nullptr);service(p);return true;
 }
 bool navigate(unsigned module,int category=-1,const std::string& page={}){
  if(module>=ports_.size()||category < -1||category>5||(category==5&&module!=2))return false;{
   std::lock_guard<std::mutex> lock(navigationMutex_);std::string key=page.empty()?(module==0?"video":module==1?"monitor":"tactical/"+std::to_string(category)):page;
   if(selected_.module==module&&selected_.category==category&&selected_.page==key){ports_[module].dirty=true;}else{selected_.module=module;selected_.category=category;selected_.page=std::move(key);++selected_.serial;latest_.reset();snapshotPending_=false;}
  }wake(ports_[module]);return true;
 }
 void renewSession(){unsigned module=0;{std::lock_guard<std::mutex> lock(navigationMutex_);++selected_.serial;module=selected_.module;latest_.reset();snapshotPending_=false;}ports_[module].dirty=true;wake(ports_[module]);}
 bool post(unsigned module,UINT message,WPARAM a=0,LPARAM b=0){auto pane=window(module);return pane&&PostMessageW(pane,message,a,b);}
 bool post(const consolemodel::Action& action){
  if(closing_||action.module>=ports_.size()||!action.id||action.id>UINT32_MAX||!action.generation||!action.seq||action.generation>consolemodel::MaxSafeInteger||action.seq>consolemodel::MaxSafeInteger||action.text.size()>consolemodel::MaxTextBytes)return false;
  {std::lock_guard<std::mutex> lock(navigationMutex_);if(selected_.module!=action.module||!latest_||latest_->module!=action.module||latest_->generation!=action.generation)return false;}
  auto& p=ports_[action.module];std::lock_guard<std::mutex> admission(p.submissionMutex);if(closing_||!p.window.load()||action.generation!=p.generation)return false;
  unsigned count=outstanding_.load();while(count<512&&!outstanding_.compare_exchange_weak(count,count+1)){}if(count>=512)return false;
  auto admitted=action;admitted.receivedAt=GetTickCount64();
  if(!p.actions.tryPush(admitted)){--outstanding_;return false;}wake(p);return true;
 }
 void dirty(unsigned module){if(module<ports_.size()){ports_[module].dirty=true;wake(ports_[module]);}}
 std::shared_ptr<const consolemodel::Snapshot> takeSnapshot(){notificationPending_=false;std::lock_guard<std::mutex> lock(navigationMutex_);if(!snapshotPending_)return {};snapshotPending_=false;return latest_;}
 std::shared_ptr<const consolemodel::Snapshot> latestSnapshot()const{std::lock_guard<std::mutex> lock(navigationMutex_);return latest_;}
 bool takeAck(consolemodel::Ack& ack){notificationPending_=false;if(!acknowledgements_.tryPop(ack))return false;--outstanding_;return true;}
 void closing(){closing_=true;}
};
} // namespace consoleadapter
