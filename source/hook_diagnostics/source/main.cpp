#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdio>
#include "../../shared/source/host_game_input.h"
#include "../../assist/source/protocol.h"
#include "../../shared/source/hook_diagnostic_gate.h"
#include "hook_test_bundle.h"
#include "sequence.h"
#include "catalog.h"
#include "selection.h"
#include "observation_gate.h"

extern "C" __declspec(dllexport) const uint32_t AssistHostProtocolInfo[2]={assist::Version,sizeof(assist::Shared)};
extern "C" __declspec(dllexport) const assist::MappingPrefixValue AssistHostMappingPrefix{};
namespace {
using hooktest::Phase;using hooktest::Kind;
HWND window,pidEdit,stepChoice,statusBox,logBox,skipList,followingBox,baselineBox;HFONT font;HANDLE gameProcess=nullptr,mapping=nullptr,logFile=INVALID_HANDLE_VALUE;
HANDLE diagnosticMapping=nullptr;hookdiagnostic::Channel*diagnostic=nullptr;
DWORD pid=0,threadId=0;HWND gameWindow=nullptr;assist::Shared*shared=nullptr;gameinput::Subscription subscription=0;
hooktest::Sequence sequence;hooktest::Selection selection;hooktest::ObservationGate observation;assist::Snapshot snapshot{};
std::vector<uint32_t>sites=hooktest::allSites();std::map<uint32_t,std::vector<uint8_t>>baseline;
std::vector<unsigned>schedule;std::wstring logText,status,folder,stopReason;bool configured=false,techRequested=false,closeWanted=false,smoke=false,restorationLogged=false;
uint64_t attemptAt=0,lastLogAt=0,lastTimerAt=0,lastFrameMoveAt=0;uint32_t initialHouse=0,initialScenario=0;int latestFrame=-1;unsigned lastFault=0;
struct Module {uintptr_t base=0;uint32_t size=0;std::wstring name,path;};std::vector<Module>modules;
std::wstring hex(uint64_t v){wchar_t b[32];swprintf(b,32,L"0x%llX",(unsigned long long)v);return b;}
std::wstring bytes(const uint8_t*p,size_t n){const wchar_t*d=L"0123456789ABCDEF";std::wstring s;for(size_t i=0;i<n;++i){if(i)s+=L' ';s+=d[p[i]>>4];s+=d[p[i]&15];}return s;}
std::string utf8(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string out(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;}
void log(const std::wstring&s){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[64];swprintf(b,64,L"%04u-%02u-%02u %02u:%02u:%02u.%03u ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);std::wstring line=b+s+L"\r\n";
 if(logFile!=INVALID_HANDLE_VALUE){auto data=utf8(line);DWORD written=0;WriteFile(logFile,data.data(),DWORD(data.size()),&written,nullptr);FlushFileBuffers(logFile);}logText+=line;if(logText.size()>100000)logText.erase(0,30000);if(logBox){SetWindowTextW(logBox,logText.c_str());SendMessageW(logBox,EM_SETSEL,logText.size(),logText.size());SendMessageW(logBox,EM_SCROLLCARET,0,0);}}
void show(const std::wstring&s){if(status==s)return;status=s;if(statusBox)SetWindowTextW(statusBox,s.c_str());}
std::wstring selectionFile(){return folder+L"\\hook-test.ini";}
bool saveSelection(){
 if(smoke)return true;bool ok=true;auto file=selectionFile();
 for(unsigned i=2;i<hooktest::steps().size();++i)if(!WritePrivateProfileStringW(L"Skip",hooktest::selectionKey(hooktest::steps()[i].kind),selection.excluded[i]?L"1":L"0",file.c_str()))ok=false;
 int choice=int(SendMessageW(stepChoice,CB_GETCURSEL,0,0));const wchar_t*group=choice>0&&choice<int(hooktest::steps().size()-1)?hooktest::selectionKey(hooktest::steps()[choice+1].kind):L"All";
 if(!WritePrivateProfileStringW(L"Selection",L"Start",group,file.c_str()))ok=false;
 if(!WritePrivateProfileStringW(L"Selection",L"Following",SendMessageW(followingBox,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",file.c_str()))ok=false;
 if(!WritePrivateProfileStringW(L"Selection",L"Baselines",SendMessageW(baselineBox,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",file.c_str()))ok=false;
 return ok;
}
void loadSelection(){
 auto file=selectionFile();
 for(unsigned i=2;i<hooktest::steps().size();++i){
  if(!smoke)selection.excluded[i]=GetPrivateProfileIntW(L"Skip",hooktest::selectionKey(hooktest::steps()[i].kind),selection.excluded[i]?1:0,file.c_str())!=0;
  SendMessageW(skipList,LB_SETSEL,selection.excluded[i],i-2);
 }
 if(!smoke){wchar_t group[64];GetPrivateProfileStringW(L"Selection",L"Start",L"All",group,64,file.c_str());
  for(unsigned i=2;i<hooktest::steps().size();++i)if(!wcscmp(group,hooktest::selectionKey(hooktest::steps()[i].kind)))SendMessageW(stepChoice,CB_SETCURSEL,i-1,0);
  SendMessageW(followingBox,BM_SETCHECK,GetPrivateProfileIntW(L"Selection",L"Following",1,file.c_str())?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(baselineBox,BM_SETCHECK,GetPrivateProfileIntW(L"Selection",L"Baselines",0,file.c_str())?BST_CHECKED:BST_UNCHECKED,0);
 }
}
void selectionChanged(){
 for(unsigned i=2;i<hooktest::steps().size();++i)selection.excluded[i]=SendMessageW(skipList,LB_GETSEL,i-2,0)>0;
 unsigned count=0;for(unsigned i=2;i<hooktest::steps().size();++i)if(selection.blockedBy(i)>=0)++count;
 bool saved=saveSelection();show(L"下次开始将略过 "+std::to_wstring(count)+L" 组（含共用 Hook 的组）。"+(saved?L"选择已保存。":L"本次选择有效，但未能保存设置文件。")+L"\r\n当前轮可用“略过本组”；发生同步错误后须重开对局。");
}
bool read(uint32_t a,void*out,size_t n){SIZE_T got=0;return gameProcess&&ReadProcessMemory(gameProcess,(void*)(uintptr_t)a,out,n,&got)&&got==n;}
template<class T>bool read(uint32_t a,T&v){return read(a,&v,sizeof v);}
bool enumerateModules(){modules.clear();HANDLE h=INVALID_HANDLE_VALUE;for(int n=0;n<3;++n){h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(h!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;}if(h==INVALID_HANDLE_VALUE)return false;
 MODULEENTRY32W m{};m.dwSize=sizeof m;if(Module32FirstW(h,&m))do{modules.push_back({uintptr_t(m.modBaseAddr),m.modBaseSize,m.szModule,m.szExePath});}while(Module32NextW(h,&m));CloseHandle(h);return !modules.empty();}
std::wstring owner(uintptr_t address){for(const auto&m:modules)if(address>=m.base&&address-m.base<m.size)return m.name+L"+"+hex(address-m.base);return L"unmapped/unknown";}
bool inRuntime(uint32_t address){for(const auto&m:modules)if(!_wcsicmp(m.name.c_str(),L"GameRuntime.dll")&&address>=m.base&&address-m.base<m.size)return true;return false;}
std::wstring digest(const std::wstring&path){HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return L"unreadable";HCRYPTPROV provider=0;HCRYPTHASH hash=0;std::wstring result=L"failed";
 if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){std::array<BYTE,65536>b;DWORD n=0;bool ok=true;while(true){if(!ReadFile(f,b.data(),DWORD(b.size()),&n,nullptr)){ok=false;break;}if(!n)break;if(!CryptHashData(hash,b.data(),n,0)){ok=false;break;}}BYTE out[32];DWORD length=sizeof out;if(ok&&CryptGetHashParam(hash,HP_HASHVAL,out,&length,0)&&length==32){result=bytes(out,32);result.erase(std::remove(result.begin(),result.end(),L' '),result.end());}}
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(f);return result;}
void recordModules(){for(const auto&m:modules){log(L"MODULE "+m.name+L" base="+hex(m.base)+L" size="+std::to_wstring(m.size)+L" path="+m.path);std::wstring p=m.path;std::transform(p.begin(),p.end(),p.begin(),[](wchar_t c){return wchar_t(towlower(c));});if(p.find(L"\\windows\\")==std::wstring::npos)log(L"FILE_SHA256 "+m.name+L" "+digest(m.path));}}
bool bundleMatches(){return !_wcsicmp(digest(folder+L"\\GameRuntime.dll").c_str(),HookTestRuntimeSha)&&!_wcsicmp(digest(folder+L"\\GameRuntimeHost32.exe").c_str(),HookTestBrokerSha);}
void recordSites(const wchar_t*tag){for(uint32_t a:sites){std::array<uint8_t,16>b{};unsigned n=experimentalpatch::size(a);if(!read(a,b.data(),n)){log(std::wstring(tag)+L" "+hex(a)+L" read-failed="+std::to_wstring(GetLastError()));continue;}uint32_t target=experimentalpatch::destination(a,b.data(),n);if(!target&&n>=5&&(b[0]==0xE9||b[0]==0xE8)){uint32_t rel;memcpy(&rel,b.data()+1,4);target=a+5+rel;}log(std::wstring(tag)+L" "+hex(a)+L" bytes="+bytes(b.data(),n)+L" known-original="+std::to_wstring(assist::hookregistry::knownOriginal(a,b.data(),n))+(target?L" destination="+hex(target)+L" "+owner(target):L""));}}
bool freshSnapshot(){if(!shared)return false;for(int n=0;n<3;++n){LONG before=shared->seq;if(before&1)continue;MemoryBarrier();memcpy(&snapshot,&shared->data,sizeof snapshot);MemoryBarrier();if(before==shared->seq)return DWORD(GetTickCount()-snapshot.sampledTick)<2000;}return false;}
void clearOptions(){if(diagnostic)diagnostic->camera=0;if(!shared)return;shared->flags=0;shared->controlAll=0;shared->networkHousePreserve=0;shared->controlCommandOwner=0;shared->nativeSellCursor=0;shared->nativeMegaGateway=0;shared->nativeCombatQueries=0;shared->buildTimeTooltip=0;shared->renderReveal=0;shared->crcMapShadow=0;shared->lowDamageFilter=shared->lowDamageNetwork=0;shared->nativeAirNetwork=0;shared->techEnhance=0;shared->placementImplementation=0;gameinput::setFrameSampling(subscription,false);MemoryBarrier();gameinput::wake(subscription);}
bool attach(){if(subscription)return true;if(!gameinput::runtimeAvailable()){show(L"缺少配套 GameRuntime.dll / GameRuntimeHost32.exe，或版本不符。");return false;}
 if(!diagnostic){wchar_t name[96];hookdiagnostic::name(name,pid,GetCurrentProcessId());diagnosticMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(hookdiagnostic::Channel),name);if(diagnosticMapping)diagnostic=(hookdiagnostic::Channel*)MapViewOfFile(diagnosticMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(hookdiagnostic::Channel));if(!diagnostic){if(diagnosticMapping)CloseHandle(diagnosticMapping);diagnosticMapping=nullptr;show(L"无法创建测试相机控制通道。");return false;}diagnostic->magic=hookdiagnostic::Magic;diagnostic->version=hookdiagnostic::Version;diagnostic->pid=pid;diagnostic->owner=GetCurrentProcessId();diagnostic->camera=0;diagnostic->heartbeat=GetTickCount();}
 wchar_t name[100];assist::mappingName(name,pid);mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(assist::Shared),name);DWORD error=GetLastError();if(!mapping||error==ERROR_ALREADY_EXISTS){if(mapping)CloseHandle(mapping);mapping=nullptr;show(L"该游戏已有工具箱映射，请完整重启游戏和旧工具箱。");return false;}
 shared=(assist::Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(assist::Shared));if(!shared){CloseHandle(mapping);mapping=nullptr;return false;}
 memset(shared,0,sizeof *shared);shared->magic=assist::Magic;shared->version=assist::Version;shared->pid=pid;shared->owner=GetCurrentProcessId();shared->hostWindow=gameinput::windowToken(window);shared->heartbeat=GetTickCount();shared->clockTick=GetTickCount();shared->scanMs=16;shared->externalDisplay=1;shared->viewZoomPercent=100;shared->computeRequested=shared->computeApplied=assist::compute::All;
 if(!gameinput::assistIdentity(gameinput::runtimeModule(),assist::Version,sizeof(assist::Shared),assist::MappingPrefix.text)){UnmapViewOfFile(shared);shared=nullptr;CloseHandle(mapping);mapping=nullptr;show(L"Runtime 协议不匹配。");return false;}
 subscription=gameinput::install(gameentry::Client::Assist,threadId,pid,gameWindow);if(!subscription){log(L"ATTACH_FAILED "+std::wstring(gameinput::installationStage())+L" error="+std::to_wstring(GetLastError()));UnmapViewOfFile(shared);shared=nullptr;CloseHandle(mapping);mapping=nullptr;return false;}log(L"ATTACHED production Runtime; every optional feature is OFF");clearOptions();return true;}
const hooktest::Step&step(){return hooktest::steps()[schedule[sequence.index]];}
void configure(){if(configured||!shared)return;clearOptions();switch(step().kind){
 case hooktest::FrameSignal:gameinput::setFrameSampling(subscription,true,true);break;
 case hooktest::Tooltip:shared->buildTimeTooltip=1;break;
 case hooktest::Mega:shared->nativeMegaGateway=1;break;
 case hooktest::Placement:shared->flags=assist::BuildAnywhere;break;
 case hooktest::Camera:if(diagnostic)diagnostic->camera=1;break;
 case hooktest::DirectPlacement:shared->placementImplementation=1;shared->flags=assist::BuildAnywhere;break;
 case hooktest::LowDamage:shared->lowDamageFilter=shared->lowDamageNetwork=1;break;
 case hooktest::AirType:case hooktest::AirFull:shared->nativeAirNetwork=step().kind==hooktest::AirFull?2:1;shared->flags=assist::NativeAirWaypoints;break;
 case hooktest::Control:shared->controlAll=1;break;
 case hooktest::ShroudClear:shared->renderReveal=1;break;
 case hooktest::ShroudHalf:shared->renderReveal=2;break;
 case hooktest::DirectMap:shared->renderReveal=3;break;
 case hooktest::Network:shared->networkHousePreserve=1;break;
 case hooktest::CommandOwner:shared->networkHousePreserve=shared->controlAll=shared->controlCommandOwner=1;break;
 case hooktest::Sell:shared->networkHousePreserve=shared->nativeSellCursor=1;break;
 case hooktest::Crc:shared->flags=assist::ExperimentalReveal;shared->crcMapShadow=1;break;
 case hooktest::TechPair:case hooktest::TechMaster:shared->flags=assist::UnsafeTech;break;
 default:break;
 }MemoryBarrier();gameinput::wake(subscription);configured=true;log(L"REQUEST "+std::wstring(step().name));}
void techCommand(){if(!shared||techRequested||(step().kind!=hooktest::TechPair&&step().kind!=hooktest::TechMaster)||!freshSnapshot()||snapshot.commandStatus!=1||snapshot.typeCount<1||snapshot.typeCount>assist::MaxTypes||shared->request!=shared->ack)return;
 memset(shared->techAllowed,1,sizeof shared->techAllowed);shared->command=assist::OpenTech;shared->arg=1;shared->arg2=step().kind==hooktest::TechMaster?2:0;shared->requestTypeHash=assist::typeListHash(snapshot.types,snapshot.typeCount);shared->requestTick=GetTickCount();shared->requestMatch=snapshot.match;MemoryBarrier();++shared->request;techRequested=true;gameinput::wake(subscription);}
bool verifyBytes(const std::vector<uint32_t>&expected){for(uint32_t a:sites){std::array<uint8_t,16>b{};unsigned n=experimentalpatch::size(a);if(!read(a,b.data(),n))return false;bool wanted=std::find(expected.begin(),expected.end(),a)!=expected.end();if(!wanted){if(memcmp(b.data(),baseline[a].data(),n))return false;}else if(directpatch::find(a)){if(!directpatch::installed(a,b.data(),n))return false;}else{if(!inRuntime(experimentalpatch::destination(a,b.data(),n)))return false;}}return true;}
struct SiteVerification {
 bool bytes=true,fresh=true,registry=true,command=true;
 bool complete()const{return bytes&&fresh&&registry&&command;}
 std::wstring reason()const{return L"bytes="+std::to_wstring(bytes)+L" snapshot-fresh="+std::to_wstring(fresh)+L" registry="+std::to_wstring(registry)+L" command="+std::to_wstring(command);}
};
SiteVerification inspectSites(const std::vector<uint32_t>&expected){
 SiteVerification v;v.bytes=verifyBytes(expected);
 if(shared){v.fresh=freshSnapshot();v.registry=v.fresh&&snapshot.hookInstalled==expected.size();v.command=v.fresh&&(expected.empty()||snapshot.commandStatus==1);}
 return v;
}
bool verifySites(const std::vector<uint32_t>&expected){return inspectSites(expected).complete();}
void detail(){if(!shared)return;freshSnapshot();auto h=gameinput::health(subscription);log(L"STATE frame="+std::to_wstring(snapshot.frame)+L" match="+std::to_wstring(snapshot.match)+L" command="+std::to_wstring(snapshot.commandStatus)+L" installed="+std::to_wstring(snapshot.hookInstalled)+L" conflicts="+std::to_wstring(snapshot.hookConflicts)+L" runtime-stage="+std::to_wstring(h.stage)+L" control="+std::to_wstring(shared->playerControlStatus)+L" network="+std::to_wstring(shared->networkHouseStatus)+L" crc="+std::to_wstring(shared->crcMapShadowStatus)+L" tech-result="+std::to_wstring(shared->result));if(h.faultSerial!=lastFault){lastFault=h.faultSerial;log(L"RUNTIME_FAULT serial="+std::to_wstring(h.faultSerial)+L" code="+hex(h.faultCode)+L" address="+hex(h.faultAddress));}}
void stop(const std::wstring&reason,bool oos=false){if(sequence.phase==Phase::Stopped||sequence.phase==Phase::Idle)return;stopReason=reason;log(L"STOP "+reason);recordSites(L"STOP_BYTES");detail();sequence.stop(oos);attemptAt=GetTickCount64();clearOptions();if(shared)shared->stop=1;show(L"已停止新增 Hook，等待 Runtime 完成还原。原因："+reason);}
void release(){if(subscription)gameinput::release(gameentry::Client::Assist,subscription);subscription=0;if(shared)UnmapViewOfFile(shared);shared=nullptr;if(mapping)CloseHandle(mapping);mapping=nullptr;if(diagnostic){diagnostic->camera=0;diagnostic->heartbeat=0;UnmapViewOfFile(diagnostic);}diagnostic=nullptr;if(diagnosticMapping)CloseHandle(diagnosticMapping);diagnosticMapping=nullptr;}
void next(){observation={};configured=techRequested=false;restorationLogged=false;attemptAt=GetTickCount64();log(L"STEP "+std::to_wstring(sequence.index+1)+L"/"+std::to_wstring(schedule.size())+L" "+step().name);std::wstring addresses;for(auto a:step().sites)addresses+=hex(a)+L" ";log(L"EXPECTED_SITES "+addresses);show(L"准备："+std::wstring(step().name));}
void tick(){if(sequence.phase==Phase::Idle||sequence.phase==Phase::Stopped||sequence.phase==Phase::Complete)return;uint64_t now=GetTickCount64(),elapsed=lastTimerAt?now-lastTimerAt:0;lastTimerAt=now;bool alive=gameProcess&&WaitForSingleObject(gameProcess,0)==WAIT_TIMEOUT;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);const bool focused=foreground==pid;
 if(shared){shared->heartbeat=GetTickCount();shared->foreground=focused;InterlockedExchange(&shared->hostNotifyPending,0);gameinput::wake(subscription);}
 if(diagnostic)diagnostic->heartbeat=GetTickCount();
 if(sequence.phase==Phase::Stopping){if(!alive||!shared||(shared->stop==2&&verifyBytes({}))){release();sequence.phase=Phase::Stopped;log(L"STOPPED cleanup="+std::wstring(!alive?L"process-exited":L"Runtime-acknowledged-and-baseline-restored")+L" reason="+stopReason);show(L"测试已停止，日志已保存。原因："+stopReason+L"\r\n下次测试请使用新游戏进程。");if(closeWanted)DestroyWindow(window);}else if(now-attemptAt>15000)show(L"还原仍未完成；保持窗口可继续清理。请退出游戏后再关闭测试器，日志已保存。");return;}
 if(!alive){stop(L"游戏进程退出（不能仅凭退出断言同步错误）");return;}
 uint8_t oos=0,running=0;uint32_t house=0,scenario=0;int frame=-1;if(!read(0xA8B8C2,oos)||!read(0xA8E9A0,running)||!read(0xA83D4C,house)||!read(0xA8B230,scenario)||!read(0xA8ED84,frame)){stop(L"游戏状态读取失败，未继续");return;}if(oos){stop(L"观察到原生 OOS 标记",true);return;}if(house!=initialHouse||scenario!=initialScenario||frame<latestFrame){stop(L"对局身份改变或游戏帧回退");return;}if(frame!=latestFrame)lastFrameMoveAt=now;latestFrame=frame;
 if(sequence.phase==Phase::Installing||sequence.phase==Phase::Observing||sequence.phase==Phase::Question){
  bool wasBackground=observation.background;
  if(!observation.focus(focused)){sequence.pause(elapsed);attemptAt+=elapsed;if(!wasBackground)log(L"PAUSE focus-lost; observation clock suspended; hooks retained; presentation/sampling may pause");show(L"已切出游戏，观察计时暂停；切回后核对并继续。\r\n已安装的本组可点正常提前确认，或点略过；同步错误仍会停止测试。");return;}
  if(wasBackground){sequence.pause(elapsed);attemptAt+=elapsed;elapsed=0;log(L"RESUME_WAIT focus-returned; revalidating native sites and Runtime snapshot");}
 }
 if((!running||now-lastFrameMoveAt>1000)&&(sequence.phase==Phase::Observing||sequence.phase==Phase::Question||sequence.phase==Phase::Installing)){sequence.pause(elapsed);attemptAt+=elapsed;show(L"游戏帧未前进或已暂停，观察倒计时暂停；如已断线请点击报告。");return;}
 if(now-lastLogAt>=5000){lastLogAt=now;detail();}
 if(sequence.phase==Phase::Restoring){
  // A background/minimized game may not dispatch cleanup yet. Continue
  // checking, but time away must not become a false restoration failure.
  if(!focused||!running||now-lastFrameMoveAt>1000){attemptAt+=elapsed;show(L"正在等待本组还原；请切回游戏让游戏线程完成清理，核对后继续。");}
  if(verifySites({})){log(L"RESTORED all 51 observed sites equal pre-test baseline; registry=0");if(sequence.restored(true,sequence.index+1<schedule.size(),now)){
   if(sequence.phase==Phase::Complete){log(L"COMPLETE scheduled observations; no netplay certification");stopReason=L"本轮所选项目已完成";sequence.stop();attemptAt=now;if(shared)shared->stop=1;}else next();}
  }else if(now-attemptAt>15000){sequence.phase=Phase::Blocked;restorationLogged=true;log(L"RESTORE_BLOCKED no next stage permitted");show(L"上一组没有完整还原，已阻止下一组。请点停止并退出游戏。");}return;}
 if(sequence.phase==Phase::Blocked)return;
 if(sequence.phase==Phase::Installing){
  bool ready=false;if(step().kind==hooktest::ReadOnly)ready=verifySites({});else if(attach()){configure();techCommand();if(enumerateModules())ready=freshSnapshot()&&snapshot.commandStatus==1&&verifySites(step().sites);}
  if(ready){sequence.installed(now);observation.verify(true,now);recordSites(L"INSTALLED_BYTES");log(L"OBSERVE_BEGIN verified installed set; 30 seconds");}
  else if(now-attemptAt>30000){sequence.phase=Phase::Blocked;recordSites(L"UNAVAILABLE_BYTES");detail();clearOptions();log(L"UNAVAILABLE not installed; not a passed test; requests withdrawn");show(L"本组未能完整安装，或签名/功能条件仍未就绪。不会记作通过。可略过（先还原）或停止。");}return;
 }
 if(sequence.phase==Phase::Observing||sequence.phase==Phase::Question){
  auto verification=inspectSites(step().sites);bool wasWaiting=observation.mismatch||observation.recovering;auto decision=observation.verify(verification.complete(),now);
  if(decision!=hooktest::Verification::Ready){
   if(!wasWaiting)log(L"VERIFY_WAIT "+verification.reason());sequence.pause(elapsed);
   if(decision==hooktest::Verification::Failed){log(L"VERIFY_FAILED "+verification.reason());stop(L"Hook/快照状态持续不一致："+verification.reason());}
   else show(L"正在等待游戏 Hook 和快照恢复，计时暂停。切换窗口不会立即停止测试。");return;
  }
  if(wasWaiting)log(L"RESUMED sites and fresh snapshot verified; observation clock continues");
  if(sequence.phase==Phase::Question){show(L"30 秒已到：请选择正常、略过本组，或报告同步错误。等待回答期间不会安装下一组。");return;}
  if(sequence.tick(now)){show(L"30 秒已到：是否同步错误或断线？请选择“正常，下一组”或“同步错误，停止”。");log(L"QUESTION awaiting user verdict; no automatic next hook");FLASHWINFO f{sizeof f,window,FLASHW_TRAY,3,0};FlashWindowEx(&f);MessageBeep(MB_ICONQUESTION);}else show(std::wstring(step().name)+L"\r\n观察中，剩余 "+std::to_wstring((hooktest::Sequence::Interval-(now-sequence.since)+999)/1000)+L" 秒。现在也可报告同步错误。\r\n安装成功后可随时点正常提前继续；未回答则 30 秒时提醒。");}
}
void openLog(){if(logFile!=INVALID_HANDLE_VALUE)CloseHandle(logFile);SYSTEMTIME t;GetLocalTime(&t);wchar_t name[100];swprintf(name,100,L"\\hook-test-%04u%02u%02u-%02u%02u%02u-%lu.log",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,pid);CreateDirectoryW((folder+L"\\logs").c_str(),nullptr);logFile=CreateFileW((folder+L"\\logs"+name).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);}
void start(){if(subscription||shared||(sequence.phase!=Phase::Idle&&sequence.phase!=Phase::Stopped))return;
 const int choice=int(SendMessageW(stepChoice,CB_GETCURSEL,0,0));const bool baselines=SendMessageW(baselineBox,BM_GETCHECK,0,0)==BST_CHECKED;auto planned=selection.schedule(choice,SendMessageW(followingBox,BM_GETCHECK,0,0)==BST_CHECKED,baselines);
 if(planned.empty()||(choice>0&&planned.size()==unsigned(baselines?2:0))){show(L"所选范围已全部略过（含共用 Hook 的组）。请修改略过清单或开始项。");return;}
 if(!bundleMatches()){show(L"配套 Runtime / 载入程序 SHA-256 不匹配，请重新解压完整测试包。");return;}wchar_t text[32];GetWindowTextW(pidEdit,text,32);pid=wcstoul(text,nullptr,10);if(gameProcess)CloseHandle(gameProcess);gameProcess=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);if(!gameProcess){show(L"无法只读打开游戏，错误 "+std::to_wstring(GetLastError())+L"。若游戏以管理员运行，请用相同权限运行测试器。");return;}
 uint32_t hwnd=0;uint8_t running=0,oos=0;DWORD actual=0;if(!read(0xB73550,hwnd)||!read(0xA8E9A0,running)||!running||!read(0xA83D4C,initialHouse)||initialHouse<0x10000||!read(0xA8B230,initialScenario)||initialScenario<0x10000||!read(0xA8ED84,latestFrame)||latestFrame<0||!read(0xA8B8C2,oos)||oos){show(L"请在全新的、已进入可操作状态且没有同步错误的对局中开始。");return;}
 gameWindow=(HWND)(uintptr_t)hwnd;threadId=GetWindowThreadProcessId(gameWindow,&actual);if(actual!=pid||!threadId||!enumerateModules()){show(L"无法确认游戏主窗口或模块列表，尚未安装 Hook。");return;}
 for(const auto&m:modules)if(!_wcsicmp(m.name.c_str(),L"GameRuntime.dll")){show(L"游戏已加载旧 Runtime。请关闭旧工具箱，完全重启游戏后再测；不能在原进程上获得干净基线。");return;}
 wchar_t mappingName[100];assist::mappingName(mappingName,pid);HANDLE existing=OpenFileMappingW(FILE_MAP_READ,FALSE,mappingName);if(existing){CloseHandle(existing);show(L"另一个工具箱已连接，请关闭工具箱并重启游戏。");return;}
 baseline.clear();for(auto a:sites){unsigned n=experimentalpatch::size(a);auto&b=baseline[a];b.resize(n);if(!read(a,b.data(),n)){show(L"无法读取 Hook 基线 "+hex(a)+L"，未启动测试。");return;}}
 openLog();if(logFile==INVALID_HANDLE_VALUE){show(L"无法创建 logs 日志。请把测试包放在可写目录。");return;}logText.clear();log(L"RA2 Hook Diagnostics / manual exclusions / early normal verdict / cursor-tooltip fix / explicit frame-hook test / peer-sync prerequisite reverted / Assist84 / Runtime4");log(L"MODE isolated groups; explicit verdict; 30-second reminder AFTER verified install; early normal accepted; manual skip; no protection bypass; platform label supplied by user=RA2OL");recordModules();recordSites(L"BASELINE");for(auto a:sites)if(hooktest::dormant(a))log(L"SKIPPED_DISABLED "+hex(a)+L" product-disabled scroll/C4; not installed");log(L"CATALOG 51 allowlisted = 48 available unique sites (camera geometry conditional) + 3 product-disabled; actual schedule excludes user selections; 14 denied sites are never installed");
 schedule=std::move(planned);log(L"SCHEDULE "+std::to_wstring(schedule.size())+L" stages; baselines="+(baselines?L"1":L"0")+L"; early normal verdict enabled; excluded groups are not passed");
 for(unsigned i=2;i<hooktest::steps().size();++i){int blocked=selection.blockedBy(i);if(blocked>=0)log(L"USER_EXCLUDED "+std::wstring(hooktest::steps()[i].name)+L" blocked-by="+hooktest::steps()[blocked].name+L" (not tested/not passed)");}
 stopReason.clear();lastFault=0;lastTimerAt=lastFrameMoveAt=GetTickCount64();sequence.start(0,lastTimerAt);closeWanted=false;next();}
void normal(){
 if(sequence.phase!=Phase::Observing&&sequence.phase!=Phase::Question)return;
 tick(); // Recheck OOS/world identity at the click, not only on the timer.
 if(sequence.phase!=Phase::Observing&&sequence.phase!=Phase::Question)return;
 // The verdict UI takes focus. Hooks remain installed; snapshot publication
 // can pause. Check exact site ownership after OOS/world checks above.
 if(!(observation.background?verifyBytes(step().sites):verifySites(step().sites))){show(L"状态尚未核对完成，暂未记录正常；请切回游戏等待恢复。");return;}
 if(shared&&shared->request!=shared->ack){show(L"当前功能请求尚未完成，暂未记录正常；请稍后再确认。");return;}
 uint64_t now=GetTickCount64(),observed=now>=sequence.since?now-sequence.since:0;
 if(!sequence.normal())return;
 log(L"USER_VERDICT normal observed-ms="+std::to_wstring(observed)+L" early="+(observed<hooktest::Sequence::Interval?L"1":L"0")+L" focus-paused="+(observation.background?L"1":L"0")+L" (manual observation; not certification)");
 recordSites(L"VERDICT_BYTES");detail();clearOptions();attemptAt=now;restorationLogged=false;show(L"已确认正常，正在还原本组；核对完成后进入下一组。");
}
void skip(){
 if(schedule.empty()||sequence.index>=schedule.size()||schedule[sequence.index]<2){show(L"两个基线不能略过；安装确认后可点正常提前结束观察。");return;}
 if(sequence.phase==Phase::Idle||sequence.phase==Phase::Complete||sequence.phase==Phase::Restoring)return;
 if(sequence.phase==Phase::Observing||sequence.phase==Phase::Question)tick();
 const unsigned current=schedule[sequence.index];selection.excluded[current]=true;SendMessageW(skipList,LB_SETSEL,TRUE,current-2);bool saved=saveSelection();
 log(L"USER_SKIP "+std::wstring(step().name)+L" (not passed); exclusion-saved="+(saved?L"1":L"0"));
 if(sequence.disconnected||sequence.phase==Phase::Stopping||sequence.phase==Phase::Stopped||restorationLogged){
  int choice=selection.nextChoice(current);if(choice>=0){SendMessageW(stepChoice,CB_SETCURSEL,choice,0);SendMessageW(followingBox,BM_SETCHECK,BST_CHECKED,0);saved=saveSelection();}
  show(L"已列入略过清单。请先完成清理并重开对局，再查找游戏、点开始。\r\n"+std::wstring(choice>=0?L"已选好下一组，并继续测后面的组。":L"当前组之后没有剩余可测项目。")+(saved?L"":L" 设置未能写入文件，请保留此测试器窗口。"));return;
 }
 if(shared&&shared->request!=shared->ack){show(L"略过选择已记录；当前功能请求尚未完成，请切回游戏等待完成后再略过，或停止并重开对局。");return;}
 if(!sequence.skip(restorationLogged)){show(L"略过选择已记录；本组仍在安装，请待安装确认后再略过，或停止并重开对局。");return;}
 schedule.erase(std::remove_if(schedule.begin()+sequence.index+1,schedule.end(),[](unsigned i){
  int blocked=selection.blockedBy(i);if(blocked<0)return false;
  log(L"SKIP_RELATED "+std::wstring(hooktest::steps()[i].name)+L" blocked-by="+hooktest::steps()[blocked].name+L" (not tested/not passed)");return true;
 }),schedule.end());
 clearOptions();attemptAt=GetTickCount64();restorationLogged=false;
 show(L"本组记为略过，正在还原；核对完成后继续后面的组。");
}
BOOL CALLBACK chooseWindow(HWND w,LPARAM){wchar_t title[256];GetWindowTextW(w,title,256);if(wcsstr(title,L"Yuri")||wcsstr(title,L"Red Alert 2")){DWORD candidate=0;GetWindowThreadProcessId(w,&candidate);SetWindowTextW(pidEdit,std::to_wstring(candidate).c_str());return FALSE;}return TRUE;}
HWND control(const wchar_t*cls,const wchar_t*text,DWORD style,int x,int y,int w,int h,int id){HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){switch(m){
 case WM_CREATE:window=w;font=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");control(L"STATIC",L"游戏 PID：",0,14,14,80,26,0);pidEdit=control(L"EDIT",L"",WS_BORDER|ES_NUMBER,100,12,100,28,101);control(L"BUTTON",L"查找游戏",0,212,12,100,28,102);control(L"BUTTON",L"开始",0,324,12,84,28,103);
  stepChoice=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,420,12,400,500,104);SendMessageW(stepChoice,CB_ADDSTRING,0,(LPARAM)L"全部未略过：按顺序逐组测试");for(unsigned i=2;i<hooktest::steps().size();++i)SendMessageW(stepChoice,CB_ADDSTRING,0,(LPARAM)hooktest::steps()[i].name);SendMessageW(stepChoice,CB_SETCURSEL,0,0);
  statusBox=control(L"STATIC",L"先关闭原工具箱并完整重启游戏，正常进入对局，再点开始。\r\n默认不略过功能组；安装成功后可随时点正常提前继续，也可手动略过本组。",0,14,54,806,76,0);
  control(L"BUTTON",L"正常，下一组",0,14,138,170,32,105);control(L"BUTTON",L"同步错误，停止",0,196,138,180,32,106);control(L"BUTTON",L"略过本组",0,388,138,170,32,107);control(L"BUTTON",L"停止并还原",0,570,138,160,32,108);
  followingBox=control(L"BUTTON",L"从选中组起，继续测后面的组（取消则只测所选组）",BS_AUTOCHECKBOX,14,180,806,28,111);SendMessageW(followingBox,BM_SETCHECK,BST_CHECKED,0);
  baselineBox=control(L"BUTTON",L"本轮也测两个基线（已确认正常可不选）",BS_AUTOCHECKBOX,14,210,806,28,113);
  control(L"STATIC",L"略过项目（点选，可多选）\r\n选中＝略过，下次开始生效",0,838,14,306,48,0);
  skipList=control(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_HSCROLL|LBS_MULTIPLESEL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,838,66,306,470,110);
  for(unsigned i=2;i<hooktest::steps().size();++i)SendMessageW(skipList,LB_ADDSTRING,0,(LPARAM)hooktest::steps()[i].name);SendMessageW(skipList,LB_SETHORIZONTALEXTENT,520,0);
  control(L"BUTTON",L"清除略过选择",0,838,550,306,32,112);loadSelection();
  logBox=control(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL,14,246,806,336,109);SendMessageW(logBox,EM_SETLIMITTEXT,150000,0);SetTimer(w,1,250,nullptr);if(!smoke)EnumWindows(chooseWindow,0);return 0;
 case WM_TIMER:tick();return 0;
 case assist::DataMessage:if(shared)InterlockedExchange(&shared->hostNotifyPending,0);return 0;
 case WM_COMMAND:switch(LOWORD(wp)){case 102:if(!subscription)EnumWindows(chooseWindow,0);break;case 103:start();break;case 105:normal();break;case 106:stop(L"用户报告同步错误/断线",true);break;case 107:skip();break;case 108:stop(L"用户停止");break;case 110:if(HIWORD(wp)==LBN_SELCHANGE)selectionChanged();break;case 111:case 113:saveSelection();break;case 104:if(HIWORD(wp)==CBN_SELCHANGE)saveSelection();break;case 112:SendMessageW(skipList,LB_SETSEL,FALSE,-1);selectionChanged();break;}return 0;
 case WM_CLOSE:if(subscription){closeWanted=true;stop(L"关闭测试器请求");}else DestroyWindow(w);return 0;
 case WM_DESTROY:KillTimer(w,1);if(!subscription)release();if(logFile!=INVALID_HANDLE_VALUE)CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;if(gameProcess)CloseHandle(gameProcess);gameProcess=nullptr;if(font)DeleteObject(font);PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,m,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int showCommand){
 smoke=command&&wcsstr(command,L"--ui-smoke");wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);folder=path;folder.resize(folder.find_last_of(L"\\/"));
 if(command&&wcsstr(command,L"--broker-smoke"))return bundleMatches()&&gameinput::runtimeAvailable()&&gameinput::assistIdentity(nullptr,assist::Version,sizeof(assist::Shared),assist::MappingPrefix.text)?0:5;
 WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=instance;c.lpszClassName=L"RA2HookDiagnosticWindow";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassW(&c))return 2;
 HWND w=CreateWindowExW(0,c.lpszClassName,L"RA2 Hook 逐组测试 — 可提前确认，可手动略过",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,1170,640,nullptr,nullptr,instance,nullptr);if(!w)return 3;
 if(smoke){
  bool ok=bundleMatches()&&SendMessageW(stepChoice,CB_GETCOUNT,0,0)==LRESULT(hooktest::steps().size()-1)&&SendMessageW(skipList,LB_GETCOUNT,0,0)==LRESULT(hooktest::steps().size()-2);
  ok=ok&&SendMessageW(baselineBox,BM_GETCHECK,0,0)==BST_UNCHECKED&&SendMessageW(followingBox,BM_GETCHECK,0,0)==BST_CHECKED&&SendMessageW(skipList,LB_GETSEL,0,0)==0;
  auto plan=selection.schedule(0,true);ok=ok&&plan.size()==19&&plan.front()==2;
  RECT client{};GetClientRect(w,&client);
  for(int id:{101,109,110,111,112,113}){RECT r{};HWND child=GetDlgItem(w,id);ok=ok&&child&&GetWindowRect(child,&r);MapWindowPoints(nullptr,w,(POINT*)&r,2);ok=ok&&r.left>=0&&r.top>=0&&r.right<=client.right&&r.bottom<=client.bottom;}
  SendMessageW(w,WM_COMMAND,112,0);ok=ok&&selection.schedule(0,true).size()==19;
  SendMessageW(skipList,LB_SETSEL,TRUE,1);SendMessageW(w,WM_COMMAND,MAKEWPARAM(110,LBN_SELCHANGE),(LPARAM)skipList);
  ok=ok&&selection.excluded[3]&&selection.blockedBy(3)==3;
  // An OOS skip records the exclusion and selects the next group, without
  // advancing the stopped game or opening any game process.
  schedule={3,4};sequence.start(0,0);sequence.stop(true);sequence.phase=Phase::Stopped;skip();
  ok=ok&&sequence.phase==Phase::Stopped&&sequence.disconnected&&SendMessageW(stepChoice,CB_GETCURSEL,0,0)==3;
  sequence={};schedule.clear();SetWindowTextW(pidEdit,L"0");start();normal();skip();
  ok=ok&&sequence.phase==Phase::Idle&&!subscription&&!shared&&!gameProcess;
  DestroyWindow(w);return ok?0:4;
 }
 ShowWindow(w,showCommand);UpdateWindow(w);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
