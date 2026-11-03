"""Execute production MO preview/input paths with bounded world and Win32 seams.

No process injection or game mutation. Baseline failures are real old production
headers fed the same motion/heartbeat/seqlock inputs, not a reimplementation.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess
import mo_orders_bridge_test as mo
import formation_quick_bridge_test as quick

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'source'

def runtime_source():
    text = mo.source()
    marker = ' std::cout<<"PASS "<<checks'
    extra = r'''
 // One native pointer query yields both the cell and hovered object.
 begin();held.fill(false);held[17]=true;sh->moFeatures=1;
 int beforeQueries=locationQueries;check(messageMo(WM_MBUTTONDOWN));check(locationQueries==beforeQueries+1);
 // Move one cell: sparse stroke stays at its starting cell, current outline
 // follows the current cell at 16ms instead of lagging two cells / 80ms.
 clockValue+=16;shared.heartbeat=clockValue;MSG motion;motion.message=WM_MOUSEMOVE;motion.lParam=MAKELPARAM(130,100);
 check(moOrdersMessage(&motion));check(moStrokeCount==1);check(sh->moOrdersPreview.nodes[0].x==(31-2)*256+128);
 // Long drag: a short host-render heartbeat stall does not cancel a read-only
 // preview; it still cannot authorize a release or any native event.
 for(int i=0;i<200;++i){clockValue+=100;shared.heartbeat=clockValue-300;moOrdersTick(lastHouse,snap.frame,clockValue);check(moHolding&&sh->moOrdersPreview.active&&events.empty());}
 check(messageMo(WM_MBUTTONUP));check(!moHolding&&events.empty());
 begin();check(messageMo(WM_MBUTTONDOWN));clockValue+=1001;moOrdersTick(lastHouse,snap.frame,clockValue);check(!moHolding&&events.empty());
 // Patrol outline and final route include the unsparsified last cell.
 begin();check(messageMo(WM_MBUTTONDOWN));clockValue+=16;shared.heartbeat=clockValue;
 motion.message=WM_MOUSEMOVE;motion.lParam=MAKELPARAM(130,100);check(moOrdersMessage(&motion));check(moStrokeCount==1);
 check(sh->moOrdersPreview.nodes[0].x==(31-2)*256+128);
'''
    return text.replace(marker, extra+marker)

def quick_source():
    text = quick.fixture.replace('@LAYOUT_HEADER@', json.dumps(str(SOURCE/'formation.h')))
    marker = ' std::cout<<"PASS production quick placement:'
    extra = r'''
 // A fast corner inside the layout cadence window must be sampled. Layout
 // remains bounded to 16ms; the path is not reduced to its last chord.
 setup();message(WM_MBUTTONDOWN);++clockValue;message(WM_MOUSEMOVE,0,false,160,100);
 ++clockValue;message(WM_MOUSEMOVE,0,false,160,160);clockValue+=14;message(WM_MOUSEMOVE,0,false,100,160);
 assert(formationQuickPath.count==4);
 assert(formationQuickPath.nodes[1].x==32&&formationQuickPath.nodes[1].y==30);
 assert(formationQuickPath.nodes[2].x==32&&formationQuickPath.nodes[2].y==32);
 // The final move inside a cadence window still flushes after the mouse
 // stops; unchanged later ticks must not solve or publish the layout again.
 ++clockValue;message(WM_MOUSEMOVE,0,false,160,160);auto revision=formationView.revision;
 clockValue+=16;shared.heartbeat=clockValue;formationTick(lastHouse,snap.frame,clockValue);assert(formationView.revision>revision);
 revision=formationView.revision;clockValue+=20;shared.heartbeat=clockValue;formationTick(lastHouse,snap.frame,clockValue);assert(formationView.revision==revision);
 for(int i=0;i<200;++i){clockValue+=100;shared.heartbeat=clockValue-300;formationTick(lastHouse,snap.frame,clockValue);assert(formationQuick&&formationView.active&&commands.empty());}
 message(WM_MBUTTONUP,0,false,100,160);assert(!formationView.active&&commands.empty()&&!formationJobCount);
'''
    return text.replace(marker, extra+marker)

def host_source(directory=SOURCE):
    # Compile the exact production reader + identity comparison; drawing uses
    # GDI types outside this fixture and does not participate in snapshot reads.
    header=(directory/'mo_orders_host.h').read_text(encoding='utf-8')
    begin=header.index('struct Drawing')
    end=header.index('static bool point(')
    reader=header[begin:end]
    return r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include "mo_orders.h"
#include "native_viewport_host.h"
#include "../../external_reader/source/projection.h"
using namespace assist;using DWORD=uint32_t;
static uint32_t pid=1,epoch=7,clockValue=1000;static bool foreground=true;
struct Snapshot {bool chat=false;int cameraX=0,cameraY=0,width=800,height=600;}snap;
struct Wire {long moOrdersPreviewSeq=0;moorders::Preview moOrdersPreview;uint32_t moFeatures=7;int32_t viewportSerial=0,viewZoomBusy=0;uint32_t viewportActive=0,viewportGeneration=0,viewportHostCapabilities=0;int32_t viewportPhysical[4]{},viewportLogical[4]{};}wire;
static Wire*shared=&wire;
static uint32_t commandMatch(){return epoch;}static bool foregroundGame(){return foreground;}
static void MemoryBarrier(){}static uint32_t GetTickCount(){return clockValue;}
static std::wstring runtimeGestureDescription(uint32_t){return L"test";}
namespace moordershost {
''' + reader + r'''
}
int main(){
 wire.moOrdersPreview.active=1;wire.moOrdersPreview.mode=4;wire.moOrdersPreview.match=epoch;wire.moOrdersPreview.tick=clockValue;
 assert(moordershost::drawing().active);
 // Writer collisions reuse complete geometry, including a 20-second drag.
 for(int i=0;i<200;++i){clockValue+=100;wire.moOrdersPreviewSeq+=2;wire.moOrdersPreview.tick=clockValue;assert(moordershost::drawing().active);++wire.moOrdersPreviewSeq;assert(moordershost::drawing().active);++wire.moOrdersPreviewSeq;}
 // A stable cancellation takes effect immediately, never preserved as stale.
 wire.moOrdersPreview.active=0;assert(!moordershost::drawing().active);
 wire.moOrdersPreview.active=1;assert(moordershost::drawing().active);wire.moOrdersPreviewSeq|=1;++pid;assert(!moordershost::drawing().active);
 ++wire.moOrdersPreviewSeq;assert(moordershost::drawing().active);wire.moOrdersPreviewSeq|=1;++epoch;assert(!moordershost::drawing().active);
 ++wire.moOrdersPreviewSeq;wire.moOrdersPreview.match=epoch;wire.moOrdersPreview.tick=clockValue;assert(moordershost::drawing().active);
 foreground=false;assert(!moordershost::drawing().active);foreground=true;wire.moOrdersPreviewSeq|=1;assert(!moordershost::drawing().active);
 ++wire.moOrdersPreviewSeq;wire.moOrdersPreview.tick=clockValue;assert(moordershost::drawing().active);clockValue+=501;wire.moOrdersPreviewSeq|=1;assert(!moordershost::drawing().active);
}
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--baseline',type=Path);args=p.parse_args()
    args.build_dir.mkdir(parents=True,exist_ok=True);results=[]
    for name,create in [('runtime',runtime_source),('quick',quick_source),('host',lambda:host_source(args.baseline or SOURCE))]:
        # Windows CRT assertion dialogs can block unattended negative fixtures.
        # Keep all assertions active and fail immediately with their expression.
        assertion = '#include <cassert>\n#include <cstdio>\n#include <cstdlib>\n#undef assert\n#define assert(value) ((value)?(void)0:(std::fprintf(stderr,"assert: %s at %d\\n",#value,__LINE__),std::_Exit(3)))\n'
        code=create().replace('#include <cassert>',assertion)
        cpp=args.build_dir/(name+'.cpp');exe=args.build_dir/(name+'.exe');cpp.write_text('#ifdef NDEBUG\n#undef NDEBUG\n#endif\n'+code,encoding='utf-8')
        include=([f'-I{args.baseline}'] if args.baseline else [])+[f'-I{SOURCE}']
        build=subprocess.run([str(args.zig),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2',*include,str(cpp),'-o',str(exe)],capture_output=True,text=True)
        if build.returncode:raise RuntimeError(build.stdout+build.stderr)
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
        results.append({'fixture':name,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
    used=args.baseline or SOURCE
    hashes={str(p.relative_to(used)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (used/'mo_orders_bridge.h',used/'formation_bridge.h',used/'mo_orders_host.h')}
    result={'pass':all(r['exit_code']==0 for r in results),'assertions_active':True,'baseline':str(args.baseline) if args.baseline else None,'source_sha256':hashes,'results':results}
    (args.build_dir/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2));raise SystemExit(0 if result['pass'] else 1)

if __name__=='__main__':main()
