#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using DWORD=uint32_t;
static DWORD now=10000,pid=0,lastFind=0,lastProcessCheck=0,hook=0;
static bool enabled=true,bridgeBlocked=false;
static unsigned bridgeAttempts=0,registered=0,detached=0;
static DWORD bridgeRetryAt=0,sentAt=0,inFlightMatch=0;
static int registeredHotkeyMode=1;
static uint64_t attachAttempts=0;
static std::wstring bridgeFailure,status,detachReason;
static std::vector<std::wstring> logs;
struct Shared {uint32_t request=0,ack=0,stop=0;};
static Shared mapping,*shared=nullptr;
static struct {int status=0;} incoming;
static std::vector<unsigned> commandQueue,hotkeyPending;
static DWORD GetTickCount(){return now;}
static int desiredHotkeyMode(){return enabled?1:0;}
static void registerKeys(){++registered;hotkeyPending.clear();registeredHotkeyMode=desiredHotkeyMode();}
static void setStatus(const std::wstring& value){status=value;}
static void critical(const std::wstring& value){logs.push_back(value);}
namespace suite {static unsigned reports=0;static bool value=false;static void reportEnabled(int,bool v){++reports;value=v;}}
namespace external {
namespace targetselection {
struct Policy {DWORD selectedPid=0;uint64_t selectedCreation=0,revision=1;};
static Policy selected;static bool live=true;static unsigned aliveChecks=0;
static uint64_t revision(){return selected.revision;}
static Policy current(){return selected;}
static bool preferredAlive(const Policy&){++aliveChecks;return live;}
}
namespace discovery {
struct Candidate {DWORD pid=0,thread=0;};
struct Scan {
 Candidate best;DWORD processCount=0,windowCount=0,enumeratedProcesses=0,nameCandidates=0;
 DWORD descriptionMatches=0,descriptionCandidates=0,descriptionPending=0,rememberedMatches=0;
 bool explicitSelection=false;
 DWORD fingerprintMatches=0,fingerprintCandidates=0,fingerprintMismatches=0;
 DWORD descriptionErrors=0,lastDescriptionError=0,queryErrors=0,lastQueryError=0,snapshotError=0;
};
}
}
static struct Reader {bool live=true;bool alive()const{return live;}} externalReader;
static void detach(const wchar_t* reason=L"fixture"){
 ++detached;detachReason=reason;pid=hook=0;shared=nullptr;commandQueue.clear();
}
static void pauseAssistControls(){detach(L"legacy-pause");}
#include HOST_CONNECTION_ROUTES
static unsigned checks=0,failures=0;
static void check(bool condition,const char* message){++checks;if(!condition){++failures;std::cout<<"FAIL: "<<message<<'\n';}}
int main(){
#ifdef LEGACY_NEGATIVE_CONTROL
 enabled=true;requestGameReconnect();
 check(enabled,"connect action must not pause an enabled host");
#else
 // Paused and disconnected: make discovery due, without entering Runtime.
 enabled=false;registeredHotkeyMode=0;bridgeBlocked=true;bridgeAttempts=9;bridgeRetryAt=30000;bridgeFailure=L"prior";
 requestGameReconnect();
 check(enabled&&suite::value,"resume host without a second master switch");
 check(lastFind==now-2000&&lastProcessCheck==now-250,"make discovery/liveness due");
 check(!bridgeBlocked&&!bridgeAttempts&&!bridgeRetryAt&&bridgeFailure.empty(),"clear only disconnected retry backoff");
 check(registered==1&&detached==0&&!shared&&!hook,"no synchronous detach/install from retry");
 check(status==L"正在检查游戏连接…","no false success before attachment");
 // Existing in-flight work and gestures survive repeated connection checks.
 shared=&mapping;pid=42;hook=77;mapping={105,104,0};sentAt=9888;inFlightMatch=9;
 commandQueue={10,20};hotkeyPending={30};bridgeBlocked=true;bridgeAttempts=4;bridgeRetryAt=20000;bridgeFailure=L"latched";
 const unsigned registrations=registered;
 requestGameReconnect();requestGameReconnect();
 check(shared==&mapping&&hook==77&&pid==42,"keep active mapping/subscription/PID");
 check(mapping.request==105&&mapping.ack==104&&mapping.stop==0,"never replay or cancel unacknowledged request");
 check(sentAt==9888&&inFlightMatch==9&&commandQueue==std::vector<unsigned>({10,20}),"keep in-flight intent and queue");
 check(hotkeyPending==std::vector<unsigned>({30})&&registered==registrations,"healthy click preserves pending gesture");
 check(bridgeBlocked&&bridgeAttempts==4&&bridgeRetryAt==20000&&bridgeFailure==L"latched","keep existing connection fault latch");
 check(detached==0,"healthy repeated click never releases connection");
 incoming.status=-2;requestGameReconnect();check(status.find(L"异常")!=std::wstring::npos&&bridgeBlocked,"fault remains visible and latched");
 incoming.status=-1;requestGameReconnect();check(status.find(L"不兼容")!=std::wstring::npos&&bridgeBlocked,"incompatible state is not disguised as success");
 incoming.status=0;
 // Saving search/allowlist policy does not disrupt this live game.
 using external::targetselection::selected;
 serviceGameTargetPreference();++selected.revision;serviceGameTargetPreference();
 check(detached==0&&shared==&mapping,"allowlist/search-only revisions preserve session");
 check(external::targetselection::aliveChecks==0,"no process query for ordinary policy edits");
 selected.selectedPid=43;selected.selectedCreation=900;external::targetselection::live=false;++selected.revision;
 serviceGameTargetPreference();check(detached==0&&pid==42,"exited/recycled selection cannot tear down healthy game");
 selected.selectedPid=44;selected.selectedCreation=901;external::targetselection::live=true;++selected.revision;
 serviceGameTargetPreference();check(detached==1&&detachReason==L"user-selected-process","explicit live target uses normal retirement");
 check(enabled&&!shared&&!hook&&commandQueue.empty(),"old target queue is discarded on deliberate target change");
 check(!bridgeBlocked&&!bridgeRetryAt,"new target is eligible for normal guarded connect");
 serviceGameTargetPreference();check(detached==1,"same revision is idempotent");
 shared=&mapping;pid=44;hook=88;commandQueue={90};++selected.revision;serviceGameTargetPreference();
 check(detached==1&&pid==44&&commandQueue.size()==1,"list edit preserves selected healthy session");
 selected.selectedPid=0;selected.selectedCreation=0;++selected.revision;serviceGameTargetPreference();
 check(detached==1&&shared==&mapping,"clearing session preference preserves current connection");
 selected.selectedPid=44;selected.selectedCreation=901;++selected.revision;enabled=false;serviceGameTargetPreference();
 check(detached==1&&enabled&&pid==44,"selecting same live process resumes without retiring it");
 // Automatic diagnostics distinguish no target, chosen PID and access failure.
 external::discovery::Scan scan;scan.enumeratedProcesses=200;attachAttempts=1;logs.clear();
 logDiscovery(scan,0,3);check(logs.size()==1&&logs[0].find(L"selectedPid=0")!=std::wstring::npos,"log first no-target failure");
 ++attachAttempts;now+=2000;logDiscovery(scan,0,3);check(logs.size()==1,"unchanged discovery is rate-limited");
 now+=15000;logDiscovery(scan,0,4);check(logs.size()==2,"persistent failure retains periodic evidence");
 scan.best.pid=45;scan.best.thread=1045;scan.processCount=1;scan.rememberedMatches=1;scan.explicitSelection=true;
 logDiscovery(scan,5,8);check(logs.size()==3&&logs.back().find(L"openError=5")!=std::wstring::npos,"access error is always in automatic log");
 check(logs.back().find(L"remembered=1")!=std::wstring::npos&&logs.back().find(L"explicitSelection=1")!=std::wstring::npos,"record recognition and explicit-selection evidence");
 logDiscovery(scan,0,2);check(logs.size()==4&&logs.back().find(L"openError=0")!=std::wstring::npos,"record recovery immediately");
 // Unsigned elapsed arithmetic remains correct around the 32-bit clock wrap.
 shared=nullptr;now=100;requestGameReconnect();
 check(DWORD(now-lastFind)==2000&&DWORD(now-lastProcessCheck)==250,"retry deadline survives DWORD wrap");
#endif
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
