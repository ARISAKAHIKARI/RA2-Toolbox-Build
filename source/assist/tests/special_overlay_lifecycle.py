"""Exercise production overlay admission with controlled worker completions."""
from pathlib import Path
import argparse,subprocess,json
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2];host=(root/'assist/source/special_unit_overlay_host.h').read_text();engine=(root/'assist/source/special_unit_overlay_engine.h').read_text()
identity=engine[engine.index('inline bool enabled'):engine.index('template<class Reader')]
types=host[host.index('struct Request'):host.index('static external::ReadSession')]
production=host[host.index('static std::shared_ptr<const Frame>frame;'):host.index('struct Drawing')]
code=r'''
#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include "world_frame.h"
#include "native_viewport_projection.h"
#include "enemy_markers.h"
#include "projection.h"
namespace nativeviewhost {using Transform=nativeviewportprojection::Transform;}
namespace external {namespace voxel {struct Image{};}namespace presentation {
struct Selection {uint64_t revision=1;};struct Settings {Selection value;Selection get(){return value;}};static Settings settings_;Settings&settings(){return settings_;}
}}
static uint32_t now=1000,pid=7,match=1;static uint64_t paintRevision=0;
static bool externalData=true,front=true,externalSourceConfirmed=true;static worldframe::Token externalWorldSource;
static struct{bool enemyMarkers=true,enemySpecialOnly=true;int enemySpecialMode=1;}extra;
static struct{bool chat=false;int cameraX=0,cameraY=0,width=800,height=600;}snap;
static struct{external::enemies::Frame markers;const auto&enemyMarkers(){return markers;}}externalSampler;
static uint32_t GetTickCount(){return now;}static bool foregroundGame(){return front;}static uint32_t commandMatch(){return match;}
namespace specialoverlay {
'''+identity+'}\n'+types+r'''
static struct{std::shared_ptr<const Frame>result;int invalidations=0,submissions=0;
 void invalidate(){result.reset();++invalidations;}auto get(){return result;}void submit(Request){++submissions;}}worker;
'''+production+r'''
static nativeviewhost::Transform transform;
static void source(int logic=1){auto next=std::make_shared<worldframe::Snapshot>();next->key.pid=pid;next->key.match=match;next->key.house=9;next->key.frame=logic;next->key.tick=now;externalWorldSource=next;
 auto&m=externalSampler.markers;m.count=1;m.match=match;m.house=9;m.frame=logic;m.items[0].object=0x200000;m.items[0].id=1;m.items[0].type=0x300000;m.items[0].owner=0x400000;}
static std::shared_ptr<Frame> completed(){auto f=std::make_shared<Frame>();f->pid=pid;f->match=match;f->source=externalWorldSource;f->transform=transform;f->selection=external::presentation::settings().get();f->identities={{0x200000,1,0x300000,0x400000}};f->composite=std::make_shared<external::voxel::Image>();return f;}
int main(){
 source();update(transform);assert(worker.submissions==1&&!frame);worker.result=completed();update(transform);assert(frame);const auto revision=paintRevision;update(transform);assert(paintRevision==revision&&worker.submissions==1);
 externalSourceConfirmed=false;now+=119;update(transform);assert(frame);++now;update(transform);assert(!frame);externalSourceConfirmed=true;
 source();update(transform);worker.result=completed();update(transform);assert(frame);externalSampler.markers.count=0;update(transform);assert(!frame&&!lastConfirmed);
 source();update(transform);worker.result=completed();update(transform);assert(frame);++externalSampler.markers.items[0].owner;update(transform);assert(!frame);
 source();update(transform);worker.result=completed();update(transform);assert(frame);++external::presentation::settings().value.revision;update(transform);assert(!frame);
 worker.result=completed();update(transform);assert(frame);extra.enemySpecialMode=0;update(transform);assert(!frame);const int invalidations=worker.invalidations;update(transform);assert(worker.invalidations==invalidations);extra.enemySpecialMode=1;
 source();update(transform);auto old=completed();worker.result=old;update(transform);assert(frame);now+=151;source(2);update(transform);assert(!frame);
 worker.result=completed();update(transform);assert(frame);snap.chat=true;update(transform);assert(!frame);snap.chat=false;
 source();update(transform);worker.result=completed();update(transform);assert(frame);++match;source();update(transform);assert(!frame);worker.result=old;update(transform);assert(!frame);
 worker.result=completed();update(transform);assert(frame);front=false;update(transform);assert(!frame);
 // A transient compositor failure must not become a completed warm frame.
 // Hold the exact same paused source/camera/settings while the worker retries.
 front=true;source();update(transform);auto failed=completed();failed->composite.reset();worker.result=failed;update(transform);
 const int beforeRetry=worker.submissions;now+=15;update(transform);assert(worker.submissions==beforeRetry);
 ++now;update(transform);assert(worker.submissions==beforeRetry+1);
 worker.result=completed();update(transform);assert(frame&&frame->composite);const int recovered=worker.submissions;
 now+=1000;update(transform);assert(worker.submissions==recovered);
 // A genuinely empty/cull-only result has no pixels to compose and stays idle.
 clear();source();update(transform);auto empty=completed();empty->identities.clear();empty->composite.reset();worker.result=empty;update(transform);
 const int emptySubmissions=worker.submissions;now+=1000;update(transform);assert(worker.submissions==emptySubmissions);
 std::cout<<"PASS warm no-op, bounded unconfirmed hold, confirmed empty, identity/profile/mode/world changes, stale completion, chat/focus cleanup, paused compositor retry and empty-result idle\n";
}
'''
cpp=a.out/'lifecycle.cpp';cpp.write_text(code);exe=a.out/'lifecycle'
cmd=['g++','-std=c++17','-O1','-I'+str(root/'shared/source'),'-I'+str(root/'external_reader/source'),str(cpp),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);(a.out/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(exe)],capture_output=True,text=True);(a.out/'result.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
(a.out/'result.json').write_text(json.dumps({'pass':True,'real_game_tested':False})+'\n')
