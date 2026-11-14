"""Fatal Host child processes must leave a record and a readable small dump."""
from pathlib import Path
import argparse,json,subprocess,struct
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
exe=a.out/'fatal-host.exe';r=subprocess.run([str(a.zig),'c++','-std=c++17','-target','x86_64-windows-gnu','-O1',str(Path(__file__).with_suffix('.cpp')),'-o',str(exe),'-luser32'],capture_output=True,text=True);(a.out/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
rows=[]
for name,kind,code in [('av','unhandled-native','C0000005'),('thread','std-terminate','E0000001')]:
 folder=a.out/name;folder.mkdir(exist_ok=True);child=subprocess.Popen([str(exe),str(folder),name],stdout=subprocess.PIPE,stderr=subprocess.PIPE);stdout,stderr=child.communicate(timeout=30)
 log=(folder/'HostCrash.log').read_text();dump=(folder/'HostCrash.dmp').read_bytes()
 assert child.returncode and kind in log and code in log and f'pid=0x{child.pid:016X}' in log and 'tid=' in log and 'moduleRva=' in log and 'dumpWritten=0x0000000000000001' in log,(name,child.returncode,log,stderr)
 assert dump[:4]==b'MDMP' and len(dump)>32
 streams=struct.unpack_from('<I',dump,8)[0];directory=struct.unpack_from('<I',dump,12)[0]
 assert streams and directory+streams*12<=len(dump)
 assert 3 in [struct.unpack_from('<I',dump,directory+i*12)[0] for i in range(streams)] # ThreadListStream
 rows.append(dict(name=name,exit=child.returncode,recorded_pid=child.pid,dump_bytes=len(dump),minidump_valid=True));print('PASS',name,'record, process/thread/code/module, MDMP thread stream; child exit',child.returncode)
(a.out/'result.json').write_text(json.dumps(dict(passed=True,cases=rows,windows_executed=True,game_tested=False),indent=2)+'\n')
