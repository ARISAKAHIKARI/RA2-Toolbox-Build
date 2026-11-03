"""Build the real ore observer/search and complete production admission functions.

Synthetic private memory and the nativeMission event boundary are explicit seams;
no game process, mouse input, native pathfinder or networking is operated.
"""
import argparse,hashlib,json,subprocess,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
SRC=HERE.parent/'source'

def function(text,needle):
    start=text.index(needle);brace=text.index('{',start);at=brace+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]

def native_paths(path):
    import pefile
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
    raw=path.read_bytes();pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    heap,stack,stop=0x20000000,0x21000000,0x22000000
    for at,n in ((heap,0x10000),(stack,0x10000),(stop,4096)):u.mem_map(at,n)
    def w(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
    def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
    def invoke(entry,self):
        sp=stack+0x8000;w(sp,stop);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,self);u.emu_start(entry,stop,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+4;return u.reg_read(UC_X86_REG_EAX)
    cell,overlay,tib,overlays,tibs=heap,heap+0x1000,heap+0x2000,heap+0x3000,heap+0x4000
    w(cell+0x44,0);w(overlays,overlay);w(0xA83D84,overlays);u.mem_write(overlay+0x2A9,b'\1');w(overlay+0x294,0)
    w(tibs,tib);w(0xB0F4EC,tibs);w(0xB0F4F8,1);w(tib+0xE0,overlay);w(tib+0xE8,1);w(tib+0xEC,0);w(tib+0x98,0);w(tib+0xB8,25)
    values=[]
    for amount in (0,1,255):
        u.mem_write(cell+0x11E,bytes([amount]));actual=invoke(0x485020,cell);assert actual==25*(amount+1);values.append({'overlay_data':amount,'native_value':actual})
    w(cell+0x44,0xffffffff);assert invoke(0x485020,cell)==0;w(cell+0x44,0);u.mem_write(overlay+0x2A9,b'\0');assert invoke(0x485020,cell)==0
    # Execute the exact original Action::Harvest switch arm. The coordinate
    # lookup and final virtual ClickedMission are explicit read/event seams.
    bucket=pe.get_data(0x4D8270-base+5,1)[0];target=struct.unpack('<I',pe.get_data(0x4D8244-base+bucket*4,4))[0];assert target==0x4D7E8E
    unit,table,coord,clicked=heap+0x5000,heap+0x6000,heap+0x7000,heap+0x8000
    w(unit,table);w(table+0x378,clicked);u.mem_write(coord,struct.pack('<2h',56,62));records=[]
    def ret(argc,result=0):
        sp=u.reg_read(UC_X86_REG_ESP);ip=r(sp);u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4+4*argc);u.reg_write(UC_X86_REG_EIP,ip)
    def seam(cpu,at,size,user):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if at==0x5657A0:
            assert cpu.reg_read(UC_X86_REG_ECX)==0x87F7E8 and r(sp+4)==coord;ret(1,cell)
        elif at==clicked:
            args=tuple(r(sp+4+4*i)for i in range(4));assert cpu.reg_read(UC_X86_REG_ECX)==unit and args==(10,0,cell,0);records.append(args);ret(4,1)
    h1=u.hook_add(UC_HOOK_CODE,seam,begin=0x5657A0,end=0x5657A0);h2=u.hook_add(UC_HOOK_CODE,seam,begin=clicked,end=clicked)
    sp=stack+0x8000;u.mem_write(sp,bytes(0x38));w(sp+0x24,stop);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,unit);u.reg_write(UC_X86_REG_EDI,coord);u.emu_start(target,stop,count=1000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+0x38 and len(records)==1
    u.hook_del(h1);u.hook_del(h2)
    assert hashlib.sha256(path.read_bytes()).digest()==hashlib.sha256(raw).digest()
    return {'game_sha256':hashlib.sha256(raw).hexdigest(),'game_hash_stable':True,'resource_entry':'0x485020','resource_cases':values,'no_overlay_and_nonresource_return_zero':True,'harvest_action':6,'native_switch_arm':hex(target),'native_clicked_mission':{'mission':10,'target':None,'destination':'clicked cell','nearest':None},'native_stack_cleanup_valid':True,'game_process_operated':False,'redraw_or_pathfinder_called':False}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--game',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--observer-header',type=Path,help='optional frozen baseline header for an isolated counterexample');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    source=(SRC/'external_orders_bridge.h').read_text(encoding='utf-8')
    (a.out/'ore_eligibility_extract.h').write_text('\n'.join(function(source,n)for n in ('static bool oreTypeAllowed(','static bool oreMiner(','static bool rawOreCompatible(','static bool rawDockBusy(','static bool rawDockLinked('))+'\n',encoding='utf-8')
    (a.out/'ore_admission_extract.h').write_text('\n'.join(function(source,n)for n in ('static bool externalOrderValid(','static int admitPlannerOrder('))+'\n',encoding='utf-8')
    report={'scope':__doc__,'architectures':{}}
    for arch in ('x86','x86_64'):
        exe=a.out/(arch+'.exe');cmd=[str(a.zig.resolve()),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu','-I'+str(a.out.resolve()),'-I'+str(SRC.resolve()),str(HERE/'ore_harvest_handoff.cpp'),'-o',str(exe)]
        if a.observer_header:cmd.append('-DORE_HARVEST_OBSERVER_HEADER='+json.dumps(str(a.observer_header.resolve()).replace('\\','/')))
        build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='')
        if run.returncode:
            report['architectures'][arch]={'pass':False,'exit_code':run.returncode,'log':run.stdout+run.stderr}
            report['observer_header_sha256']=hashlib.sha256((a.observer_header or SRC/'ore_harvest_bridge.h').read_bytes()).hexdigest();(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');run.check_returncode()
        report['architectures'][arch]=json.loads(run.stdout)
    if a.game:report['native_paths']=native_paths(a.game)
    names=[SRC/'ore_harvest_bridge.h',SRC/'ore_harvest_search.h',SRC/'external_orders_bridge.h',SRC/'planner_types.h',SRC/'ore_queue.h',SRC/'bridge.cpp',HERE/'ore_harvest_handoff.cpp',Path(__file__).resolve()]
    report['source_sha256']={str(n.relative_to(HERE.parents[1])).replace('\\','/'):hashlib.sha256(n.read_bytes()).hexdigest()for n in names}
    (a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
