#!/usr/bin/env python3
"""Run the production status reader and capture body with portable HWND data.

This verifies snapshot routing, not Win32 calls, rendering, or game attachment.
The real HWND integration cases live in console_adapter_windows.cpp.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def block(text, marker):
    start = text.index(marker)
    at = text.index('{', start)
    depth, end = 1, at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True)
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
here = Path(__file__).resolve().parent
adapter = here.parent / 'source/console_adapter.h'
model = here.parent / 'source/console_model.h'
out = Path(args.out).resolve()
out.mkdir(parents=True, exist_ok=True)
source = adapter.read_text(encoding='utf-8-sig')
status = block(source, 'inline bool status(HWND w,std::string& value)')
capture = block(source, 'void capture(Port& p,const Navigation& nav,uint64_t now)')
fixture = r'''
#ifdef NDEBUG
#error "status regression assertions must be enabled"
#endif
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <locale>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include MODEL_HEADER
struct Window {bool live=true,visible=true,skip=false,status=false;const std::wstring* source=nullptr;std::wstring text;};
using HWND=Window*;using HANDLE=void*;using LPARAM=intptr_t;using DWORD=unsigned;
struct RECT {long left=0,top=0,right=100,bottom=26;};
static DWORD GetCurrentThreadId(){return 7;}
static bool IsWindow(HWND w){return w&&w->live;}
static int GetDlgCtrlID(HWND){return 0;}
static HANDLE GetPropW(HWND w,const wchar_t* name){
 if(!std::wcscmp(name,L"RA2.Console.Status"))return w->status?reinterpret_cast<HANDLE>(1):nullptr;
 if(!std::wcscmp(name,L"RA2.Console.StatusText"))return const_cast<std::wstring*>(w->source);
 if(!std::wcscmp(name,L"RA2.Console.Skip"))return w->skip?reinterpret_cast<HANDLE>(1):nullptr;
 return nullptr;
}
static void EnumChildWindows(HWND,bool(*)(HWND,LPARAM),LPARAM){}
namespace detail {
static std::string utf8(const std::wstring& s){return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(s);}
static std::wstring text(HWND w){return w->text;}
static bool visibleStyle(HWND w,HWND){return w->visible;}
STATUS_BODY
}
struct Entry {uint32_t token=0;HWND window=nullptr;std::string type="toggle",optionIdentity;bool published=false;};
struct Port {
 std::atomic<HWND> window{nullptr};DWORD owner=7;unsigned module=2;bool capturing=false,dirty=true;
 uint64_t lastCapture=0,lastPeriodic=0,generation=0,revision=0;std::string lastContent;
 std::unordered_map<HWND,Entry> entries;std::mutex submissionMutex;
};
struct Navigation {std::string page="tactical/1";int category=1;};
struct NativeControl {HWND window=nullptr;RECT rect{};consolemodel::Control control;std::string optionIdentity;};
struct Host {
 std::atomic<uint32_t> nextToken_{100};std::atomic<uint64_t> nextGeneration_{1};
 std::vector<consolemodel::Snapshot> published;unsigned reads=0;
 static bool enumerate(HWND,LPARAM){return true;}
 NativeControl read(Port&,Entry& e){++reads;NativeControl n;n.window=e.window;n.control.id=e.token;n.control.type=e.type;n.control.visible=e.window->visible;n.control.text=n.control.label=detail::utf8(e.window->text);return n;}
 void publish(Port&,const Navigation&,consolemodel::Snapshot s){published.push_back(std::move(s));}
 CAPTURE_BODY
};
static unsigned checks=0;
static void check(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
 Window pane,setting,status,unrelated;setting.text=L"自动维修";
 std::wstring actual=L"正在寻找游戏…";status.status=true;status.visible=false;status.source=&actual;status.text=L"悬浮说明";
 unrelated.visible=false;unrelated.text=L"不相关的隐藏说明";
 Port p;p.window=&pane;p.entries.emplace(&setting,Entry{1,&setting});p.entries.emplace(&status,Entry{2,&status});p.entries.emplace(&unrelated,Entry{3,&unrelated});
 Host h;Navigation nav;h.capture(p,nav,100);
 check(h.published.size()==1,"first snapshot published");auto first=h.published.back();
 check(first.status==u8"正在寻找游戏…","hidden label publishes authoritative status");
 check(first.controls.size()==1&&first.controls.front().id==1,"status and hidden labels are not settings rows");
 check(h.reads==1,"hidden status is consumed before visible-control reads");
 check(!p.capturing&&p.lastPeriodic==100,"normal capture completion retained");
 actual=L"只读连接失败 (Win32=5)";h.capture(p,nav,300);
 check(h.published.size()==2,"source-only status update publishes without native label update");
 check(h.published.back().status==u8"只读连接失败 (Win32=5)","connection failure stays visible");
 check(h.published.back().revision>first.revision&&h.published.back().generation==first.generation,"status update keeps setting lifetime");
 check(h.published.back().controls.front().id==first.controls.front().id,"status update preserves setting token");
 status.text=L"新的悬浮说明";h.capture(p,nav,500);
 check(h.published.size()==2,"hover text cannot replace actual status");
 actual.clear();h.capture(p,nav,700);
 check(h.published.size()==3&&h.published.back().status.empty(),"empty authoritative source clears stale failure");
 status.source=nullptr;status.text=L"已暂停。";h.capture(p,nav,900);
 check(h.published.back().status==u8"已暂停。","marked label without dedicated source uses native text");
 status.skip=true;status.visible=true;status.text=L"已连接只读 PID 1234";h.capture(p,nav,1100);
 check(h.published.back().status==u8"已连接只读 PID 1234","Skip metadata cannot suppress status");
 check(h.published.back().controls.size()==1,"visible status is not duplicated as a setting");
 nav.page="tactical/2";nav.category=2;h.capture(p,nav,1300);
 check(h.published.back().page=="tactical/2"&&h.published.back().status==u8"已连接只读 PID 1234","status survives category navigation");
 status.live=false;h.capture(p,nav,1500);
 check(h.published.back().status.empty(),"destroyed source cannot retain previous error");
 status.live=true;status.visible=false;status.status=false;h.capture(p,nav,1700);
 check(h.published.back().status.empty(),"unmarked hidden labels are not status sources");
 status.status=true;status.text=L"错误 \"gameares.exe\"\nWin32=5";h.capture(p,nav,1900);
 auto encoded=consolemodel::encodeSnapshot(h.published.back());
 check(encoded.find("\\\"gameares.exe\\\"\\nWin32=5")!=std::string::npos,"status uses protocol JSON escaping");
 std::printf("{\"checks\":%u,\"assertions_enabled\":true}\n",checks);
}
'''.replace('MODEL_HEADER', json.dumps(str(model))).replace('STATUS_BODY', status).replace('CAPTURE_BODY', capture)
generated = out / 'console-status-production.cpp'
generated.write_text(fixture, encoding='utf-8')
binary = out / 'console-status-production'
command = [args.cxx, '-std=c++17', '-O1', '-UNDEBUG', str(generated), '-o', str(binary)]
built = subprocess.run(command, capture_output=True, text=True)
(out / 'build.log').write_text(built.stdout + built.stderr, encoding='utf-8')
if built.returncode:
    raise SystemExit(built.stdout + built.stderr)
ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
(out / 'run.log').write_text(ran.stdout + ran.stderr, encoding='utf-8')
if ran.returncode:
    raise SystemExit(ran.stdout + ran.stderr)
receipt = {
    'schema': 'ra2.console-status-regression.v1',
    'source_sha256': {str(path.relative_to(here.parents[1])): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in (adapter, model, Path(__file__))},
    'production_bodies_executed': ['consoleadapter::detail::status', 'consoleadapter::Host::capture'],
    'result': json.loads(ran.stdout),
    'win32_calls': 'portable fixture data only',
    'windows_ui_executed': False,
    'game_process_accessed': False,
    'exit_code': ran.returncode,
}
(out / 'result.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(receipt, ensure_ascii=False, indent=2))
