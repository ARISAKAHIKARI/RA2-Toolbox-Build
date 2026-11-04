#!/usr/bin/env python3
"""Run the read-only ordinary-Spawner interval and guard audits in order."""
import argparse,json,subprocess,sys
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo',required=True,type=Path)
    p.add_argument('--ordinary-dll',required=True,type=Path)
    p.add_argument('--upstream-source',required=True,type=Path)
    p.add_argument('--output-dir',required=True,type=Path)
    a=p.parse_args();here=Path(__file__).resolve().parent
    args=['--repo',str(a.repo),'--ordinary-dll',str(a.ordinary_dll),'--upstream-source',str(a.upstream_source),'--output-dir',str(a.output_dir)]
    for name in ('audit_writes.py','dynamic_intervals.py','audit_guards.py'):
        subprocess.run([sys.executable,str(here/name),*args],check=True)
    write=json.loads((a.output_dir/'all-write-intersections.json').read_text())
    dynamic=json.loads((a.output_dir/'dynamic-write-intersections.json').read_text())
    guards=json.loads((a.output_dir/'guard-intersections.json').read_text())
    print(json.dumps({'toolbox_sites':len(write['toolbox_allowlist']),'ordinary_descriptors':write['ordinary_hooks']+write['ordinary_patches'],'physical_intersections':len(write['intersections']),'public_dynamic_calls':len(dynamic['calls']),'dynamic_intersections':len(dynamic['toolbox_write_intersections']),'unique_guard_ranges':guards['unique_address_size_ranges'],'additional_guard_intersections':len(guards['descriptor_intersections'])+len(guards['dynamic_public_write_intersections'])},indent=2))
if __name__=='__main__':main()
