// Real Win32 fixture: compile/run on Windows; cross compilation alone is not
// evidence that native control dispatch or desktop visibility passed.
#define UNICODE
#define _UNICODE
#include "../source/console_adapter.h"
#include <cstdio>
#include <atomic>
#include <functional>

namespace {
using consoleadapter::Host;
constexpr UINT MutateChoice=WM_APP+180,TryShow=WM_APP+181,ReplaceEdit=WM_APP+182,SetMetadata=WM_APP+183;
struct Fixture {
 Host host;HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),mutated=CreateEventW(nullptr,FALSE,FALSE,nullptr),nestedEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr),nestedRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 HWND pane=nullptr,toggle=nullptr,edit=nullptr,choice=nullptr,list=nullptr,hotkey=nullptr,slider=nullptr,readonlyEdit=nullptr,tabs=nullptr,noIdEdit=nullptr,editableCombo=nullptr;
 std::atomic<DWORD> thread{0},actionThread{0};std::atomic<unsigned> toggles{0},edits{0},selections{0},activations{0},keys{0},scrolls{0},tabChanges{0},anonymousEdits{0},comboEdits{0};
 std::atomic<int> checked{0},key{0},position{0};std::atomic<bool> hidden{false},dialogRecognized{false},nested{false},reentered{false};
 consolemodel::Ack lastAck;
 ~Fixture(){CloseHandle(ready);CloseHandle(mutated);CloseHandle(nestedEntered);CloseHandle(nestedRelease);}
};
Fixture* fixture=nullptr;
int failures=0;
void check(bool value,const char* label){if(!value){++failures;std::fprintf(stderr,"FAIL %s\n",label);}}
LRESULT CALLBACK paneProc(HWND w,UINT m,WPARAM a,LPARAM b){
 auto& f=*fixture;
 if(m==WM_CREATE){
  auto ctl=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int id){return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,26,w,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);};
  modernui::heading(ctl(L"STATIC",L"默认分组",0,10,-26,260,0));
  f.toggle=ctl(L"BUTTON",L"开关",BS_AUTOCHECKBOX,10,10,180,100);modernui::style(f.toggle);
  ctl(L"STATIC",L"名称",0,10,50,80,0);f.edit=ctl(L"EDIT",L"初始",ES_AUTOHSCROLL,110,50,160,101);
  ctl(L"STATIC",L"选择",0,10,90,80,0);f.choice=ctl(L"COMBOBOX",L"",CBS_DROPDOWNLIST,110,90,160,102);
  SendMessageW(f.choice,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"甲"));SendMessageW(f.choice,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"乙"));SendMessageW(f.choice,CB_SETITEMDATA,0,11);SendMessageW(f.choice,CB_SETITEMDATA,1,22);SendMessageW(f.choice,CB_SETCURSEL,0,0);
  f.list=ctl(L"LISTBOX",L"",LBS_NOTIFY,10,130,260,103);SendMessageW(f.list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"生产甲"));SendMessageW(f.list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"生产乙"));SendMessageW(f.list,LB_SETCURSEL,0,0);
  ctl(L"STATIC",L"快捷键",0,10,170,80,0);f.hotkey=ctl(HOTKEY_CLASSW,L"",0,110,170,160,104);modernui::style(f.hotkey);modernui::holdKey(f.hotkey);SendMessageW(f.hotkey,HKM_SETHOTKEY,0xb011,0);
  f.slider=ctl(TRACKBAR_CLASSW,L"",TBS_HORZ,10,210,260,105);SendMessageW(f.slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,20));SendMessageW(f.slider,TBM_SETPOS,TRUE,3);
  f.readonlyEdit=ctl(L"EDIT",L"只读",ES_READONLY,10,250,260,106);
  f.tabs=ctl(WC_TABCONTROLW,L"",0,10,290,260,107);TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(L"第一页");TabCtrl_InsertItem(f.tabs,0,&item);item.pszText=const_cast<wchar_t*>(L"第二页");TabCtrl_InsertItem(f.tabs,1,&item);
  ctl(L"STATIC",L"无数字ID",0,10,330,80,0);f.noIdEdit=ctl(L"EDIT",L"独立",0,110,330,160,0);
  ctl(WC_TREEVIEWW,L"原生树",0,10,370,260,108);
  ctl(L"STATIC",L"可输入",0,10,410,80,0);f.editableCombo=ctl(L"COMBOBOX",L"125",CBS_DROPDOWN,110,410,160,109);SendMessageW(f.editableCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"100"));SendMessageW(f.editableCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"150"));SetWindowTextW(f.editableCombo,L"125");
  ctl(L"BUTTON",L"嵌套任务",BS_PUSHBUTTON,10,450,260,111);
  return 0;
 }
 if(m==WM_COMMAND){f.actionThread=GetCurrentThreadId();int id=LOWORD(a),notification=HIWORD(a);
  if(f.nested)f.reentered=true;
  if(id==111&&notification==BN_CLICKED){f.nested=true;SetEvent(f.nestedEntered);DWORD started=GetTickCount();while(WaitForSingleObject(f.nestedRelease,0)!=WAIT_OBJECT_0&&DWORD(GetTickCount()-started)<4000){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}f.nested=false;return 0;}
  if(id==100&&notification==BN_CLICKED){++f.toggles;f.checked=int(SendMessageW(f.toggle,BM_GETCHECK,0,0));}
  if(id==101&&notification==EN_CHANGE)++f.edits;
  if(id==102&&notification==CBN_SELCHANGE)++f.selections;
  if(id==103&&notification==LBN_DBLCLK)++f.activations;
  if(id==104&&notification==EN_CHANGE){++f.keys;f.key=int(SendMessageW(f.hotkey,HKM_GETHOTKEY,0,0));}
  if(!id&&reinterpret_cast<HWND>(b)==f.noIdEdit&&notification==EN_CHANGE)++f.anonymousEdits;
  if(id==109&&notification==CBN_EDITCHANGE)++f.comboEdits;
  return 0;
 }
 if(m==WM_HSCROLL&&reinterpret_cast<HWND>(b)==f.slider){f.actionThread=GetCurrentThreadId();++f.scrolls;f.position=int(SendMessageW(f.slider,TBM_GETPOS,0,0));return 0;}
 if(m==WM_NOTIFY&&b){auto* hdr=reinterpret_cast<NMHDR*>(b);if(hdr->hwndFrom==f.tabs&&hdr->code==TCN_SELCHANGE){f.actionThread=GetCurrentThreadId();++f.tabChanges;}return 0;}
 if(m==MutateChoice){SendMessageW(f.choice,CB_SETITEMDATA,1,333);SetEvent(f.mutated);return 0;}
 if(m==ReplaceEdit){DestroyWindow(f.edit);f.edit=CreateWindowW(L"EDIT",L"替换",WS_CHILD|WS_VISIBLE,110,50,160,26,w,reinterpret_cast<HMENU>(INT_PTR(101)),GetModuleHandleW(nullptr),nullptr);SetEvent(f.mutated);return 0;}
 if(m==SetMetadata){SetPropW(f.toggle,L"RA2.Console.Group",const_cast<wchar_t*>(L"固定语义组"));SetPropW(f.toggle,L"RA2.Console.Row",reinterpret_cast<HANDLE>(INT_PTR(101)));SetPropW(f.toggle,L"RA2.Console.Column",reinterpret_cast<HANDLE>(INT_PTR(21)));SetPropW(f.edit,L"RA2.Console.Skip",reinterpret_cast<HANDLE>(1));f.host.dirty(0);SetEvent(f.mutated);return 0;}
 if(m==TryShow){ShowWindow(w,SW_SHOW);f.hidden=!IsWindowVisible(w);HWND dialog=CreateWindowW(L"STATIC",L"Native dialog",WS_POPUP,0,0,50,50,w,nullptr,GetModuleHandleW(nullptr),nullptr);f.dialogRecognized=f.host.contains(dialog);DestroyWindow(dialog);SetEvent(f.mutated);return 0;}
 if(m==WM_CLOSE){DestroyWindow(w);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
DWORD WINAPI worker(void*){
 auto& f=*fixture;f.thread=GetCurrentThreadId();WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=paneProc;wc.lpszClassName=L"RA2.Console.Adapter.Fixture";RegisterClassW(&wc);
 f.pane=CreateWindowW(wc.lpszClassName,L"后台控制",WS_OVERLAPPEDWINDOW,0,0,400,460,nullptr,nullptr,wc.hInstance,nullptr);
 check(f.host.adopt(0,f.pane,paneProc),"owner adopts pane");f.hidden=!IsWindowVisible(f.pane);f.toggles=f.edits=f.selections=f.activations=f.keys=f.scrolls=f.tabChanges=f.anonymousEdits=f.comboEdits=0;SetEvent(f.ready);
 MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
bool pumpUntil(const std::function<bool()>& predicate,DWORD timeout=4000){
 auto start=GetTickCount();while(DWORD(GetTickCount()-start)<timeout){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(predicate())return true;MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}return predicate();
}
const consolemodel::Control* find(const consolemodel::Snapshot& snapshot,const std::string& type,const std::string& text={}){
 for(const auto& control:snapshot.controls)if(control.type==type&&(text.empty()||control.label==text||control.text==text))return &control;return nullptr;
}
std::shared_ptr<const consolemodel::Snapshot> awaitSnapshot(Fixture& f,const std::function<bool(const consolemodel::Snapshot&)>& predicate){
 std::shared_ptr<const consolemodel::Snapshot> result;pumpUntil([&]{auto snapshot=f.host.latestSnapshot();if(snapshot&&predicate(*snapshot)){result=std::move(snapshot);return true;}return false;});check(bool(result),"snapshot arrived");return result;
}
bool action(Fixture& f,const consolemodel::Snapshot& snapshot,uint32_t id,const char* verb,uint64_t seq,int value=0,const std::string& text={},bool checked=false,bool expected=true){
 consolemodel::Action a;a.module=snapshot.module;a.generation=snapshot.generation;a.id=id;a.seq=seq;a.action=verb;a.value=value;a.text=text;a.checked=checked;
 bool admitted=f.host.post(a);if(!admitted){check(!expected,"invalid action rejected at admission");return false;}consolemodel::Ack receipt;bool got=false;pumpUntil([&]{while(f.host.takeAck(receipt))if(receipt.seq==seq){got=true;return true;}return false;});check(got,"action receipt");if(got){f.lastAck=receipt;check(receipt.ok==expected,"action result");check(receipt.ok?receipt.revision>0:receipt.revision==0,"success receipt promises a future authoritative revision");}return got&&receipt.ok;
}
}
int main(){
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES|ICC_BAR_CLASSES|ICC_TAB_CLASSES|ICC_TREEVIEW_CLASSES};InitCommonControlsEx(&cc);Fixture f;fixture=&f;
 HWND shell=CreateWindowW(L"STATIC",L"Console fixture",WS_OVERLAPPED,0,0,200,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);f.host.shell(shell);
 HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);check(thread!=nullptr,"worker created");check(WaitForSingleObject(f.ready,4000)==WAIT_OBJECT_0,"worker ready");check(f.hidden,"pane starts hidden");check(!f.host.adopt(1,f.pane,paneProc),"wrong-thread adoption rejected");
 auto snapshot=awaitSnapshot(f,[](const consolemodel::Snapshot& s){return s.controls.size()>=9;});
 if(snapshot){
  auto toggle=find(*snapshot,"toggle");auto edit=find(*snapshot,"text","名称");auto choice=find(*snapshot,"choice","选择");auto hotkey=find(*snapshot,"hotkey","快捷键");auto slider=find(*snapshot,"range");auto list=find(*snapshot,"choice");auto readonlyEdit=find(*snapshot,"text","只读");auto anonymous=find(*snapshot,"text","无数字ID");auto editable=find(*snapshot,"choice","可输入");const consolemodel::Control* tabs=nullptr;
  for(const auto& control:snapshot->controls)if(control.presentation=="list")list=&control;
  for(const auto& control:snapshot->controls)if(control.type=="choice"&&control.options.size()==2&&control.options[0]=="第一页")tabs=&control;
  check(toggle&&edit&&choice&&hotkey&&slider&&list&&readonlyEdit&&anonymous&&editable&&tabs,"typed controls retained");
  bool unsupported=false;for(const auto& control:snapshot->controls)if(control.hint.find("未支持的原生控件")!=std::string::npos)unsupported=true;check(unsupported,"unsupported setting explicitly represented");
  if(toggle){action(f,*snapshot,toggle->id,"toggle",1,0,{},true);check(f.checked==BST_CHECKED&&f.toggles==1,"checkbox toggled exactly once");action(f,*snapshot,toggle->id,"toggle",1,0,{},false,false);check(f.toggles==1,"duplicate command not replayed");}
  if(edit){action(f,*snapshot,edit->id,"text",2,0,"更新");check(f.edits==1,"edit sends exactly one notification");}
  if(choice){action(f,*snapshot,choice->id,"select",3,1);check(f.selections==1,"choice semantic notification");}
  if(list){action(f,*snapshot,list->id,"activate",4,1);check(f.activations==1,"list double-click preserved");}
  if(hotkey){action(f,*snapshot,hotkey->id,"hotkey",5,0xb011);check(f.key==0xb011&&f.keys==1,"double-hold bare modifier roundtrip");}
  if(slider){action(f,*snapshot,slider->id,"range",6,13);check(f.position==13&&f.scrolls==2,"slider uses WM_HSCROLL");}
  if(tabs){action(f,*snapshot,tabs->id,"select",7,1);check(f.tabChanges==1,"tab selection uses WM_NOTIFY");}
  if(readonlyEdit)action(f,*snapshot,readonlyEdit->id,"text",8,0,"非法",false,false);
  if(anonymous){action(f,*snapshot,anonymous->id,"text",9,0,"无ID更新");check(f.anonymousEdits==1,"opaque tokens support numeric ID zero");}
  if(editable){check(editable->presentation=="editable"&&!editable->readonly,"editable combo presentation");action(f,*snapshot,editable->id,"text",10,0,"137");check(f.comboEdits==1,"editable combo keeps arbitrary text");}
  check(f.actionThread==f.thread,"handlers run only on owning thread");
  if(choice){PostMessageW(f.pane,MutateChoice,0,0);WaitForSingleObject(f.mutated,4000);action(f,*snapshot,choice->id,"select",11,1,{},false,false);check(f.selections==1,"same-label changed item identity rejects stale index");}
  auto next=awaitSnapshot(f,[&](const consolemodel::Snapshot& s){const auto* selected=find(s,"choice","选择");return choice&&selected&&selected->id!=choice->id;});
  if(next){check(next->generation==snapshot->generation,"catalogue change preserves page generation");const auto* preserved=find(*next,"text","名称");check(edit&&preserved&&preserved->id==edit->id,"catalogue change preserves unrelated edit token");}
  if(next&&edit){auto oldToken=edit->id;PostMessageW(f.pane,ReplaceEdit,0,0);WaitForSingleObject(f.mutated,4000);auto replaced=awaitSnapshot(f,[&](const consolemodel::Snapshot& s){auto replacement=find(s,"text","名称");return replacement&&replacement->id!=oldToken;});if(replaced){consolemodel::Snapshot old=*replaced;action(f,old,oldToken,"text",12,0,"失效",false,false);}}
  auto before=f.host.latestSnapshot();f.host.renewSession();check(!f.host.latestSnapshot(),"renew clears previous renderer session");auto renewed=awaitSnapshot(f,[&](const consolemodel::Snapshot& s){return before&&s.generation!=before->generation;});
  if(renewed&&toggle){action(f,*renewed,toggle->id,"toggle",1,0,{},false);check(f.toggles==2,"new renderer sequence starts afresh");action(f,*before,toggle->id,"toggle",1000,0,{},true,false);action(f,*renewed,toggle->id,"toggle",2,0,{},true);check(f.toggles==3,"stale generation cannot poison new sequence");}
  auto settled=awaitSnapshot(f,[](const consolemodel::Snapshot& s){const auto* control=find(s,"toggle");return control&&control->checked;});
  if(settled&&toggle){action(f,*settled,toggle->id,"toggle",3,0,{},true);auto confirmation=awaitSnapshot(f,[&](const consolemodel::Snapshot& s){return s.revision>settled->revision&&s.revision>=f.lastAck.revision;});check(confirmation&&find(*confirmation,"toggle")->checked,"unchanged value still publishes promised authoritative command state");}
  auto modal=f.host.latestSnapshot();const auto* nestedButton=modal?find(*modal,"button","嵌套任务"):nullptr;
  if(modal&&nestedButton&&toggle){consolemodel::Action first;first.module=0;first.generation=modal->generation;first.id=nestedButton->id;first.seq=4;first.action="press";check(f.host.post(first),"nested native command admitted");check(WaitForSingleObject(f.nestedEntered,4000)==WAIT_OBJECT_0,"nested message pump entered");consolemodel::Action second=first;second.id=toggle->id;second.seq=5;second.action="toggle";second.checked=false;unsigned prior=f.toggles;check(f.host.post(second),"second command queued behind native modal");pumpUntil([&]{return f.reentered.load();},150);check(!f.reentered&&f.toggles==prior,"nested native pump cannot reenter service");SetEvent(f.nestedRelease);std::vector<uint64_t> receipts;pumpUntil([&]{consolemodel::Ack ack;while(f.host.takeAck(ack)){if(ack.seq==4||ack.seq==5){check(ack.ok,"modal command accepted");receipts.push_back(ack.seq);}}return receipts.size()==2;});check(receipts==std::vector<uint64_t>({4,5}),"modal acknowledgements preserve FIFO");check(f.toggles==prior+1,"queued edit executes once after modal returns");}
  auto beforeMetadata=f.host.latestSnapshot();const auto* skip=beforeMetadata?find(*beforeMetadata,"text","名称"):nullptr;const uint32_t skipped=skip?skip->id:0;
  PostMessageW(f.pane,SetMetadata,0,0);WaitForSingleObject(f.mutated,4000);auto metadata=awaitSnapshot(f,[](const consolemodel::Snapshot& s){const auto* control=find(s,"toggle");return control&&control->group=="固定语义组";});
  if(metadata){const auto* control=find(*metadata,"toggle");check(control&&control->row==100&&control->column==20,"logical coordinates override old native scroll layout");check(!find(*metadata,"text","名称"),"Skip metadata removes compatibility-only field");if(skipped)action(f,*metadata,skipped,"text",6,0,"不可写",false,false);
   consolemodel::Action beforeNavigation;beforeNavigation.module=0;beforeNavigation.generation=metadata->generation;beforeNavigation.id=control->id;beforeNavigation.seq=7;beforeNavigation.action="toggle";beforeNavigation.checked=true;check(f.host.post(beforeNavigation),"blur edit admitted before navigation");f.host.navigate(0,-1,"video/next");bool received=false;consolemodel::Ack ack;pumpUntil([&]{while(f.host.takeAck(ack))if(ack.seq==7){received=true;check(ack.ok,"edit before navigation is not discarded");}return received;});auto navigated=awaitSnapshot(f,[](const consolemodel::Snapshot& s){return s.page=="video/next";});check(navigated&&navigated->generation!=metadata->generation&&find(*navigated,"toggle")->checked,"new page samples completed prior edits");
   if(navigated){auto inactive=beforeNavigation;inactive.generation=navigated->generation;inactive.seq=8;inactive.checked=false;check(f.host.post(inactive),"inactive-module edit admitted before switch");f.host.navigate(1,-1,"monitor");bool completed=false;pumpUntil([&]{while(f.host.takeAck(ack))if(ack.seq==8){completed=true;check(ack.ok,"inactive port completes already-admitted edit");}return completed;});check(completed&&!f.checked,"switching modules preserves prior accepted edit");}
  }
 }
 PostMessageW(f.pane,TryShow,0,0);WaitForSingleObject(f.mutated,4000);check(f.hidden,"backend pane cannot become visible");check(f.dialogRecognized,"backend-owned native modal belongs to console focus");
 f.host.navigate(1,-1,"monitor");check(!f.host.latestSnapshot(),"navigation never reuses previous module snapshot");
 PostMessageW(f.pane,WM_CLOSE,0,0);check(WaitForSingleObject(thread,4000)==WAIT_OBJECT_0,"owner exits");CloseHandle(thread);DestroyWindow(shell);
 std::printf("console adapter Win32 fixture: %d failures\n",failures);return failures?1:0;
}
