#!/usr/bin/env python3
"""Exercise the production Host against controlled Win32/COM scheduling.

This Linux fixture verifies host lifetime, origin gates and asynchronous
delivery. It does not execute WebView2 or certify Windows visual behavior.
Pass --zig/--sdk to additionally compile the same source against the real SDK.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "source/suite/source/web_console.cpp"

WINDOWS = r'''
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include <deque>
#include <locale>
#include <codecvt>
#include <typeinfo>
#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define CALLBACK
#define WINAPI
using HRESULT=int32_t; using ULONG=uint32_t; using DWORD=uint32_t; using UINT=unsigned;
using BOOL=int; using LONG_PTR=intptr_t; using LPARAM=intptr_t; using WPARAM=uintptr_t; using LRESULT=intptr_t;
using HINSTANCE=void*; using HMODULE=void*; using LPCWSTR=const wchar_t*; using PCWSTR=const wchar_t*; using LPWSTR=wchar_t*;
struct GUID {uint32_t Data1;uint16_t Data2,Data3;uint8_t Data4[8];};
using REFIID=const GUID&;
constexpr HRESULT S_OK=0,E_POINTER=-1,E_NOINTERFACE=-2,E_FAIL=-3;
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
constexpr BOOL TRUE=1,FALSE=0;
constexpr UINT WM_APP=0x8000, WM_NCCREATE=0x81,WM_NCDESTROY=0x82;
constexpr int GWLP_USERDATA=-21;
constexpr DWORD ERROR_CLASS_ALREADY_EXISTS=1410,ERROR_ALREADY_EXISTS=183,FILE_ATTRIBUTE_DIRECTORY=16;
constexpr DWORD INVALID_FILE_ATTRIBUTES=0xffffffff;
constexpr DWORD LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR=0x100,LOAD_LIBRARY_SEARCH_SYSTEM32=0x800;
constexpr unsigned CP_UTF8=65001,MB_ERR_INVALID_CHARS=8,WC_ERR_INVALID_CHARS=0x80;
struct RECT { long left=0,top=0,right=0,bottom=0; };
struct Window; using HWND=Window*;
#define HWND_MESSAGE reinterpret_cast<HWND>(intptr_t(-3))
using WindowProc=LRESULT (*)(HWND,UINT,WPARAM,LPARAM);
struct Window { WindowProc proc=nullptr; LONG_PTR user=0; bool valid=true; DWORD owner=1; };
struct CREATESTRUCTW { void* lpCreateParams; };
struct WNDCLASSW { WindowProc lpfnWndProc=nullptr; HINSTANCE hInstance=nullptr; const wchar_t* lpszClassName=nullptr; };
namespace fixture {
inline DWORD thread=1,lastError=0; inline WNDCLASSW registered;
inline std::wstring loaderPath,userDirectory; inline unsigned releases=0;
struct Posted { HWND window; UINT message; WPARAM a; LPARAM b; };
inline std::deque<Posted> posted;
inline bool inWebCallback=false;
inline void pump() { while(!posted.empty()){auto x=posted.front();posted.pop_front();if(x.window&&x.window->valid&&x.window->proc)x.window->proc(x.window,x.message,x.a,x.b);} }
}
inline bool IsEqualIID(REFIID a,REFIID b){return std::memcmp(&a,&b,sizeof(GUID))==0;}
inline DWORD GetCurrentThreadId(){return fixture::thread;}
inline BOOL IsWindow(HWND w){return w&&w!=HWND_MESSAGE&&w->valid;}
inline DWORD GetWindowThreadProcessId(HWND w,DWORD*){return w->owner;}
inline DWORD GetLastError(){return fixture::lastError;}
inline HMODULE GetModuleHandleW(const wchar_t*){return reinterpret_cast<void*>(1);}
inline DWORD RegisterClassW(const WNDCLASSW* c){fixture::registered=*c;return 1;}
inline LONG_PTR GetWindowLongPtrW(HWND w,int){return w->user;}
inline LONG_PTR SetWindowLongPtrW(HWND w,int,LONG_PTR n){auto p=w->user;w->user=n;return p;}
inline LRESULT DefWindowProcW(HWND,UINT m,WPARAM,LPARAM){return m==WM_NCCREATE?TRUE:0;}
inline HWND CreateWindowExW(DWORD,const wchar_t*,const wchar_t*,DWORD,int,int,int,int,HWND,void*,HINSTANCE,void* p){
 auto w=new Window;w->proc=fixture::registered.lpfnWndProc;CREATESTRUCTW s{p};w->proc(w,WM_NCCREATE,0,reinterpret_cast<LPARAM>(&s));return w;
}
inline BOOL DestroyWindow(HWND w){if(w&&w->valid){w->proc(w,WM_NCDESTROY,0,0);w->valid=false;}return TRUE;}
inline BOOL PostMessageW(HWND w,UINT m,WPARAM a,LPARAM b){if(!IsWindow(w))return FALSE;fixture::posted.push_back({w,m,a,b});return TRUE;}
inline BOOL GetClientRect(HWND,RECT* r){*r={0,0,1200,800};return TRUE;}
inline DWORD GetModuleFileNameW(void*,wchar_t* p,DWORD n){std::wstring s=L"C:\\Tools\\RA2Toolbox.exe";if(n<=s.size())return n;std::wcscpy(p,s.c_str());return DWORD(s.size());}
inline DWORD GetEnvironmentVariableW(const wchar_t*,wchar_t* p,DWORD n){std::wstring s=L"C:\\Users\\Test\\AppData\\Local";if(!p)return DWORD(s.size()+1);if(n<=s.size())return DWORD(s.size()+1);std::wcscpy(p,s.c_str());return DWORD(s.size());}
inline BOOL CreateDirectoryW(const wchar_t*,void*){return TRUE;}
inline DWORD GetFileAttributesW(const wchar_t*){return FILE_ATTRIBUTE_DIRECTORY;}
inline int lstrcmpW(const wchar_t* a,const wchar_t* b){return std::wcscmp(a,b);}
inline void CoTaskMemFree(void* p){std::free(p);}
inline LPWSTR duplicate(const std::wstring& s){auto p=static_cast<LPWSTR>(std::malloc((s.size()+1)*sizeof(wchar_t)));std::wcscpy(p,s.c_str());return p;}
inline int MultiByteToWideChar(unsigned,DWORD,const char* p,int n,wchar_t* out,int cap){try{std::wstring_convert<std::codecvt_utf8<wchar_t>> c;auto s=c.from_bytes(p,p+n);if(out){if(cap<int(s.size()))return 0;std::copy(s.begin(),s.end(),out);}return int(s.size());}catch(...){return 0;}}
inline int WideCharToMultiByte(unsigned,DWORD,const wchar_t* p,int n,char* out,int cap,void*,void*){try{std::wstring_convert<std::codecvt_utf8<wchar_t>> c;auto s=c.to_bytes(p,p+n);if(out){if(cap<int(s.size()))return 0;std::copy(s.begin(),s.end(),out);}return int(s.size());}catch(...){return 0;}}
inline HMODULE LoadLibraryExW(const wchar_t* p,void*,DWORD flags){fixture::loaderPath=p;if(flags!=(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32))return nullptr;return reinterpret_cast<void*>(2);}
inline BOOL FreeLibrary(HMODULE){++fixture::releases;return TRUE;}
using FARPROC=void(*)();
FARPROC GetProcAddress(HMODULE,const char*);
'''

SDK = r'''
#pragma once
#include "windows.h"
#include <cassert>
#include <functional>
struct IUnknown {virtual HRESULT QueryInterface(REFIID,void**){return E_NOINTERFACE;} virtual ULONG AddRef(){return 1;} virtual ULONG Release(){return 1;} virtual ~IUnknown()=default;};
struct ICoreWebView2;struct ICoreWebView2Environment;struct ICoreWebView2Controller;
struct ICoreWebView2EnvironmentOptions: IUnknown {};
struct EventRegistrationToken { int64_t value=0; };
enum COREWEBVIEW2_PERMISSION_STATE {COREWEBVIEW2_PERMISSION_STATE_DEFAULT,COREWEBVIEW2_PERMISSION_STATE_ALLOW,COREWEBVIEW2_PERMISSION_STATE_DENY};
struct ICoreWebView2NavigationStartingEventArgs: IUnknown {std::wstring uri=L"about:blank";BOOL cancel=FALSE;virtual HRESULT get_Uri(LPWSTR* p){*p=duplicate(uri);return S_OK;}virtual HRESULT put_Cancel(BOOL x){cancel=x;return S_OK;}};
struct ICoreWebView2NavigationCompletedEventArgs: IUnknown {BOOL success=TRUE;virtual HRESULT get_IsSuccess(BOOL* p){*p=success;return S_OK;}};
struct ICoreWebView2WebMessageReceivedEventArgs: IUnknown {std::wstring uri=L"about:blank",json=L"{}";virtual HRESULT get_Source(LPWSTR* p){*p=duplicate(uri);return S_OK;}virtual HRESULT get_WebMessageAsJson(LPWSTR* p){*p=duplicate(json);return S_OK;}};
struct ICoreWebView2NewWindowRequestedEventArgs: IUnknown {BOOL handled=FALSE;virtual HRESULT put_Handled(BOOL x){handled=x;return S_OK;}};
struct ICoreWebView2PermissionRequestedEventArgs: IUnknown {COREWEBVIEW2_PERMISSION_STATE state=COREWEBVIEW2_PERMISSION_STATE_DEFAULT;virtual HRESULT put_State(COREWEBVIEW2_PERMISSION_STATE x){state=x;return S_OK;}};
struct ICoreWebView2ProcessFailedEventArgs: IUnknown {};
struct ICoreWebView2DownloadStartingEventArgs: IUnknown {BOOL cancel=FALSE,handled=FALSE;virtual HRESULT put_Cancel(BOOL x){cancel=x;return S_OK;}virtual HRESULT put_Handled(BOOL x){handled=x;return S_OK;}};
#define EVENT_HANDLER(Name,Arg) struct Name: IUnknown {virtual HRESULT Invoke(ICoreWebView2*,Arg*)=0;};
EVENT_HANDLER(ICoreWebView2NavigationStartingEventHandler,ICoreWebView2NavigationStartingEventArgs)
EVENT_HANDLER(ICoreWebView2NavigationCompletedEventHandler,ICoreWebView2NavigationCompletedEventArgs)
EVENT_HANDLER(ICoreWebView2WebMessageReceivedEventHandler,ICoreWebView2WebMessageReceivedEventArgs)
EVENT_HANDLER(ICoreWebView2NewWindowRequestedEventHandler,ICoreWebView2NewWindowRequestedEventArgs)
EVENT_HANDLER(ICoreWebView2PermissionRequestedEventHandler,ICoreWebView2PermissionRequestedEventArgs)
EVENT_HANDLER(ICoreWebView2ProcessFailedEventHandler,ICoreWebView2ProcessFailedEventArgs)
EVENT_HANDLER(ICoreWebView2DownloadStartingEventHandler,ICoreWebView2DownloadStartingEventArgs)
struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler: IUnknown {virtual HRESULT Invoke(HRESULT,ICoreWebView2Environment*)=0;};
struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler: IUnknown {virtual HRESULT Invoke(HRESULT,ICoreWebView2Controller*)=0;};
struct ICoreWebView2Settings: IUnknown {
 std::map<std::string,BOOL> values;
 HRESULT QueryInterface(REFIID,void**)override;
 #define SETTING(Name) virtual HRESULT put_##Name(BOOL x){values[#Name]=x;return S_OK;}
 SETTING(IsScriptEnabled) SETTING(IsWebMessageEnabled) SETTING(AreHostObjectsAllowed) SETTING(AreDefaultScriptDialogsEnabled)
 SETTING(AreDefaultContextMenusEnabled) SETTING(AreDevToolsEnabled) SETTING(IsStatusBarEnabled) SETTING(IsZoomControlEnabled)
 #undef SETTING
};
struct ICoreWebView2Settings3: ICoreWebView2Settings {
 virtual HRESULT put_AreBrowserAcceleratorKeysEnabled(BOOL x){values["AreBrowserAcceleratorKeysEnabled"]=x;return S_OK;}
};
inline HRESULT ICoreWebView2Settings::QueryInterface(REFIID iid,void** p){constexpr GUID id{0xfdb5ab74,0xaf33,0x4854,{0x84,0xf0,0x0a,0x63,0x1d,0xeb,0x5e,0xba}};if(!IsEqualIID(iid,id))return E_NOINTERFACE;*p=static_cast<ICoreWebView2Settings3*>(this);return S_OK;}
struct ICoreWebView2: IUnknown {
 ICoreWebView2Settings3 settings;std::wstring source=L"about:blank",posted,html;
 unsigned removeCount=0;
 virtual HRESULT get_Settings(ICoreWebView2Settings** p){*p=&settings;return S_OK;}
 virtual HRESULT get_Source(LPWSTR* p){*p=duplicate(source);return S_OK;}
 virtual HRESULT PostWebMessageAsJson(LPCWSTR p){posted=p;return S_OK;}
 virtual HRESULT NavigateToString(LPCWSTR p){html=p;return S_OK;}
 #define EVENT(Name,Type) Type* Name=nullptr; virtual HRESULT add_##Name(Type* p,EventRegistrationToken*){Name=p;p->AddRef();return S_OK;} virtual HRESULT remove_##Name(EventRegistrationToken){if(Name){Name->Release();Name=nullptr;++removeCount;}return S_OK;}
 EVENT(NavigationStarting,ICoreWebView2NavigationStartingEventHandler)
 EVENT(FrameNavigationStarting,ICoreWebView2NavigationStartingEventHandler)
 EVENT(NavigationCompleted,ICoreWebView2NavigationCompletedEventHandler)
 EVENT(WebMessageReceived,ICoreWebView2WebMessageReceivedEventHandler)
 EVENT(NewWindowRequested,ICoreWebView2NewWindowRequestedEventHandler)
 EVENT(PermissionRequested,ICoreWebView2PermissionRequestedEventHandler)
 EVENT(ProcessFailed,ICoreWebView2ProcessFailedEventHandler)
};
struct ICoreWebView2_4: ICoreWebView2 {
 EVENT(DownloadStarting,ICoreWebView2DownloadStartingEventHandler)
 HRESULT QueryInterface(REFIID iid,void** p)override{constexpr GUID id{0x20d02d59,0x6df2,0x42dc,{0xbd,0x06,0xf9,0x8a,0x69,0x4b,0x13,0x02}};if(!IsEqualIID(iid,id))return E_NOINTERFACE;*p=this;return S_OK;}
};
#undef EVENT
struct ICoreWebView2Controller: IUnknown {
 ICoreWebView2_4 view;unsigned boundsCalls=0,closeCalls=0;BOOL visible=FALSE;RECT bounds{};
 virtual HRESULT get_CoreWebView2(ICoreWebView2** p){*p=&view;return S_OK;}
 virtual HRESULT put_Bounds(RECT x){++boundsCalls;bounds=x;return S_OK;}
 virtual HRESULT put_IsVisible(BOOL x){visible=x;return S_OK;}
 virtual HRESULT Close(){++closeCalls;return S_OK;}
};
namespace fixture {inline ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentPending=nullptr;inline ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* controllerPending=nullptr;inline unsigned controllerStarts=0;}
struct ICoreWebView2Environment: IUnknown {
 virtual HRESULT CreateCoreWebView2Controller(HWND,ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* p){++fixture::controllerStarts;assert(!fixture::controllerPending);fixture::controllerPending=p;p->AddRef();return S_OK;}
};
namespace fixture {
inline HRESULT createEnvironment(PCWSTR,PCWSTR path,ICoreWebView2EnvironmentOptions*,ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* p){assert(!environmentPending);userDirectory=path;environmentPending=p;p->AddRef();return S_OK;}
inline HRESULT availableVersion(PCWSTR,LPWSTR* p){*p=duplicate(L"150.0.0.0");return S_OK;}
inline void completeEnvironment(ICoreWebView2Environment& e){auto* p=environmentPending;environmentPending=nullptr;assert(p);p->Invoke(S_OK,&e);p->Release();}
inline void completeController(ICoreWebView2Controller& c){auto* p=controllerPending;controllerPending=nullptr;assert(p);p->Invoke(S_OK,&c);p->Release();}
template<class Handler,class Args> void event(Handler* h,ICoreWebView2* view,Args& args){assert(h);inWebCallback=true;h->Invoke(view,&args);inWebCallback=false;}
}
inline FARPROC GetProcAddress(HMODULE,const char* name){if(std::strcmp(name,"CreateCoreWebView2EnvironmentWithOptions")==0)return reinterpret_cast<FARPROC>(fixture::createEnvironment);if(std::strcmp(name,"GetAvailableCoreWebView2BrowserVersionString")==0)return reinterpret_cast<FARPROC>(fixture::availableVersion);return nullptr;}
'''

HARNESS = r'''
#include "WebView2.h"
#include "web_console.h"
#include <cassert>
#include <iostream>

int main(){
 Window parent; ICoreWebView2Environment environment;
 unsigned messages=0,errors=0;
 auto receive=[&](const std::string& message){assert(!fixture::inWebCallback);assert(message=="{\"kind\":\"ready\"}");++messages;};
 auto error=[&](const std::wstring&){assert(!fixture::inWebCallback);++errors;};
 consoleweb::Host host;
 assert(host.create(&parent,receive,error));assert(!host.ready());assert(!host.post("{}"));
 assert(fixture::loaderPath==L"C:\\Tools\\WebView2Loader.dll");
 assert(fixture::userDirectory==L"C:\\Users\\Test\\AppData\\Local\\RA2Toolbox\\WebView2");
 fixture::completeEnvironment(environment);
 ICoreWebView2Controller controller;
 fixture::completeController(controller);
 auto& view=controller.view;
 assert(!host.ready());assert(!view.html.empty());assert(controller.visible);
 assert(!view.settings.values["AreHostObjectsAllowed"]&&!view.settings.values["AreDefaultScriptDialogsEnabled"]&&!view.settings.values["AreDevToolsEnabled"]);
 assert(view.settings.values.count("AreBrowserAcceleratorKeysEnabled")&&!view.settings.values["AreBrowserAcceleratorKeysEnabled"]);
 ICoreWebView2WebMessageReceivedEventArgs early;early.json=L"{\"kind\":\"ready\"}";
 fixture::event(view.WebMessageReceived,&view,early);fixture::pump();assert(messages==0);
 ICoreWebView2NavigationCompletedEventArgs completed;
 fixture::event(view.NavigationCompleted,&view,completed);assert(host.ready());assert(messages==0);
 fixture::pump();assert(messages==1);
 assert(host.post("{\"hello\":\"世界\"}"));assert(view.posted==L"{\"hello\":\"世界\"}");
 assert(!host.post(std::string(2*1024*1024+1,'x')));assert(!host.post(std::string(1,char(0xff))));
 fixture::thread=2;auto calls=controller.boundsCalls;host.resize({0,0,8,8});assert(controller.boundsCalls==calls);assert(!host.ready());assert(!host.post("{}"));host.close();assert(!controller.closeCalls);fixture::thread=1;assert(host.ready());
 ICoreWebView2NavigationStartingEventArgs navigation;navigation.uri=L"https://example.com/";
 fixture::event(view.NavigationStarting,&view,navigation);assert(navigation.cancel);
 ICoreWebView2NewWindowRequestedEventArgs window;fixture::event(view.NewWindowRequested,&view,window);assert(window.handled);
 ICoreWebView2PermissionRequestedEventArgs permission;fixture::event(view.PermissionRequested,&view,permission);assert(permission.state==COREWEBVIEW2_PERMISSION_STATE_DENY);
 ICoreWebView2DownloadStartingEventArgs download;fixture::event(view.DownloadStarting,&view,download);assert(download.cancel&&download.handled);
 ICoreWebView2WebMessageReceivedEventArgs foreign;foreign.uri=L"https://example.com/";fixture::event(view.WebMessageReceived,&view,foreign);
 ICoreWebView2WebMessageReceivedEventArgs oversized;oversized.json=std::wstring(65537,L'a');fixture::event(view.WebMessageReceived,&view,oversized);fixture::pump();assert(messages==1);
 ICoreWebView2ProcessFailedEventArgs process;fixture::event(view.ProcessFailed,&view,process);assert(!host.ready());assert(errors==0);fixture::pump();assert(errors==1);
 host.close();assert(controller.closeCalls==1);assert(view.removeCount==8);assert(!host.ready());
 // A queued controller completion after close must never touch its HWND.
 assert(host.create(&parent,receive,error));fixture::completeEnvironment(environment);host.close();ICoreWebView2Controller late;fixture::completeController(late);assert(late.boundsCalls==0&&!late.visible);fixture::pump();
 // Destruction before environment completion must not start a controller.
 const auto starts=fixture::controllerStarts;
 {consoleweb::Host temporary;assert(temporary.create(&parent,receive,error));}
 fixture::completeEnvironment(environment);assert(fixture::controllerStarts==starts);fixture::pump();
 // A shell callback may destroy its Host while a delivery is being drained.
 consoleweb::Host selfClosing;
 assert(selfClosing.create(&parent,[&](const std::string&){selfClosing.close();},error));
 fixture::completeEnvironment(environment);ICoreWebView2Controller last;fixture::completeController(last);
 fixture::event(last.view.NavigationCompleted,&last.view,completed);fixture::event(last.view.WebMessageReceived,&last.view,early);fixture::pump();assert(last.closeCalls==1);
 std::cout<<"Web console async lifetime/origin/bounds contract passed\n";
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zig")
    parser.add_argument("--sdk", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ra2-web-console-") as temporary:
        directory = Path(temporary)
        (directory / "windows.h").write_text(WINDOWS)
        (directory / "WebView2.h").write_text(SDK)
        (directory / "web_console_assets.h").write_text(
            '#pragma once\n#include <cstddef>\nnamespace consoleassets { inline constexpr char Html[]="<!doctype html><title>console</title>"; inline constexpr std::size_t HtmlSize=sizeof(Html)-1; }\n'
        )
        (directory / "harness.cpp").write_text(HARNESS)
        binary = directory / "contract"
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(directory), "-I", str(SOURCE.parent),
            str(SOURCE), str(directory / "harness.cpp"), "-o", str(binary)
        ], check=True)
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        # Fake windows deliberately outlive their queued messages; no process,
        # browser or game object is created by this controlled fixture.
        subprocess.run([str(binary)], env=environment, check=True)
        if args.zig or args.sdk:
            if not args.zig or not args.sdk:
                parser.error("--zig and --sdk must be provided together")
            sdk_text = (args.sdk / "include/WebView2.h").read_text()
            identities = re.findall(r"CONSOLE_COM_ID\((\w+),\s*(0x[^)]+)\)", SOURCE.read_text())
            assert len(identities) == 12
            for name, values in identities:
                numbers = [int(value.strip(), 16) for value in values.split(",")]
                assert len(numbers) == 11
                actual = f"{numbers[0]:08x}-{numbers[1]:04x}-{numbers[2]:04x}-{numbers[3]:02x}{numbers[4]:02x}-" + "".join(f"{value:02x}" for value in numbers[5:])
                if name == "IUnknown":
                    expected = "00000000-0000-0000-c000-000000000046"
                else:
                    match = re.search(r'MIDL_INTERFACE\("([0-9A-Fa-f-]+)"\)\s*' + re.escape(name) + r'\s*:', sdk_text)
                    assert match, name
                    expected = match.group(1).lower()
                assert actual == expected, (name, actual, expected)
            print("All 12 explicit COM interface identities match SDK/COM definitions.")
            assets = directory / "assets"
            assets.mkdir()
            (assets / "web_console_assets.h").write_text((directory / "web_console_assets.h").read_text())
            entry = directory / "entry.cpp"
            entry.write_text('#include "web_console.h"\nint WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){consoleweb::Host host;return host.create(GetDesktopWindow(),[](const std::string&){},[](const std::wstring&) {})?0:1;}\n')
            subprocess.run([
                args.zig, "c++", "-target", "x86_64-windows-gnu", "-std=c++17", "-O2", "-fms-extensions",
                "-I", str(args.sdk / "include"), "-I", str(assets), "-I", str(SOURCE.parent),
                "-municode", str(SOURCE), str(entry), "-Wl,--subsystem,windows", "-luser32", "-lole32", "-lkernel32",
                "-o", str(directory / "web_console_link.exe")
            ], check=True)
            print("Production Host compiled and linked against Windows/WebView2 SDK (not executed).")


if __name__ == "__main__":
    main()
