"""Compile the actual placement completion header with fail-closed native UI seams.

No game process, input or network operation is performed. Optional private PE
validation executes only the two original nullptr setter paths in x86 emulation,
with their redraw marker primitive replaced by an explicit checked seam.
"""
import argparse, hashlib, json, struct, subprocess
from pathlib import Path

HERE=Path(__file__).resolve().parent
SRC=HERE.parent/'source'

def native_setter_paths(pe):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
    base=pe.OPTIONAL_HEADER.ImageBase;image=pe.get_memory_mapped_image();results=[]
    map_at=0x87F7E8;stack,stop=0x20000000,0x21000000
    for entry,position,center,foundation,marker in ((0x4A8BF0,0x1174,0x1178,0x117C,0x4A95A0),(0x4A8D50,0x1182,0x1186,0x118C,0x4A9650)):
        for existing in (False,True):
            u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,image)
            u.mem_map(stack,4096);u.mem_map(stop,4096)
            u.mem_write(map_at+position,struct.pack('<2h',2,3));u.mem_write(map_at+center,struct.pack('<2h',4,5));u.mem_write(map_at+foundation,struct.pack('<I',0x22220000 if existing else 0));u.mem_write(0x8A03F8,struct.pack('<I',0xFFFFFFFF))
            identity=struct.pack('<3I',0x20020000,0x20030000,3)
            for offset in (0x1190,0x11A4):u.mem_write(map_at+offset,identity)
            sp=stack+2048;u.mem_write(sp,struct.pack('<2I',stop,0));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,map_at)
            saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x3456789A,UC_X86_REG_EBP:0x456789AB}
            for reg,value in saved.items():u.reg_write(reg,value)
            redraw=[];writes=[]
            def code(cpu,address,size,user):
                if address!=marker:return
                top=cpu.reg_read(UC_X86_REG_ESP);ret,cell,mode=struct.unpack('<3I',cpu.mem_read(top,12))
                assert cpu.reg_read(UC_X86_REG_ECX)==map_at and mode==0
                redraw.append(tuple(struct.unpack('<2h',cpu.mem_read(cell,4))));assert redraw[-1]==(6,8)
                cpu.reg_write(UC_X86_REG_ESP,top+12);cpu.reg_write(UC_X86_REG_EIP,ret)
            def write(cpu,access,address,size,value,user):
                if stack<=address and address+size<=stack+4096:return
                assert map_at+center<=address and address+size<=map_at+foundation+4,(hex(entry),hex(address),size)
                writes.append((address,size))
            u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write);u.emu_start(entry,stop,count=1000)
            assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+8
            assert all(u.reg_read(reg)==value for reg,value in saved.items())
            assert struct.unpack('<I',u.mem_read(map_at+foundation,4))[0]==0
            assert struct.unpack('<I',u.mem_read(map_at+center,4))[0]==0xFFFFFFFF
            assert all(bytes(u.mem_read(map_at+offset,12))==identity for offset in (0x1190,0x11A4))
            assert len(redraw)==int(existing)
            results.append({'entry':hex(entry),'existing_foundation':existing,'redraw_marker_calls':len(redraw),'foundation_only_writes':len(writes),'thiscall_ret4_and_nonvolatile_abi':True,'cursor_identity_unchanged':True,'pass':True})
    return results

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--game',type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    report={'scope':__doc__,'architectures':{}}
    if a.game:
        import pefile
        raw=a.game.read_bytes();pe=pefile.PE(data=raw);fingerprints={}
        for entry,expected in ((0x4A8BF0,0xC44D9D5E),(0x4A8D50,0xDC84E30E)):
            body=pe.get_data(entry-pe.OPTIONAL_HEADER.ImageBase,12);assert len(body)==12;fnv=2166136261
            for b in body:fnv=((fnv^b)*16777619)&0xffffffff
            assert fnv==expected,(hex(entry),hex(fnv));fingerprints[hex(entry)]=f'{fnv:08X}'
        report.update(game_sha256=hashlib.sha256(raw).hexdigest(),native_setter_fnv12=fingerprints,native_setter_paths=native_setter_paths(pe))
    for arch in ('x86','x86_64'):
        exe=a.out/(arch+'.exe');cmd=[str(a.zig.resolve()),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu',str(HERE/'native_placement_completion.cpp'),'-o',str(exe)]
        build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();report['architectures'][arch]=json.loads(run.stdout)
    if a.game:
        assert hashlib.sha256(a.game.read_bytes()).hexdigest()==report['game_sha256'];report['game_hash_stable']=True
    names=[SRC/'native_placement_completion_bridge.h',HERE/'native_placement_completion.cpp',Path(__file__).resolve(),SRC/'native_placement_policy.h']
    report['source_sha256']={str(n.relative_to(HERE.parents[1])):hashlib.sha256(n.read_bytes()).hexdigest()for n in names}
    (a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
