"""Execute production routing for both dodge modes and computation locations."""
from pathlib import Path
import argparse,subprocess,tempfile,json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;out=a.out or Path(tempfile.mkdtemp(prefix='dodge-routing-'));out.mkdir(parents=True,exist_ok=True)
s=(here.parent/'source/dodge_bridge.h').read_text()
def function(marker):
 start=s.index(marker);brace=s.index('{',start);end=brace+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
(out/'routing.h').write_text(function('static void dispatchDodgeCalculation(')+'\n'+function('static bool dodgeControlTick(')+'\n')
r=subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(out),'-I'+str(here.parent/'source'),str(here/'dodge_compute_routing.cpp'),'-o',str(out/'routing')],capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
r=subprocess.run([str(out/'routing')],capture_output=True,text=True,timeout=20);(out/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
(out/'result.json').write_text(json.dumps({'pass':True,'stdout':r.stdout,'source':'assist/source/dodge_bridge.h','native_game_executed':False},indent=2)+'\n')
