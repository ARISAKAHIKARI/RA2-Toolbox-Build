"""Run the production watch host with its real worker and controlled read delays.
The memory/assets/GDI boundary doubles do not prove actual Windows composition.
"""
from pathlib import Path
import subprocess,tempfile
from watch_windows_test import root,prefix

scenario=r'''
static std::shared_ptr<const watchhost::Result> awaitResult(uint32_t control,const std::shared_ptr<const watchhost::Result>&previous={}){
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(std::chrono::steady_clock::now()<deadline){auto r=watchhost::worker.get();if(r&&r->control==control&&r!=previous)return r;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 return {};
}
int main(){
 auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 auto vec=[&](uint32_t p,uint32_t storage,const std::vector<uint32_t>&v){put(p+4,storage);put(p+8,int(v.size()));put(p+16,int(v.size()));memcpy(memory.data()+storage,v.data(),v.size()*4);};
 put(0xA8E378,uint8_t(1));put(external::radar::Map+0xF4,32);put(external::radar::Map+0xF8,64);put(external::radar::Map+0x104,32);put(external::radar::Map+0x108,64);
 uint32_t cells=0x200000;put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);
 std::vector<uint32_t> actors;
 for(int i=0;i<3;++i){uint32_t p=0x10000+i*0x1000,t=0x800000+i*0x2000,c=0x400000+i*0x150;int x=30+i,y=30;actors.push_back(p);
  put(p,0x7F5C70u);put(p+0x10,uint32_t(i+1));put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x6C4,t);put(p+0x21C,0x180000u);put(p+0x9C,x*256+128);put(p+0xA0,y*256+128);put(t+0xB0,1u);put(t+0xCA1,uint8_t(1));
  put(cells+(y*512+x)*4,c);put(c+0x24,int16_t(x));put(c+0x26,int16_t(y));put(c+0x120,int8_t(-1));put(c+0x121,int8_t(-1));
  watchhost::tracks[i].target={p,uint32_t(i+1)};
 }
 vec(0xA8EC78,0x190000,actors);watchhost::process=pid;watchhost::epoch=1;watchhost::controls=1;
 watchhost::update(clockValue);auto nativeResult=awaitResult(watchhost::controls);assert(nativeResult);for(const auto&v:nativeResult->views)assert(v.pixels&&v.pixels->size()==640*512);
 // A NEW completion can still contain older lifecycle evidence: stop a job
 // after sampling/drawing, confirm death on the foreground path, then let
 // that old job finish. Pointer-based one-time consumption alone cannot
 // reject it; the foreground identity change must invalidate its epoch.
 watchtest::holdFresh();auto beforeForeground=watchhost::worker.get();
 watchhost::update(clockValue+=1);assert(watchtest::awaitFresh());
 put(actors[0]+0x6C,0);auto freshFollow=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};
 auto oldControl=watchhost::controls;assert(watchhost::followTracks(freshFollow,0));
 assert(watchhost::controls==oldControl+1&&!watchhost::tracks[0]&&!watchhost::views[0].shown&&!watchhost::worker.get());
 watchtest::hold();watchhost::update(clockValue+=1);watchtest::releaseFresh();assert(watchtest::awaitEntry());
 assert(!watchhost::worker.get()&&!watchhost::tracks[0]); // obsolete job was discarded, replacement is still blocked
 watchtest::release();auto afterForeground=awaitResult(watchhost::controls,beforeForeground);
 assert(afterForeground&&!afterForeground->tracks[0]&&!afterForeground->views[0].shown&&afterForeground->views[1].shown&&afterForeground->views[2].shown);
 // Restore the slot through an explicit user pin, preserving the remainder
 // of the delayed-worker regression below.
 put(actors[0]+0x6C,100);vec(0xA8ECB8,0x191000,{actors[0]});watchhost::pin(extra.watchKeys[0]);
 watchhost::update(clockValue+=1);assert(awaitResult(watchhost::controls));
 watchtest::hold();watchhost::update(clockValue+=10);assert(watchtest::awaitEntry());
 external::watch::Box boxes[3];for(int i=0;i<3;++i){assert(watchhost::views[i].shown);boxes[i]=watchhost::views[i].box;}
 // A boundary trigger can fire after cached sampling, without a control or
 // simulation-frame change. Stop the real worker at the uncached publication
 // read, change VisibleRect, and ensure its old-domain batch is never released.
 auto beforeBounds=watchhost::worker.get();auto sameControl=watchhost::controls;watchtest::holdFresh();watchtest::release();assert(watchtest::awaitFresh());
 put(external::radar::Map+0x108,40);watchtest::hold();watchhost::update(clockValue+=1);watchtest::releaseFresh();assert(watchtest::awaitEntry());
 assert(watchhost::controls==sameControl&&watchhost::worker.get()==beforeBounds);
 for(int i=0;i<3;++i){assert(watchhost::views[i].shown&&watchhost::views[i].pixels==beforeBounds->views[i].pixels);auto b=watchhost::views[i].box,s=boxes[i];assert(b.x==s.x&&b.y==s.y&&b.w==s.w&&b.h==s.h);}
 watchtest::release();auto afterBounds=awaitResult(sameControl,beforeBounds);assert(afterBounds);
 for(const auto&v:afterBounds->views)assert(v.shown&&v.frame.focus&&v.frame.bounds.rows==40);
 watchtest::hold();watchhost::update(clockValue+=1);assert(watchtest::awaitEntry());
 // The real background worker is blocked at its external reader. UI updates
 // remain nonblocking, and even a >500 ms render gap cannot change occupancy.
 for(int n=0;n<12;++n){watchhost::update(clockValue+=100);for(int i=0;i<3;++i){auto b=watchhost::views[i].box,s=boxes[i];assert(watchhost::views[i].shown&&b.x==s.x&&b.y==s.y&&b.w==s.w&&b.h==s.h);}}
 // Foreground navigation can confirm a dead target before the renderer catches
 // up. Re-reading the same completed result must not restore that old track.
 put(actors[0]+0x6C,0);auto read=[](uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);};external::watch::follow(read,watchhost::tracks[0]);assert(!watchhost::tracks[0]);
 watchhost::update(clockValue+=1);assert(!watchhost::tracks[0]);
 // A close while rendering invalidates both the running completion and its
 // queued obsolete snapshot. Only the remaining third slot may return.
 watchhost::close(1);watchhost::update(clockValue+=1);assert(!watchhost::tracks[1]&&!watchhost::views[1].shown);
 watchtest::release();auto final=awaitResult(watchhost::controls);assert(final&&!final->views[0].shown&&!final->views[1].shown&&final->views[2].shown);
 watchtest::hold();watchhost::update(clockValue+=1);assert(watchtest::awaitEntry());
 assert(!watchhost::tracks[0]&&!watchhost::tracks[1]&&watchhost::tracks[2]);assert(!watchhost::views[0].shown&&!watchhost::views[1].shown&&watchhost::views[2].shown);
 // Switching the auto-tracking setting is also a control change. Even when
 // rapid toggles replace pending jobs, the worker must receive the reset.
 extra.watchDanger=true;watchhost::update(clockValue+=1);extra.watchDanger=false;watchhost::update(clockValue+=1);extra.watchDanger=true;watchhost::update(clockValue+=1);
 watchtest::release();auto resumed=awaitResult(watchhost::controls);assert(resumed&&!resumed->suppressed);
 watchtest::release();
 // Display lifetime follows the reader's stable world identity, independent
 // of the bridge command namespace and primary overlay/master toggles.
 extra.watchSecondary=true;extra.watchEnabled=false;enabled=false;focused=false;snap.chat=1;secondAvailable=true;
 auto secondaryEpoch=watchhost::epoch,secondaryControl=watchhost::controls;auto secondaryTrack=watchhost::tracks[2].target;++commandEpoch;
 watchhost::update(clockValue+=1);assert(watchhost::epoch==secondaryEpoch&&watchhost::tracks[2].target==secondaryTrack);
 auto secondaryFrame=awaitResult(watchhost::controls);assert(secondaryFrame&&secondaryFrame->views[2].shown);
 secondAvailable=false;watchhost::update(clockValue+=1);assert(watchhost::suspended&&!watchhost::views[2].shown&&watchhost::tracks[2].target==secondaryTrack&&!watchhost::worker.get());
 auto idleControls=watchhost::controls;watchhost::suspend();assert(watchhost::controls==idleControls);
 secondAvailable=true;watchhost::update(clockValue+=1);assert(!watchhost::suspended&&watchhost::epoch==secondaryEpoch&&watchhost::tracks[2].target==secondaryTrack);assert(awaitResult(watchhost::controls));
 watchhost::shutdown();
 std::cout<<"PASS real async host: foreground-confirmed death rejects newly completed stale jobs, uncached boundary publication gate with unchanged control/frame, stable three-window layout during delayed renders, one-time result consumption, death and close while a job is blocked, obsolete completion rejection, auto-tracking toggle reset\n";
}
'''

with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'watch_async.cpp';exe=Path(tmp)/'watch_async';cpp.write_text(prefix+scenario)
 subprocess.run(['g++','-std=c++17','-O1','-pthread','-I'+str(root/'assist/tests'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=20)
