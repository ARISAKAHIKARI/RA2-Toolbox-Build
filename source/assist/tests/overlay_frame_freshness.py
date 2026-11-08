#!/usr/bin/env python3
"""Execute verbatim paint gates with real TacticalSampler and Motion; no GDI run."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[3]);p.add_argument('--main',type=Path);p.add_argument('--policy',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--expect-fixed',action='store_true');a=p.parse_args()
 root=a.repo.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 source=a.main or root/'source/assist/source/main.cpp';policy=a.policy or root/'source/assist/source/display_sample_lease.h'
 fixture=Path(__file__).with_suffix('.cpp');enemy=root/'source/assist/tests/enemy_markers.cpp'
 inputs=[source,policy,fixture,enemy,root/'source/external_reader/source/tactical_sampler.h',root/'source/assist/source/label_motion.h',root/'source/external_reader/source/unit_status.h',root/'source/external_reader/source/enemy_markers.h']
 hashes={str(path):sha(path) for path in inputs}
 lines=[]
 for name in ['statusCount','rawEnemyCount']:
  matches=[line.strip() for line in source.read_text().splitlines() if re.match(r'int '+name+r'=',line.strip())]
  if len(matches)!=1:raise SystemExit('Production paint gate marker changed: '+name)
  lines.append(matches[0])
 gates=out/'paint-gates.inc';gates.write_text('\n'.join(lines)+'\n')
 exe=out/'fixture';cmd=['g++','-std=c++17','-O2','-UNDEBUG','-DENEMY_FIXTURE="'+str(enemy)+'"','-DDISPLAY_POLICY="'+str(policy)+'"','-DPAINT_GATES="'+str(gates)+'"',str(fixture),'-o',str(exe)]
 build=subprocess.run(cmd,text=True,capture_output=True);(out/'build.log').write_text(build.stdout+build.stderr)
 if build.returncode:raise SystemExit(build.stdout+build.stderr)
 run_cmd=[str(exe)]+(['--fixed'] if a.expect_fixed else []);run=subprocess.run(run_cmd,text=True,capture_output=True);(out/'run.log').write_text(run.stdout+run.stderr)
 if run.returncode:raise SystemExit(run.stdout+run.stderr or 'fixture failed')
 result=json.loads(run.stdout);stable=all(sha(path)==hashes[str(path)] for path in inputs)
 receipt=dict(passed=stable and result['failures']==0,expected_fixed=a.expect_fixed,result=result,source_sha256=hashes,source_hash_stable=stable,extracted_paint_statements=lines,build_command=cmd,run_command=run_cmd,build_exit_code=build.returncode,run_exit_code=run.returncode,windows_executed=False,game_tested=False,multiplayer_tested=False,scope='Verbatim production paint count statements, real TacticalSampler/Motion, controlled memory; no Windows/GDI display test')
 (out/'result.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result));raise SystemExit(not receipt['passed'])
if __name__=='__main__':main()
