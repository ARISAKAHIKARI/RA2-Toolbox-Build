#!/usr/bin/env python3
"""Execute production cooperative admission across the read-only fence.

Uses exact current Runtime consumer/reset bodies, Shared layout, lifecycle,
allocator and freshness helpers. Game reads/capture/event admission are controlled
boundaries; this is not Windows/game execution. --negative-control uses verified
pre-cancellation helper bytes and the same current consumer. No git object needed.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess

FIXTURE = Path(__file__).resolve().parent / 'fixtures/read_only_pre_cooperative/read_only_lifecycle.h'
FIXTURE_SHA = '8bb040dd7ee9081d1ed0b1fbc09bf36445a173a22e19dd45d9934cb228281d87'
EXPECTED = [
 'FAIL completed old cooperative click must not be replayed after pause/resume',
 'FAIL unconsumed old cooperative click must be cancelled by pause',
]

def run(ROOT, OUT, old_header=None):
    OUT.mkdir(parents=True, exist_ok=True)
    SNAP = OUT / 'snapshot'
    hashes = {}

    def freeze(path):
        path = path.resolve()
        rel = path.relative_to(ROOT)
        if str(rel) in hashes:
            return
        data = old_header if old_header is not None and str(rel) == "source/assist/source/read_only_lifecycle.h" else path.read_bytes()
        hashes[str(rel)] = hashlib.sha256(data).hexdigest()
        dest = SNAP / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        for ref in re.findall(rb'^\s*#\s*include\s*"([^"]+)"', data, re.M):
            child = path.parent / ref.decode()
            if child.is_file():
                freeze(child)

    for rel in ['source/assist/source/protocol.h', 'source/assist/source/cooperative_plan.h']:
        freeze(ROOT / rel)
    consumer_path = ROOT / 'source/assist/source/cooperative_bridge.h'
    consumer_bytes = consumer_path.read_bytes()
    hashes['source/assist/source/cooperative_bridge.h'] = hashlib.sha256(consumer_bytes).hexdigest()
    (OUT / 'cooperative_bridge.source').write_bytes(consumer_bytes)
    consumer = consumer_bytes.decode().replace('\r\n', '\n')
    start = consumer.index('static std::vector<cooperative::Assignment> runtimeCoopOrders')
    end = consumer.index('// Only the common game-thread capture epoch', start)
    body = consumer[start:end]
    (OUT / 'extracted-consumer.cpp').write_text(body)

    cpp = r'''
    #include "source/assist/source/protocol.h"
    #include "source/assist/source/cooperative_plan.h"
    #include <cstring>
    #include <memory>
    #include <iostream>
    using namespace assist;
    using DWORD=uint32_t;
    static auto state=std::make_unique<Shared>();
    static Shared* sh=state.get();
    static uint32_t match=7;
    static DWORD observedNow=1008;
    static int observedFrame=100;
    static unsigned submissions=0,collects=0,failures=0,checks=0;
    static void MemoryBarrier(){}
    template<class T>static T get(uint32_t a){return a==0xA8ED84?T(observedFrame):T(0);}
    static DWORD sampleClock(Shared*){return observedNow;}
    static int planningRoom(int){return 8;}
    static int outgoingRoom(){return 8;}
    static void notifyHost(){}
    static int admitCooperative(const cooperative::Assignment&,int,uint32_t,int,DWORD){++submissions;return 1;}
    static bool collectCooperativeInput(const cooperative::Request&r,const compute::QuotaConfig&,cooperative::Input&out){
     ++collects;out.mode=r.mode;out.clicked=r.target;
     out.units.push_back({r.sources[0],10,0,0,1});
     out.targets.push_back({r.target,r.type,0,0,1,0,{}});out.compatible.push_back(1);return true;
    }
    ''' + body + r'''
    static void check(bool v,const char*text){++checks;if(!v){++failures;std::cout<<"FAIL "<<text<<'\n';}}
    static void init(){
     state=std::make_unique<Shared>();sh=state.get();resetRuntimeCooperative();submissions=collects=0;
     sh->computeApplied=compute::All;sh->flags=AdvancedEnter;
     auto&r=sh->cooperativeRequest;r.serial=5;r.match=match;r.tick=1000;r.target=20;r.type=30;
     r.frame=100;r.mode=cooperative::Enter;r.count=1;r.viewWidth=1024;r.viewHeight=768;r.sources[0]={10,0};
     observedNow=1008;observedFrame=100;
    }
    static void pauseResume(){
     readonly::RuntimeState runtime;
     readonly::request(*sh,MemoryBarrier);
     // Production bridge::reset calls resetCooperativeState, which directly
     // calls this exact resetRuntimeCooperative; unrelated teardown is outside
     // this fixture. RuntimeState and cancelMailboxes are actual headers.
     auto retire=[](){resetRuntimeCooperative();return true;};
     auto cancel=[](){readonly::cancelMailboxes(*sh,MemoryBarrier);};
     check(runtime.service(*sh,cancel,retire,MemoryBarrier)==readonly::Admission::Observe,"retirement reaches observation");
     check(readonly::settled(*sh),"retirement acknowledged before resume");
     sh->readOnly=0;
     check(runtime.service(*sh,cancel,retire,MemoryBarrier)==readonly::Admission::Commands,"ordinary admission restored");
     observedNow=1100;observedFrame=101;
    }
    int main(){
     init();runtimeCooperativeTick(0x10000,observedFrame,observedNow);
     check(submissions==1&&sh->runtimeCoopDone==5,"normal request accepted once");
     runtimeCooperativeTick(0x10000,observedFrame,observedNow);
     check(submissions==1,"unchanged request does not repeat without retirement");
     pauseResume();runtimeCooperativeTick(0x10000,observedFrame,observedNow);
     std::cout<<"OBSERVED completed-request submissions-after-resume="<<submissions<<" shared-serial="<<sh->cooperativeRequest.serial<<" done="<<sh->runtimeCoopDone<<'\n';
     check(submissions==1,"completed old cooperative click must not be replayed after pause/resume");
     init();const auto freshRequest=sh->cooperativeRequest;pauseResume();runtimeCooperativeTick(0x10000,observedFrame,observedNow);
     std::cout<<"OBSERVED unconsumed-request submissions-after-resume="<<submissions<<" shared-serial="<<sh->cooperativeRequest.serial<<'\n';
     check(submissions==0,"unconsumed old cooperative click must be cancelled by pause");
     sh->cooperativeRequest=freshRequest;sh->cooperativeRequest.serial=6;sh->cooperativeRequest.tick=observedNow;
     runtimeCooperativeTick(0x10000,observedFrame,observedNow);
     check(sh->runtimeCoopDone==6,"new explicit click after resume remains admissible");
     std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"submissions\":"<<submissions<<"}\n";
     return failures?1:0;
    }
    '''
    (OUT / 'production.cpp').write_text(cpp)
    cmd = ['g++', '-std=c++17', '-O0', '-I', str(SNAP), str(OUT/'production.cpp'), '-o', str(OUT/'production')]
    compiled = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUT/'compile.log').write_text(compiled.stdout)
    report = {'scope':'Production readonly cancellation/retirement and cooperative runtime consumer under controlled game-read, capture, and event-admission boundaries. Does not prove Windows/game execution.',
              'sources':hashes, 'extracted_consumer_sha256':hashlib.sha256(body.encode()).hexdigest(),
              'generated_cpp_sha256':hashlib.sha256(cpp.encode()).hexdigest(), 'compile_argv':cmd, 'compile_exit':compiled.returncode}
    if compiled.returncode == 0:
        ran = subprocess.run([str(OUT/'production')], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (OUT/'run.log').write_text(ran.stdout)
        report.update(run_exit=ran.returncode, execution=json.loads(ran.stdout.splitlines()[-1]), raw_run_sha256=hashlib.sha256(ran.stdout.encode()).hexdigest())

    (OUT/'result.json').write_text(json.dumps(report, indent=2)+'\n')

    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--negative-control',action='store_true')
    args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    current=run(args.root.resolve(),args.out/'current')
    report={'schema':1,'suite':'cooperative_read_only_recovery','scope':current['scope'],
            'current':current,'sources':dict(current['sources'])}
    report['passed']=current['compile_exit']==0 and current.get('run_exit')==0 and current.get('execution',{}).get('checks')==11 and current['execution']['failures']==0
    if args.negative_control:
        data=FIXTURE.read_bytes()
        if hashlib.sha256(data).hexdigest()!=FIXTURE_SHA:
            raise SystemExit('Pre-cancellation fixture SHA-256 mismatch')
        negative=run(args.root.resolve(),args.out/'negative-control',data)
        log=(args.out/'negative-control/run.log').read_text() if negative['compile_exit']==0 else ''
        observed=[line for line in log.splitlines() if line.startswith('FAIL ')]
        ok=negative['compile_exit']==0 and negative.get('run_exit')==1 and negative.get('execution',{}).get('checks')==11 and negative['execution']['failures']==2 and observed==EXPECTED
        report['negative_control']={'passed':ok,'fixture_sha256':FIXTURE_SHA,'fixture_provenance':'Public read-only helper captured before cooperative request retirement was added, 2026-11-09; not a released pre.5 source.', 'expected_failures':EXPECTED,'observed_failures':observed,'result':negative}
        report['sources'][str(FIXTURE.relative_to(args.root.resolve()))]=FIXTURE_SHA
        report['passed']=report['passed'] and ok
    script=Path(__file__).resolve()
    report['sources'][str(script.relative_to(args.root.resolve()))]=hashlib.sha256(script.read_bytes()).hexdigest()
    (args.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'suite':report['suite'],'passed':report['passed'],'checks':current.get('execution',{}).get('checks'),'failures':current.get('execution',{}).get('failures'),'negative_control':report.get('negative_control',{}).get('passed')}))
    return 0 if report['passed'] else 1

if __name__=='__main__':
    raise SystemExit(main())
