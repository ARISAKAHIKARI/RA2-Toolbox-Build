#!/usr/bin/env python3
"""Source-bound lock-scope audit of broker recovery; no real Windows/game run."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from runtime_broker_retirement import function

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--cxx',default='c++')
    args=parser.parse_args()
    here=Path(__file__).resolve().parent
    source=here.parents[1]
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    shim=out/'include';shim.mkdir(exist_ok=True)
    (shim/'windows.h').write_text('#pragma once\n#include "runtime_broker_retirement_fixture.h"\n')
    client=source/'shared/source/runtime_broker_client.h'
    host=source/'shared/source/host_game_input.h'
    inputs=[client,host,source/'shared/source/runtime_broker_protocol.h',here/'runtime_broker_retirement.cpp',here/'runtime_broker_retirement_fixture.h',here/'runtime_broker_lifecycle_audit.cpp',Path(__file__)]
    hashes={str(p.relative_to(source.parent)):sha(p) for p in inputs}
    routes=out/'production-host.inc'
    routes.write_text('\n\n'.join(function(host.read_text(),name) for name in ('inline bool clear(', 'inline void reapTargets(', 'inline void CALLBACK pulse('))+'\n')
    exe=out/'broker-lifecycle-audit'
    command=[args.cxx,'-std=c++17','-O0','-pthread','-D_WIN64','-I'+str(shim),'-I'+str(here),'-I'+str(source/'shared/source'),'-DBROKER_CLIENT="'+str(client)+'"','-DBROKER_HOST="'+str(routes)+'"',str(here/'runtime_broker_lifecycle_audit.cpp'),'-o',str(exe)]
    build=subprocess.run(command,text=True,capture_output=True)
    (out/'build.log').write_text(build.stdout+build.stderr)
    if build.returncode:raise SystemExit(build.stdout+build.stderr)
    run=subprocess.run([str(exe)],text=True,capture_output=True)
    (out/'run.log').write_text(run.stdout+run.stderr)
    if run.returncode:raise SystemExit(run.stdout+run.stderr)
    result=json.loads(run.stdout.strip().splitlines()[-1])
    if any(sha(p)!=hashes[str(p.relative_to(source.parent))] for p in inputs):
        raise SystemExit('Audit inputs changed during execution')
    receipt={'schema':'ra2.broker-lifecycle-lock-audit.v1','pass':result['failures']==0,'result':result,'build_command':command,'build_exit_code':build.returncode,'run_exit_code':run.returncode,'source_sha256':hashes,'generated_production_routes_sha256':sha(routes),'executable_sha256':sha(exe),'windows_executed':False,'game_tested':False,'multiplayer_tested':False,'production_changed':False,'limits':['Win32 process/IPC waits use deterministic virtual time; 4980ms is supplied test latency, not a real elapsed-time or FPS measurement.','A second actual C++ thread probes the production host mutex at each wait boundary.','This diagnoses lifecycle serialization; it does not prove ordinary healthy game pulses stall or deadlock.']}
    (out/'receipt.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'pass':receipt['pass'],**result},ensure_ascii=False))

if __name__=='__main__':main()
