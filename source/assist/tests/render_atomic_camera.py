#!/usr/bin/env python3
"""Execute production Host configuration/group methods with local boundaries.

The real HWND/thread loop has separate Windows tests; this checks the camera
and scene lock transaction, including a concurrent compositor-state observer.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'shared/source/overlay_compositor.h').read_text('utf-8')
start=source.index(' Context configure(')
end=source.index(' void suspend()',start)
methods=source[start:end]
header=(root/'shared/source/overlay_composition.h').as_posix()
statistics='struct UploadStatistics {'+source.split('struct UploadStatistics {',1)[1].split('};',1)[0]+'};'
fixture=r'''
#include "HEADER"
#include "GROUP"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <cstdio>
using DWORD=uint32_t;
static uint64_t GetTickCount64(){return 1000;}
using overlaycomposition::Context;using overlaycomposition::Surface;
struct Node {Surface surface;uint64_t lastSeen=0;};
STATISTICS
class Host {
 std::mutex mutex;std::condition_variable cv;std::thread thread;
 std::map<uint64_t,Node>nodes;Context context{};
 uint64_t generation=0,revision=0,heartbeat=0;bool enabled=false,stop=false,top=false;
 int rate=60;DWORD error=0;bool startupFailed=false;
 UploadStatistics uploadStatistics_{};bool uploadContinuous_=false;
 void run(){}
public:
 ~Host(){if(thread.joinable())thread.join();}
'''.replace('HEADER',header).replace('GROUP',(root/'shared/source/overlay_group.h').as_posix()).replace('STATISTICS',statistics)
observers=r'''
 bool snapshotValid(){std::lock_guard<std::mutex>lock(mutex);auto i=nodes.find(3);
  return i!=nodes.end()&&overlaycomposition::valid(i->second.surface,context);}
 void presentationError(DWORD code,bool fatal){std::lock_guard<std::mutex>lock(mutex);error=code;startupFailed=fatal;}
};
int main(){
 Host host;auto c=host.configure(101,77,8,640,480,0,0,true,false,500);
 auto pixels=std::make_shared<const std::vector<uint32_t>>(16,0xff123456);
 auto layer=[&](const Context&view){return Surface{3,5,view,{0,0,4,4},true,255,pixels};};
 assert(host.replaceGroup({3},{layer(c)})&&host.snapshotValid());
 // Runtime User32 upload failures retain compositor ownership and continue
 // accepting the latest frames. Only failed HWND startup enables fallback.
 host.presentationError(123,false);
 assert(host.capture(101,77).token==c.token&&host.publish(layer(c))&&host.touch(3,101,77));
 assert(host.replaceGroupAt(c,{3},{layer(c)})&&host.snapshotValid());
 host.presentationError(123,true);
 assert(!host.capture(101,77).token&&!host.publish(layer(c))&&!host.touch(3,101,77));
 assert(!host.replaceGroup({3},{layer(c)}));host.presentationError(0,false);
 // The original configure-then-publish sequence exposes precisely the gap:
 // the camera changes before a matching scene exists.
 auto legacy=host.configure(101,77,8,640,480,1,0,true,false,500);
 assert(!host.snapshotValid());assert(host.replaceGroup({3},{layer(legacy)})&&host.snapshotValid());
 std::atomic<bool>stop{false},bad{false};
 std::thread compositor([&]{while(!stop.load())if(!host.snapshotValid())bad=true;});
 for(int camera=2;camera<1002;++camera){
  auto desired=host.configure(101,77,8,640,480,camera,camera/2,true,false,500,true);
  auto prior=host.capture(101,77);assert(prior.cameraX==camera-1&&host.snapshotValid());
  auto invalid=desired;invalid.token++;
  assert(!host.replaceGroupAt(invalid,{3},{layer(invalid)}));
  assert(host.capture(101,77).cameraX==camera-1&&host.snapshotValid());
  std::this_thread::yield();
  assert(host.replaceGroupAt(desired,{3},{layer(desired)}));
  assert(host.capture(101,77).cameraX==camera&&host.snapshotValid());
 }
 stop=true;compositor.join();assert(!bad.load());
 std::puts("PASS: actual Host camera/group/publication methods; runtime errors retain owner while startup failure falls back; legacy gap reproduced;1000 deferred camera commits and invalid proposals remain complete under concurrent observation");
}
'''
with tempfile.TemporaryDirectory(prefix='render-atomic-camera-') as tmp:
    cpp,exe=Path(tmp)/'atomic.cpp',Path(tmp)/'atomic'
    cpp.write_text(fixture+methods+observers,'utf-8')
    subprocess.run(['g++','-std=c++17','-O2','-pthread',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
