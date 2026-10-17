#!/usr/bin/env python3
from pathlib import Path
import json,os,platform,subprocess,tempfile
r=Path(__file__).resolve().parents[1];out=r/'verification/extended';out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ra2-extended-') as tmp:
 exe=Path(tmp)/'robust';subprocess.run(['g++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(r/'assist/tests/extended_robustness.cpp'),'-o',str(exe)],check=True)
 run=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=120)
 d=json.loads(run.stdout);d['scope']='production pure algorithms under ASan/UBSan; deterministic fuzz; no Windows/game';d['leak_sanitizer']='disabled: container thread enumeration unsupported';(out/'robustness-stress.json').write_text(json.dumps(d,indent=2)+'\n');print('PASS robustness stress',d['orders_checked'],flush=True)
 exe=Path(tmp)/'bench';subprocess.run(['g++','-std=c++17','-O2',str(r/'assist/tests/latency_bench.cpp'),'-o',str(exe)],check=True)
 run=subprocess.run([str(exe)],capture_output=True,text=True,check=True,timeout=120);d=json.loads(run.stdout);d['environment']={'platform':platform.platform(),'machine':platform.machine(),'cpu_count':os.cpu_count(),'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0]};(out/'latency-cpu.json').write_text(json.dumps(d,indent=2)+'\n');print('PASS',len(d['results']),'CPU latency workloads',flush=True)
