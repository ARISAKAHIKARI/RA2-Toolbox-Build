"""Compile and execute production EjectAssist watcher with controllable native fields."""
from pathlib import Path
import argparse,subprocess
here=Path(__file__).resolve().parent
source=(here.parent/'source'/'native_orders_bridge.h').read_text()
marker='static void watchEjections(uint32_t house,int frame,Vec all){'
start=source.index(marker);at=source.index('{',start);end=at+1;depth=1
while depth:
 depth+=(source[end]=='{')-(source[end]=='}');end+=1
parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
args.out.mkdir(parents=True,exist_ok=True)
inc=args.out/'watch_ejections.inc';inc.write_text(source[start:end]+'\n')
exe=args.out/'fixture'
for cmd in (['g++','-std=c++17','-O1','-Wall','-Wextra','-UNDEBUG',
             '-DEJECTION_SOURCE="'+str(inc.resolve())+'"',
             str(here/'unload_ejection_runtime.cpp'),'-o',str(exe)],[str(exe)]):
 run=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
 print(run.stdout+run.stderr,flush=True,end='')
 if run.returncode:raise SystemExit(run.returncode)
