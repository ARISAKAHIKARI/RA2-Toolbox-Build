#!/usr/bin/env python3
"""Execute actual Broker/Host lifecycle routes, optionally reproducing old blocking waits."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from runtime_broker_retirement import function

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--baseline-client',type=Path)
    parser.add_argument('--baseline-host',type=Path)
    parser.add_argument('--cxx',default='c++')
    args=parser.parse_args()
    if bool(args.baseline_client)!=bool(args.baseline_host):parser.error('both baseline paths are required together')
    here=Path(__file__).resolve().parent;source=here.parents[1]
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    shim=out/'include';shim.mkdir(exist_ok=True)
    (shim/'windows.h').write_text('#pragma once\n#include "runtime_broker_retirement_fixture.h"\n')
    client=source/'shared/source/runtime_broker_client.h';host=source/'shared/source/host_game_input.h'
    inputs=[client,host,source/'shared/source/runtime_broker_protocol.h',here/'runtime_broker_retirement.cpp',here/'runtime_broker_retirement_fixture.h',here/'runtime_broker_lifecycle_audit.cpp',Path(__file__)]
    hashes={str(p.relative_to(source.parent)):sha(p) for p in inputs}
    variants=[('current',client,host)]
    if args.baseline_client:variants.insert(0,('baseline',args.baseline_client.resolve(),args.baseline_host.resolve()))
    results={}
    for name,current_client,current_host in variants:
        routes=out/(name+'-host.inc');attach=out/(name+'-attach.inc');body=current_host.read_text()
        routes.write_text('\n\n'.join(function(body,n) for n in ('inline bool clear(', 'inline void reapTargets(', 'inline void CALLBACK pulse('))+'\n')
        attach.write_text('\n\n'.join(function(body,n) for n in ('inline bool rebindWindow(', 'inline Subscription install(', 'inline bool release('))+'\n')
        exe=out/(name+'-broker-lifecycle')
        command=[args.cxx,'-std=c++17','-O0','-pthread','-D_WIN64','-I'+str(shim),'-I'+str(here),'-I'+str(source/'shared/source'),'-DBROKER_CLIENT="'+str(current_client)+'"','-DBROKER_HOST="'+str(routes)+'"','-DBROKER_ATTACH="'+str(attach)+'"',str(here/'runtime_broker_lifecycle_audit.cpp'),'-o',str(exe)]
        if name=='baseline':command.append('-DBROKER_BASELINE')
        build=subprocess.run(command,text=True,capture_output=True);(out/(name+'-build.log')).write_text(build.stdout+build.stderr)
        if build.returncode:raise SystemExit(build.stdout+build.stderr)
        run=subprocess.run([str(exe)],text=True,capture_output=True);(out/(name+'-run.log')).write_text(run.stdout+run.stderr)
        if run.returncode:raise SystemExit(run.stdout+run.stderr)
        result=json.loads(run.stdout.strip().splitlines()[-1]);print(name+': '+json.dumps(result))
        results[name]={'pass':result['failures']==0,'result':result,'build_command':command,'build_exit_code':build.returncode,'run_exit_code':run.returncode,'client_sha256':sha(current_client),'host_sha256':sha(current_host),'production_routes_sha256':sha(routes),'production_attach_sha256':sha(attach),'executable_sha256':sha(exe)}
    if any(sha(p)!=hashes[str(p.relative_to(source.parent))] for p in inputs):raise SystemExit('Audit inputs changed during execution')
    receipt={'schema':'ra2.broker-lifecycle-lock-audit.v2','pass':all(r['pass'] for r in results.values()),'results':results,'source_sha256':hashes,'windows_executed':False,'game_tested':False,'multiplayer_tested':False,'production_changed':True,'limits':['Complete production Client and extracted production Host install/release/rebind/clear/reap/pulse execute against controlled Win32 IPC/process boundaries.','The 2990ms startup and 1990ms ACK latencies are supplied virtual time, not real elapsed time or FPS. A second actual C++ thread probes the real Host mutex.','Zero-wait polling removes explicit timeout waits. CreateProcess, handle/mapping APIs and TerminateProcess remain synchronous OS calls; no absolute wall-time upper bound is claimed.','Helper hook installation and Windows/game execution are not exercised in this controlled test.']}
    (out/'receipt.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n')
    print('PASS current lifecycle checks='+str(results['current']['result']['checks'])+'; baseline checks reproduce the old defect and are not counted as fixed checks.')

if __name__=='__main__':main()
