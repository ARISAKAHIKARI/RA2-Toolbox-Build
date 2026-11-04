#!/usr/bin/env python3
"""Compare production manager allowlist with actual ordinary Spawner tables.

The bundled read-only PE table parser accepts the fixed official ordinary DLL.
No DLL or game is loaded. The tiny C++ helper evaluates production size() rather than
maintaining a second manually copied allowlist.
"""
import argparse,hashlib,json,re,subprocess,tempfile
from ordinary_pe import PE,ORDINARY_SHA256,PRODUCT_CODE_SHA,PRODUCT_RELEASE_SHA,UPSTREAM_SHA,RELEASE_RUNTIME_SHA256,intersects,coverage_method,validate_upstream
from pathlib import Path

EXPECTED='58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--ordinary-dll',required=True,type=Path);p.add_argument('--upstream-source',type=Path);p.add_argument('--output-dir',required=True,type=Path);a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
 assert sha(a.ordinary_dll)==EXPECTED,'Unexpected ordinary DLL'
 source_pin=validate_upstream(a.upstream_source) if a.upstream_source is not None else None
 policy=a.repo/'source/shared/source/experimental_patch_policy.h';direct=a.repo/'source/shared/source/direct_patch_policy.h'
 text=policy.read_text();names={int(v,16):k for k,v in re.findall(r'\b(\w+)\s*=\s*(0x[0-9A-Fa-f]+)',text) if int(v,16)>=0x400000}
 for v in re.findall(r'^\s*\{(0x[0-9A-Fa-f]+),Group::',direct.read_text(),re.M):names.setdefault(int(v,16),'Direct'+v)
 candidates=sorted({int(v,16) for v in re.findall(r'0x[0-9A-Fa-f]+',text+direct.read_text()) if int(v,16)>=0x400000})
 source='#include <cstdio>\n#include '+json.dumps(str(policy.resolve()))+'\nint main(){unsigned a[]={'+','.join(hex(x) for x in candidates)+'};for(auto at:a)if(auto n=experimentalpatch::size(at))std::printf("%u %u\\n",at,n);}\n'
 with tempfile.TemporaryDirectory() as t:
  folder=Path(t);cpp=folder/'inventory.cpp';exe=folder/'inventory';cpp.write_text(source)
  subprocess.run(['g++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
  allowed=[{'address':int(at),'address_hex':hex(int(at)),'size':int(n),'name':names.get(int(at),'Unknown')} for at,n in (s.split() for s in subprocess.check_output([str(exe)],text=True).splitlines())]
 ordinary=PE(a.ordinary_dll).report();assert ordinary['sha256']==EXPECTED
 overlaps=[]
 for site in allowed:
  for family in ('hooks','patches'):
   for entry in ordinary[family]:
    if intersects(site,entry):overlaps.append({'toolbox':site,'ordinary_family':family,'ordinary':entry})
 assert not intersects({'address':0,'size':5},{'address':5,'size':2})
 assert intersects({'address':0,'size':5},{'address':4,'size':2})
 # Regression: a zero declared descriptor must cover its conservative envelope.
 assert intersects({'address':0x4381F2,'size':1},{'address':0x4381DA,'size':0,'coverage_size':25})
 assert not intersects({'address':0x4381F3,'size':1},{'address':0x4381DA,'size':0,'coverage_size':25})
 result={'scope':'All production GameEntryManager allowlisted write spans, including dormant sites; static ordinary binary descriptors. Dynamic writes separately recorded.',
 'toolbox_release':'v1.0.39-pre.6','toolbox_code_commit':PRODUCT_CODE_SHA,'toolbox_release_commit':PRODUCT_RELEASE_SHA,'toolbox_release_runtime_sha256':RELEASE_RUNTIME_SHA256,'upstream_source_commit':UPSTREAM_SHA,'ordinary_sha256':EXPECTED,
 'source_sha256':{str(v.relative_to(a.repo)):sha(v) for v in (policy,direct)},'parser_sha256':sha(Path(__file__).with_name('ordinary_pe.py')),
 'ordinary_hooks':len(ordinary['hooks']),'ordinary_patches':len(ordinary['patches']),'descriptor_coverage_method':coverage_method(ordinary),'verified_upstream_source_pin':source_pin,'toolbox_allowlist':allowed,'intersections':overlaps}
 (a.output_dir/'all-write-intersections.json').write_text(json.dumps(result,indent=2));print(json.dumps({'toolbox_sites':len(allowed),'ordinary_hooks':len(ordinary['hooks']),'ordinary_patches':len(ordinary['patches']),'intersections':overlaps},indent=2))
if __name__=='__main__':main()
