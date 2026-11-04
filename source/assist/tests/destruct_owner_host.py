"""Compile the actual Host send/one-shot/key/config/UI routes on both ABIs."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;src=here.parent/'source';out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
texts={name:(src/name).read_text(encoding='utf-8-sig') for name in ('main.cpp','hotkey_host.h','extra_settings_io.h','auto_settings_ui.h','type_bindings_ui.h','tactical_controls.h','additional_settings.h','destruct_owner_input.h','runtime_feature_demand.h')}
def block(text,needle):
 start=text.index(needle);at=text.index('{',start);end=at+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
def statement(text,needle):
 start=text.index(needle);return text[start:text.index(';',start)+1]
main=texts['main.cpp'];keys=texts['hotkey_host.h'];ui=texts['auto_settings_ui.h'];io=texts['extra_settings_io.h']
generated='\n'.join(block(main,n) for n in ('static bool userCommand(','static bool queueCommand(','static void send(int command','static void submitDestructSelectedOwner(){'))+'\n'
generated+=block(keys,'static bool dispatchHotkeyGroup(')+'\n'+block(keys,'static void handleHotkey(')+'\n'
generated+='static void fixtureExpired(){hotkeyroute::Group delayed;'+block(keys,'while(hotkeyRegistry.expired(')+'}\n'
generated+='static void fixtureQueued(WPARAM wp){'+block(keys,'if(wp==destructownerhost::Hotkey)')+'}\n'
generated+='\n'.join(block(texts['type_bindings_ui.h'],n) for n in ('static WORD comparableKey(','static bool keyConflict(','static bool destructOwnerKeyConflict('))+'\n'
generated+='static bool fixtureUI(int id,int notification){const bool edit=notification==EN_CHANGE;'+block(ui,'if(id==9253&&')+block(ui,'if(id==9254&&')+'return false;}\n'
load='\n'.join(statement(io,'extra.'+n+'=') for n in ('destructSelectedOwner','destructSelectedOwnerKey'))
save='\n'.join(statement(io,n) for n in ('write(L"Experimental",L"DestructSelectedOwner",','write(L"Hotkeys",L"DestructSelectedOwner",'))
generated+='static void fixtureLoad(){'+load+'}\nstatic void fixtureSave(){'+save+'}\n'
inc=out/'production-routes.inc';inc.write_text(generated,encoding='utf-8')
assert 'if(c==DestructPlayer)return false;' in block(main,'static bool queueCommand(')
assert 'destructownerhost::exclusive(hotkeyRegistry,extra.destructSelectedOwnerKey)' in block(main,'static void registerKeys()')
assert 'bool destructSelectedOwner=false;uint32_t destructSelectedOwnerKey=0;' in texts['additional_settings.h']
assert '原生武器／射程查询与 CRC 原值镜像不标高危' not in texts['tactical_controls.h']
assert '高危：全图 CRC 原值镜像' in texts['tactical_controls.h']
assert 'send(DestructPlayer,int(selected.pointer)' not in main
results={}
for arch in ('x86_64','x86'):
 exe=out/(arch+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(here/'destruct_owner_host.cpp'),'-DDESTRUCT_HOST_ROUTES="'+inc.as_posix()+'"','-o',str(exe),'-luser32']
 built=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8')
 if built.returncode:raise SystemExit(built.stdout+built.stderr)
 ran=subprocess.run([str(exe)],capture_output=True,text=True);(out/(arch+'.log')).write_text(ran.stdout+ran.stderr,encoding='utf-8')
 if ran.returncode:raise SystemExit(ran.stdout+ran.stderr)
 results[arch]=json.loads(ran.stdout.strip().splitlines()[-1])
report={'results':results,'sources':{n:hashlib.sha256((src/n).read_bytes()).hexdigest() for n in texts},'scope':'actual Host command and hotkey routes; simulated Win32; no game process or network test'}
(out/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
