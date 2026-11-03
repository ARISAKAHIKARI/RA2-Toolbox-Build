#ifdef NDEBUG
#error Navigation smoke checks must be enabled
#endif
#define UNICODE
#define _UNICODE
#define RA2_CONSOLE_TESTING
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cwchar>
static std::vector<std::string> traces;
static std::string utf8(const wchar_t*s){if(!s)return {};int n=WideCharToMultiByte(CP_UTF8,0,s,-1,nullptr,0,nullptr,nullptr);if(n<1)return {};std::string out(size_t(n),0);WideCharToMultiByte(CP_UTF8,0,s,-1,out.data(),n,nullptr,nullptr);out.pop_back();return out;}
static void trace(const wchar_t*s){traces.push_back(utf8(s));std::printf("%s",traces.back().c_str());std::fflush(stdout);}
#define RA2_CONSOLE_NAVIGATION_TRACE trace
#include "../source/web_console.cpp"
namespace consoleweb {struct NavigationProbe{static ICoreWebView2*view(Host&host){return host.state_?host.state_->view:nullptr;}};}
static std::string quote(const std::string&s){std::string out="\"";for(unsigned char c:s){if(c=='\\'||c=='"'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c<32)out+=' ';else out+=char(c);}return out+'"';}

namespace consoleweb {namespace {
template<> struct InterfaceIdentity<ICoreWebView2ExecuteScriptCompletedHandler>{inline static constexpr GUID value{0x49511172,0xcc67,0x4bca,{0x99,0x23,0x13,0x71,0x12,0xf4,0xc4,0xcc}};};
template<> struct InterfaceIdentity<ICoreWebView2CapturePreviewCompletedHandler>{inline static constexpr GUID value{0x697e05e9,0x3d8f,0x45fa,{0x96,0xf4,0x8f,0xfe,0x1e,0xde,0xda,0xf5}};};
}}
static bool pump(const std::function<bool()>&done,DWORD timeout=10000){const DWORD started=GetTickCount();while(DWORD(GetTickCount()-started)<timeout&&!done()){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}return done();}
struct Result{bool done=false;HRESULT hr=E_PENDING;std::wstring json;};
static std::shared_ptr<Result>script(ICoreWebView2*view,const wchar_t*text){auto out=std::make_shared<Result>();auto*cb=new consoleweb::Callback<ICoreWebView2ExecuteScriptCompletedHandler,HRESULT,LPCWSTR>([out](HRESULT hr,LPCWSTR json){out->hr=hr;out->json=json?json:L"";out->done=true;return S_OK;});out->hr=view->ExecuteScript(text,cb);cb->Release();pump([&]{return out->done;});return out;}
int wmain(int argc,wchar_t**argv){
 const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(initialized)){std::printf("{\"status\":\"COM_FAILED\",\"hr\":%ld}\n",long(initialized));return 2;}
 HWND window=CreateWindowExW(0,L"STATIC",L"RA2 isolated WebView2 navigation fixture",WS_OVERLAPPEDWINDOW,0,0,1240,880,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 consoleweb::Host host;bool frontendReady=false,actionReceived=false;unsigned messages=0;std::wstring error;
 const bool admitted=host.create(window,[&](const std::string&json){++messages;std::printf("WEB_MESSAGE %s\n",json.c_str());std::fflush(stdout);if(json.find("\"ready\"")!=std::string::npos)frontendReady=true;if(json.find("\"action\"")!=std::string::npos)actionReceived=true;},[&](const std::wstring&text){error=text;std::printf("HOST_ERROR %s\n",utf8(text.c_str()).c_str());std::fflush(stdout);});
 pump([&]{return !error.empty()||frontendReady;},30000);const bool ready=host.ready();auto*view=consoleweb::NavigationProbe::view(host);bool shellPosted=false,statePosted=false,domReady=false,captured=false,deniedRetains=false;std::wstring domJson;HRESULT captureHr=E_PENDING;
 if(error.empty()&&ready&&frontendReady&&view){
  shellPosted=host.post(R"({"kind":"shell","protocol":1,"version":"controlled WebView2 smoke","selectedCategory":0,"videoSection":0,"servicesReady":true,"modules":[{"module":0,"ready":true,"enabled":true}],"config":{"pending":0,"busy":false,"failed":false}})");
  statePosted=host.post(R"({"kind":"state","protocol":1,"module":0,"generation":1,"revision":1,"page":"cursor","title":"Controlled navigation smoke","status":"No game process","controls":[{"id":101,"type":"toggle","label":"Smoke switch","checked":false,"group":"Controlled fixture"},{"id":102,"type":"label","label":"Actual WebView2 navigation and native message bridge","group":"Controlled fixture"}]})");
  pump([]{return false;},250);
  auto dom=script(view,L"({title:document.title,app:!!window.RA2Console,controls:document.querySelectorAll('.setting-row').length,switch:!!document.querySelector('.setting-row input[role=switch]'),text:document.body.innerText.includes('Smoke switch')})");domJson=dom->json;
  domReady=dom->done&&SUCCEEDED(dom->hr)&&domJson.find(L"\"controls\":2")!=std::wstring::npos&&domJson.find(L"\"switch\":true")!=std::wstring::npos&&domJson.find(L"\"text\":true")!=std::wstring::npos;
  auto click=script(view,L"document.querySelector('.setting-row input[role=switch]').click();true");pump([&]{return actionReceived;});
  // New navigation is denied even when it is about:blank or repeats the
  // original exact data document. Cancellation completion is non-fatal.
  view->Navigate(L"about:blank");pump([]{return false;},150);view->NavigateToString(consoleweb::widen(consoleassets::Html,consoleassets::HtmlSize).c_str());pump([]{return false;},150);view->Navigate(L"https://example.invalid/");pump([]{return false;},150);
  auto after=script(view,L"({controls:document.querySelectorAll('.setting-row').length,app:!!window.RA2Console})");deniedRetains=host.ready()&&error.empty()&&after->done&&SUCCEEDED(after->hr)&&after->json.find(L"\"controls\":2")!=std::wstring::npos;
  if(argc>1&&domReady){IStream*stream=nullptr;if(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))){auto result=std::make_shared<Result>();auto*cb=new consoleweb::Callback<ICoreWebView2CapturePreviewCompletedHandler,HRESULT>([result](HRESULT hr){result->hr=hr;result->done=true;return S_OK;});captureHr=view->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream,cb);cb->Release();if(SUCCEEDED(captureHr)&&pump([&]{return result->done;})){captureHr=result->hr;if(SUCCEEDED(captureHr)){STATSTG stat{};HGLOBAL memory=nullptr;if(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME))&&SUCCEEDED(GetHGlobalFromStream(stream,&memory))&&stat.cbSize.QuadPart>8&&stat.cbSize.QuadPart<100000000){auto*bits=GlobalLock(memory);HANDLE file=CreateFileW(argv[1],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;if(bits&&file!=INVALID_HANDLE_VALUE)captured=WriteFile(file,bits,DWORD(stat.cbSize.QuadPart),&written,nullptr)&&written==stat.cbSize.QuadPart;if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(bits)GlobalUnlock(memory);}}}stream->Release();}}
 }
 const bool okay=error.empty()&&ready&&frontendReady&&shellPosted&&statePosted&&domReady&&actionReceived&&deniedRetains&&(argc<2||captured);
 std::printf("DOM_RESULT %s\n",utf8(domJson.c_str()).c_str());
 std::printf("{\"status\":%s,\"create_admitted\":%s,\"navigation_ready\":%s,\"frontend_ready\":%s,\"snapshot_posted\":%s,\"dom_ready\":%s,\"action_received\":%s,\"denied_navigation_keeps_document\":%s,\"capture_preview\":%s,\"capture_hr\":%ld,\"messages\":%u,\"error\":%s,\"game_process_accessed\":false}\n",okay?"\"PASS\"":"\"FAIL\"",admitted?"true":"false",ready?"true":"false",frontendReady?"true":"false",shellPosted&&statePosted?"true":"false",domReady?"true":"false",actionReceived?"true":"false",deniedRetains?"true":"false",captured?"true":"false",long(captureHr),messages,quote(utf8(error.c_str())).c_str());
 host.close();DestroyWindow(window);CoUninitialize();return okay?0:1;
}
