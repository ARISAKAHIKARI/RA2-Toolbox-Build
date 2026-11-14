"""Execute the actual shared map-completion lease, including conflicts."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3];source=root/'source/assist/source/view_zoom_native_bridge.h';text=source.read_text()
def block(marker):
 start=text.index(marker);at=text.index('{',start);end=at+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
code=r'''
#include <cassert>
#include <cstdio>
struct Site {unsigned address=5;bool current=true;};static Site nativeViewSites[17];
static bool nativeMapZoomLease=false,nativeMapPipLease=false,active=false,conflict=false,denyRestore=false;static unsigned installed=0,restored=0;
struct Registry {bool active(unsigned){return ::active;}bool conflict(unsigned){return ::conflict;}} codeHookRegistry;
static void nativeViewMakeSites(){}
static bool riskSiteCurrent(const Site&s){return s.current;}
static bool installRiskSite(Site&){assert(!active);active=true;++installed;return true;}
static bool restoreRiskSite(Site&s){if(denyRestore||conflict||!s.current)return false;active=false;++restored;return true;}
'''+block('static bool nativeMapAcquire(')+'\n'+block('static bool nativeMapRelease(')+r'''
int main(){
 assert(nativeMapAcquire(false)&&nativeMapAcquire(true)&&installed==1);assert(nativeMapRelease(false)&&active&&restored==0);assert(nativeMapRelease(true)&&!active&&restored==1);
 assert(nativeMapAcquire(true)&&nativeMapAcquire(false)&&installed==2);assert(nativeMapRelease(true)&&active);assert(nativeMapRelease(false)&&!active&&restored==2);
 active=true;assert(!nativeMapAcquire(false));active=false;assert(nativeMapAcquire(false));nativeViewSites[5].current=false;assert(!nativeMapAcquire(true)&&!nativeMapRelease(false)&&nativeMapPipLease&&active);nativeViewSites[5].current=true;assert(nativeMapRelease(false));
 assert(nativeMapAcquire(true));denyRestore=true;assert(!nativeMapRelease(true)&&nativeMapZoomLease&&active);denyRestore=false;assert(nativeMapRelease(true));assert(nativeMapRelease(true));
 std::puts("PASS production shared map-completion lease: both acquisition/release orders, one installation, no early unhook, foreign owner/replacement rejection and failed-restore retention");
}
'''
cpp=a.out/'lease.cpp';cpp.write_text(code);exe=a.out/'lease';subprocess.run(['g++','-std=c++17','-O1','-UNDEBUG',str(cpp),'-o',str(exe)],check=True);r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout+r.stderr);(a.out/'result.json').write_text(json.dumps(dict(passed=r.returncode==0,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),game_tested=False))+'\n');assert r.returncode==0
