#!/usr/bin/env python3
"""Compare actual AntiTheft/planner collectors on sealed synthetic snapshots."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from tactical_frame_sampling import function


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--baseline',default='e5e9abd69cdf2963f7bef3dd8fbc3e12c4537a63')
    p.add_argument('--cxx',default='g++')
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();here=Path(__file__).resolve().parent;src=here.parent/'source';root=here.parents[2];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    fixture=(here/'tactical_frame_sampling.cpp').read_text().split('#include "tactical_frame_production.h"')[0]
    fixture=fixture.replace('#include "sample_scope_production.h"','static bool countWork=false;static uint64_t sampleReads=0,buildingChecks=0;\nstatic std::function<void(uint32_t,size_t)> readMutation;\n#include "sample_scope_production.h"')
    features=(src/'feature_type_bridge.h').read_text().split('static bool featureTypeAllowed(')[0]
    fixture=fixture.replace('static bool refreshFeatureTypes(){return true;}static long featureConfigRevision=0;','static void MemoryBarrier();\n'+features)
    fixture=fixture.replace('static bool antiTheftBuilding(uint32_t){return false;}static bool antiTheftThreat(uint32_t,uint32_t){return false;}',
        '#define antiTheftBuilding actualAntiTheftBuilding\n#include "anti_theft_bridge.h"\n#undef antiTheftBuilding\nstatic bool antiTheftBuilding(uint32_t p){if(countWork)++buildingChecks;return actualAntiTheftBuilding(p);}')
    scope=(src/'world_frame_runtime.h').read_text()
    scope='static worldframe::DemandLeases runtimeDemandLeases;\n'+'\n'.join(function(scope,s) for s in ('static void requestWorldRange(', 'static bool readWorldSample(', 'class SampleReadScope'))+';\n'
    scope=scope.replace('static bool readWorldSample(', 'static bool readWorldSampleRaw(')
    scope+='\nstatic bool readWorldSample(uint32_t at,void*out,size_t n){if(countWork)++sampleReads;const bool ok=readWorldSampleRaw(at,out,n);if(readMutation){auto mutation=readMutation;mutation(at,n);}return ok;}\n'
    (out/'sample_scope_production.h').write_text(scope)
    sources=[src/n for n in ('external_orders_bridge.h','anti_theft_sample_bridge.h','anti_theft_bridge.h','feature_type_bridge.h','feature_type_config.h','world_frame_runtime.h','tactical_frame_read.h','raw_combat.h','combat_query_bridge.h','combat_probe_bridge.h','planner_world_copy.h','harvester_defense_raw.h','protocol.h')]
    sources += [root/'source/shared/source/world_frame.h',here/'tactical_frame_sampling.cpp',here/'tactical_frame_sampling.py',here/'anti_theft_sample_cache.cpp',Path(__file__)]
    hashes={str(p.relative_to(root)):sha(p) for p in sources}
    baseline=subprocess.check_output(['git','-C',str(root),'show',a.baseline+':source/assist/source/external_orders_bridge.h'],text=True)
    (out/'baseline-external-orders.h').write_text(baseline)
    results={}
    for name,orders in [('baseline',baseline),('current',(src/'external_orders_bridge.h').read_text())]:
        directory=out/name;directory.mkdir(exist_ok=True)
        parts=[function(orders,s) for s in ('static bool oreTypeAllowed(', 'static bool oreMiner(', 'static bool rawOreCompatible(', 'static bool rawDockBusy(', 'static bool rawDockLinked(')]
        parts+=['#include "raw_combat.h"']
        raw=(src/'harvester_defense_raw.h').read_text()
        parts += [function(raw,s) for s in ('static bool minerDefenseMiner(', 'static bool minerDefenseDock(')]
        parts+=['static bool dodgeOwns(uint32_t){return false;}\n#include "combat_query_bridge.h"\n#include "combat_probe_bridge.h"\n#include "planner_world_copy.h"']
        if name=='current':parts+=['#include "anti_theft_sample_bridge.h"']
        parts+=['static planner::World planningWorld,planningCapture;static int planningFrame=-1;static uint32_t planningSerial,inputSerial;',function(orders,'static bool publishPlanningWorld(')]
        route='\n\n'.join(parts)+'\n';(directory/'production.h').write_text(route)
        cpp=directory/'fixture.cpp';cpp.write_text(fixture+'\n#include "production.h"\n#include "anti_theft_sample_cache.cpp"\n')
        exe=directory/'check'
        command=[a.cxx,'-std=c++17','-O2','-UNDEBUG','-I',str(directory),'-I',str(out),'-I',str(src),'-I',str(here),'-DANTI_THEFT_BASELINE='+('1' if name=='baseline' else '0'),str(cpp),'-o',str(exe)]
        if a.sanitize:command[2:2]=['-g','-fsanitize=undefined','-fno-sanitize-recover=all']
        build=subprocess.run(command,text=True,capture_output=True);(directory/'build.log').write_text(build.stdout+build.stderr)
        if build.returncode:raise SystemExit(build.stdout+build.stderr)
        run=subprocess.run([str(exe)],text=True,capture_output=True,timeout=60);(directory/'run.log').write_text(run.stdout+run.stderr)
        if run.returncode:raise SystemExit(name+':\n'+run.stdout+run.stderr)
        records=[(line.split(' ',1)[0],json.loads(line.split(' ',1)[1])) for line in run.stdout.splitlines()]
        results[name]={'build_command':command,'build_exit_code':build.returncode,'run_exit_code':run.returncode,'benchmarks':[r for k,r in records if k=='BENCH'],'result':next(r for k,r in records if k=='RESULT'),'production_routes_sha256':sha(directory/'production.h'),'fixture_sha256':sha(cpp),'executable_sha256':sha(exe)}
    for old,new in zip(results['baseline']['benchmarks'],results['current']['benchmarks']):
        assert old['active_world_digest']==new['active_world_digest']
        assert old['buildings']==new['buildings'] and old['positive']==new['positive']
        assert not old['live_reads'] and not new['live_reads']
    assert all(sha(p)==hashes[str(p.relative_to(root))] for p in sources),'Source changed during verification'
    report={'schema':'ra2.anti-theft-sealed-presence-cache.v1','pass':True,'baseline_commit':a.baseline,'source_sha256':hashes,'results':results,'active_world_digests_equal':True,'undefined_behavior_sanitizer':a.sanitize,'windows_executed':False,'game_tested':False,'multiplayer_tested':False,'scope':['Actual production publishPlanningWorld/AntiTheft predicates/feature refresh and raw scope execute with synthetic memory and explicit native seams; the prior tactical fixture supplies the boundaries.','Full Key fields, own-house argument, stable feature config, incomplete/malformed/empty roster, prefix-positive proof, incomplete later projection, new-token dependency renewal and scoped same-frame invalidation execute.','Only operation counters and read-change injection wrap the real sealed read; no live fallback is supplied.'],'limits':['Elapsed time measures portable C++ fixture CPU work, not Windows ReadProcessMemory, game FPS or a native integration run.','The baseline is the exact fixed-commit publisher; shared helper headers are current unchanged semantics for this focused AntiTheft route.']}
    (out/'receipt.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'pass':True,'current':results['current']['result'],'baseline_benchmarks':results['baseline']['benchmarks'],'current_benchmarks':results['current']['benchmarks']},ensure_ascii=False,indent=2))


if __name__=='__main__':main()
