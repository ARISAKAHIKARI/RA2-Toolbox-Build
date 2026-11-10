"""Actual DLL managed sender adapter with native packing and decode fixtures.

Synthetic Win32, Houses and capture-only transport. Runs the installed DLL's
thunks, native 649CA0/64B660 and 64BDD0; protocol 0 wire records are parsed.
No live peer, platform transport, event effects or complete House scheduler.
AI/nonhuman Houses may be skipped by the original scheduler and are rejected
as foreign adapter targets. MegaMission byte 0D is native cache metadata.
Fault cases test safe reads and deferred mapping cleanup. An optional matching
PDB resolves the actual Abort body, not Windows SEH dispatch or exception unwind.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UcError
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');p.add_argument('--pdb',help='optional matching PDB for a direct Abort-body fault-safety check');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('network_runtime_support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
SITES={0x649ca0:bytes.fromhex('83ec1c5355'),0x649e1e:bytes.fromhex('888c40d602a800'),
       0x64b660:bytes.fromhex('81ec94000000'),0x64b9d4:bytes.fromhex('888c40d602a800'),
       0x64b4ea:bytes.fromhex('8bcac744241000000000'),
       0x649d61:bytes.fromhex('a14cb2a800'),0x649dbc:bytes.fromhex('8b5424148b742410')}
GPRS=(UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
FPREGS=(UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_MXCSR,
        UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7,
        UC_X86_REG_XMM0,UC_X86_REG_XMM1,UC_X86_REG_XMM2,UC_X86_REG_XMM3,UC_X86_REG_XMM4,UC_X86_REG_XMM5,UC_X86_REG_XMM6,UC_X86_REG_XMM7)
def fp(m):return tuple(m.u.reg_read(r) for r in FPREGS)
def canonical(b):
    q=bytearray(b)
    if q[0]==4:q[13]=0
    return bytes(q)
def check(name,fn):
    if a.filter and a.filter not in name:return
    try:
        details=fn();rows.append({'case':name,'status':'PASS',**(details or {})});print('PASS',name,flush=True)
    except Exception as error:
        rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()
def setup(mode=3,protocol=1):
    m=n.fixture(mode);m.vector(0xa8da74,[])
    for house,ident in ((m.house,100),(m.enemy,200)):
        typ=m.alloc(0x2000);m.w(house+0x34,typ);m.w(house+0x10,ident);m.u.mem_write(typ+0x24,b'Americans\0')
    m.b(m.house+0x1ec,1);m.b(m.enemy+0x1ed,1);m.w(0xa8b230,m.alloc(0x2000))
    m.w(0xa8b24c,protocol);m.w(0xa8b554,3);m.w(0xa8b54c,2);m.w(0xac51fc,0x12345678);m.b(0xa8ed74,0)
    s=n.state(m);s.nativeMegaGateway=0;s.networkHousePreserve=0;s.nativeSellCursor=0;n.save(m,s);n.tick(m)
    m.protocol=protocol;m.sent=[];m.onSend=None;m.transport=m.alloc();m.transportVT=m.alloc();m.buffer=m.alloc(0x10000)
    m.w(m.transport,m.transportVT);m.w(m.transportVT+4,m.STUB+0xe200);m.w(m.transportVT+8,m.STUB+0xe220)
    def transport(uc,at,size,_):
        sp=uc.reg_read(UC_X86_REG_ESP)
        if at==m.STUB+0xe220:
            data=bytes(uc.mem_read(m.r(sp+4),m.r(sp+8)));m.sent.append(data)
            assert m.r(sp+16)==0xffffffff
            if m.onSend:m.onSend(len(m.sent))
            m.ret(0,4)
        else:m.ret(0,0)
    for at in (m.STUB+0xe200,m.STUB+0xe220):m.u.hook_add(UC_HOOK_CODE,transport,begin=at,end=at)
    for at,size in ((0xac50fc,64*4),(0xafa468,64*640),(0xafa338,8*4),(0xafa42c,8*4),(0xac5230,8*640)):
        m.u.mem_write(at,bytes(size))
    return m
def toggle(m,on):
    s=n.state(m);s.networkHousePreserve=int(on);n.save(m,s);return n.tick(m)
def installed(m):
    for at,before in SITES.items():
        actual=bytes(m.u.mem_read(at,len(before)));assert actual[0]==0xe9 and actual!=before,(hex(at),actual.hex())
        # The legacy helper shares RuntimeControl storage. Inspect through an
        # independent Request buffer so later Runtime wakes remain valid.
        q=m.request(3);q.size=108;q.version=3;q.address=at;address=m.alloc(108)
        m.u.mem_write(address,bytes(q));m.run(m.manager_export,(address,))
        result=C.c_int32(m.u.reg_read(UC_X86_REG_EAX)).value;q=m.EntryRequest.from_buffer_copy(m.u.mem_read(address,108))
        assert result==1 and q.active and not q.conflict,(hex(at),result,q.active,q.conflict)
def restored(m):
    for at,before in SITES.items():assert bytes(m.u.mem_read(at,len(before)))==before,hex(at)
def fill(m,events,head=0):
    m.w(0xa802c8,len(events));m.w(0xa802cc,head);m.w(0xa802d0,(head+len(events))&127)
    m.w(0x8b41f8,0);m.w(0x8b41fc,0);m.w(0x8b4200,0);m.sent=[];m.inputEvents=events
    for i,(kind,house,ident) in enumerate(events):
        slot=(head+i)&127;at=0xa802d4+slot*111;m.u.mem_write(at,bytes(111));m.w(0xa83a54+slot*4,450000+i)
        if kind==4:m.run(0x4c6860,(house,ident,0x34,1,12345,0x34,30020,11,12346,0x34),at)
        elif kind==0x16:m.run(0x4c65e0,(house,kind,ident,0x34),at)
        else:m.run(0x4c66c0,(house,kind),at)
def logical_out(m):
    count=m.r(0xa802c8);head=m.r(0xa802cc)
    return [(bytes(m.u.mem_read(0xa802d4+((head+i)&127)*111,111)),m.r(0xa83a54+((head+i)&127)*4)) for i in range(count)]
def send(m,capacity=4096):
    m.u.reg_write(UC_X86_REG_EDX,m.buffer);m.run(0x649ca0,(capacity,5,100),m.transport)
    result=C.c_int32(m.u.reg_read(UC_X86_REG_EAX)).value
    local=[bytes(m.u.mem_read(0x8b4204+111*i,111)) for i in range(m.r(0x8b41f8))]
    return result,local
def decode(m):
    if m.protocol==0:return [packet[i:i+111] for packet in m.sent for i in range(0,len(packet),111)]
    m.w(0x8b41f8,0);m.w(0x8b41fc,0);m.w(0x8b4200,0)
    for packet in m.sent:
        m.u.mem_write(m.buffer,packet);m.u.reg_write(UC_X86_REG_EDX,len(packet));m.run(0x64bdd0,(),m.buffer)
    return [bytes(m.u.mem_read(0x8b4204+111*i,111)) for i in range(m.r(0x8b41f8))]
def protect(m):return {p:bytes(m.u.mem_read(p,size)) for p,size in ((m.house,0x20000),(m.enemy,0x20000),(m.unit,0x2000),(m.building,0x2000),(0xa83d4c,4))}
def unchanged(m,before):assert all(bytes(m.u.mem_read(p,len(data)))==data for p,data in before.items()),'sender changed simulation state'
def guarded_reads(m):
    # The common Win32 fixture assumes every RPM source is mapped. This scoped
    # replacement implements the real API's failure result for an unmapped
    # source, so a direct unsafe game-memory load still faults in Unicorn.
    address=next(p for p,name in m.imports.items() if name=='ReadProcessMemory')
    del m.imports[address];failed=[]
    def rpm(uc,at,size,_):
        sp=uc.reg_read(UC_X86_REG_ESP);source=m.r(sp+8);target=m.r(sp+12);count=m.r(sp+16);got=m.r(sp+20)
        try:data=bytes(uc.mem_read(source,count))
        except UcError:
            failed.append((source,count));m.w(got,0);m.ret(0,5);return
        uc.mem_write(target,data);m.w(got,count);m.ret(1,5)
    m.u.hook_add(UC_HOOK_CODE,rpm,begin=address,end=address)
    return failed
def mapping_lifetime(m):
    calls=[]
    for name in ('UnmapViewOfFile','CloseHandle'):
        address=next(p for p,value in m.imports.items() if value==name);del m.imports[address]
        def record(uc,at,size,_,name=name):
            calls.append((name,m.r(uc.reg_read(UC_X86_REG_ESP)+4)));m.ret(1,1)
        m.u.hook_add(UC_HOOK_CODE,record,begin=address,end=address)
    return calls
def pdb_public(m,path,name):
    # Resolve a non-exported test target from the matching native link artifact;
    # do not add diagnostic exports or patch the function under test.
    raw=Path(path).read_bytes();assert raw.startswith(b'Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0')
    block_size,_,_,directory_size,_,block_map=struct.unpack_from('<6I',raw,32)
    block_count=lambda size:(size+block_size-1)//block_size
    def stream_bytes(blocks,size):return b''.join(raw[i*block_size:(i+1)*block_size] for i in blocks)[:size]
    blocks=struct.unpack_from('<%dI'%block_count(directory_size),raw,block_map*block_size)
    directory=stream_bytes(blocks,directory_size);count=struct.unpack_from('<I',directory)[0]
    sizes=struct.unpack_from('<%dI'%count,directory,4);at=4+count*4;streams=[]
    for size in sizes:
        if size==0xffffffff:streams.append(b'');continue
        count=block_count(size);blocks=struct.unpack_from('<%dI'%count,directory,at);at+=count*4
        streams.append(stream_bytes(blocks,size))
    rsds=[m.dll.get_data(e.struct.AddressOfRawData,e.struct.SizeOfData) for e in m.dll.DIRECTORY_ENTRY_DEBUG if e.struct.Type==2]
    assert len(rsds)==1 and rsds[0][:4]==b'RSDS'
    assert streams[1][8:12]==rsds[0][20:24] and streams[1][12:28]==rsds[0][4:20],'PDB/DLL identity mismatch'
    found=set()
    for stream in streams:
        offset=0
        while True:
            index=stream.find(name+b'\0',offset)
            if index<0:break
            offset=index+len(name)+1
            if index<14:continue
            length,kind,flags,value,segment=struct.unpack_from('<HHIIH',stream,index-14)
            if kind==0x110e and flags&2 and 0<segment<=len(m.dll.sections):
                found.add(m.dll.OPTIONAL_HEADER.ImageBase+m.dll.sections[segment-1].VirtualAddress+value)
    assert len(found)==1,found
    return found.pop()
def watch_thunks(m):
    pending={};seen=[]
    continuations={0x649d66:0x649d61,0x649dc4:0x649dbc,0x649e25:0x649e1e,0x64b666:0x64b660,0x64b9db:0x64b9d4,0x64b551:0x64b4ea}
    def hook(uc,at,size,_):
        if at in continuations.values():pending[at]=(tuple(uc.reg_read(r) for r in GPRS),uc.reg_read(UC_X86_REG_EFLAGS),fp(m))
        elif at in continuations:
            site=continuations[at]
            if site not in pending:return # native cache path may reach the same continuation
            regs,flags,float_state=pending.pop(site);now=tuple(uc.reg_read(r) for r in GPRS)
            if site==0x649d61:
                assert now[0]==m.protocol and now[1:-1]==regs[1:-1],(hex(site),'protocol load / preserved registers')
            elif site==0x649dbc:
                assert all(now[k]==regs[k] for k in (0,1,2,5,6)),(hex(site),'unrelated registers')
                assert now[3]==m.r(regs[-1]+0x14) and 0<=now[4]<=m.r(regs[-1]+0x10),(hex(site),'native counter / packet bound')
            else:assert now[:-1]==regs[:-1],(hex(site),'registers',regs,now)
            assert now[-1]==regs[-1]-(0x94 if site==0x64b660 else 0),(hex(site),'stack')
            if site not in (0x64b660,0x649dbc):assert uc.reg_read(UC_X86_REG_EFLAGS)==flags,(hex(site),'flags')
            assert fp(m)==float_state,(hex(site),'x87/SSE state')
            seen.append(site)
    for at in set(continuations)|set(continuations.values()):m.u.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    return seen

MIXED=[(4,0,101),(4,0,102),(0x16,1,501),(4,1,101),(4,1,102),(8,0,0),(4,0,101),(4,0,102)]
for protocol in (0,1,2):
    def actual_command(protocol=protocol):
        m=setup(3,protocol);assert toggle(m,True).networkHouseStatus==1
        before=protect(m);s=n.state(m);s.request+=1;s.command=32;s.arg=m.enemy;s.arg2=200;s.requestToken=1
        s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s);s=n.tick(m)
        assert s.ack==s.request and s.result==1 and s.count==1 and m.r(0xa802c8)==1,(s.result,s.count,s.data.lastSubmitError)
        assert bytes(m.u.mem_read(0xa802d4,3))==bytes((8,0,1))
        sent,local=send(m);remote=[b for b in decode(m) if b[0]!=0x1c]
        assert sent==(0 if protocol==0 else 1) and len(local)==len(remote)==1 and local==remote and local[0][:3]==bytes((8,0,1))
        unchanged(m,before)
        return {'runtime_command':32,'foreign_house':1,'same_native_local_and_decoded_event':True}
    check(f'runtime_foreign_destruct_through_sender_protocol_{protocol}',actual_command)
for protocol in (0,1,2):
    def display_hidden_delivery(protocol=protocol):
        m=setup(3,protocol);assert toggle(m,True).networkHouseStatus==1
        # Submit through the actual Runtime command gateway, then hide the
        # tactical display before the engine consumes the accepted OutList.
        s=n.state(m);s.command=32;s.arg=m.enemy;s.arg2=200;s.requestToken=1
        s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s)
        s=m.hook();assert s.result==1 and m.r(0xa802c8)==1
        before=logical_out(m)
        m.b(0xa8e378,0)
        s=n.tick(m);assert s.data.commandStatus==10,s.data.commandStatus
        assert logical_out(m)==before
        # Hidden display still denies a *new* command, without deleting the old one.
        s.command=32;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s)
        m.hook();assert logical_out(m)==before
        sent,local=send(m);remote=[b for b in decode(m) if b[0]!=0x1c]
        assert sent==(0 if protocol==0 else 1) and len(local)==len(remote)==1,(sent,len(local),len(remote))
        assert local==remote and local[0][:3]==bytes((8,0,1))
        assert n.state(m).networkHouseDropped==0 and m.r(0xa802c8)==0
        return {'accepted_before_display_hidden':True,'new_input_denied':True,
                'foreign_destruct_delivered':True,'foreign_dropped':0}
    check(f'display_hidden_queued_destruct_protocol_{protocol}',display_hidden_delivery)
for protocol in (0,1,2):
    def own_wire(protocol=protocol):
        m=setup(3,protocol);assert toggle(m,True).networkHouseStatus==1
        fill(m,[(kind,0,ident) for kind,house,ident in MIXED],126)
        before=protect(m);crc_calls=[]
        trace=m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:crc_calls.append(at),begin=0x64dab0,end=0x64dab0)
        sent,local=send(m);m.u.hook_del(trace);unchanged(m,before)
        assert not crc_calls,'sender must not recompute game CRC / advance RNG'
        assert len(m.sent)==1 and m.sent[0][2]==0
        remote=[b for b in decode(m) if b[0]!=0x1c]
        assert [canonical(b) for b in local]==[canonical(b) for b in remote]
        return {'packet_sha256':[hashlib.sha256(packet).hexdigest() for packet in m.sent],
                'native_crc_recomputations':len(crc_calls),'event_houses':[b[2] for b in local]}
    check(f'own_wire_protocol_{protocol}',own_wire)

for mode in (3,4):
    for protocol in (0,1,2):
        def main(mode=mode,protocol=protocol):
            m=setup(mode,protocol);restored(m);assert n.state(m).networkHouseStatus==0
            s=toggle(m,True);assert s.networkHouseStatus==1,s.networkHouseStatus;installed(m)
            fill(m,MIXED,126);before=protect(m);seen=watch_thunks(m)
            seed=m.STUB+0xe300;m.u.mem_write(seed,bytes.fromhex('d9e8d9ebc3'));m.run(seed)
            m.u.reg_write(UC_X86_REG_FPCW,0x027f);m.u.reg_write(UC_X86_REG_MXCSR,0x5f80)
            m.u.reg_write(UC_X86_REG_XMM0,0x123456789abcdef0011223344556677);m.u.reg_write(UC_X86_REG_XMM7,0xfedcba98765432100123456789abcdef)
            original_fp=fp(m);sent,local=send(m);assert fp(m)==original_fp,'outer sender FP state'
            # Native protocol 0 returns zero despite consuming its records;
            # compressed protocols return the consumed event count.
            assert sent==(0 if protocol==0 else len(MIXED)) and m.r(0xa802c8)==0,(sent,len(MIXED),m.r(0xa802c8))
            remote=[b for b in decode(m) if b[0]!=0x1c]
            assert [canonical(b) for b in local]==[canonical(b) for b in remote]
            assert [b[2] for b in local]==[h for k,h,i in MIXED]
            expected={0x649d61,0x649dbc,0x649e1e} if protocol==0 else {0x649d61,0x64b660,0x64b9d4,0x64b4ea}
            # Three contiguous runs, including a three-event foreign run. No
            # regrouping across an intervening local run and no per-event split.
            assert len(m.sent)==3 and [packet[2] for packet in m.sent]==[0,1,0]
            assert all(struct.unpack_from('<I',packet,7)[0]==0x12345678 for packet in m.sent)
            assert all(struct.unpack_from('<H',packet,11)[0]==100 and packet[13]==5 for packet in m.sent)
            assert len({packet[3:7] for packet in m.sent})==1
            if protocol==0:assert [len(packet) for packet in m.sent]==[333,444,444]
            assert expected<=set(seen),(protocol,seen)
            unchanged(m,before)
            packets=len(m.sent);assert n.state(m).networkHousePackets==packets
            # Disable with unsent foreign orders. Keep each local event and its
            # exact timestamp in original order, even across ring wrap.
            fill(m,[(0x16,0,601),(8,1,0),(4,0,602),(0x16,1,603)],126)
            queued=logical_out(m);m.u.mem_write(0xafa338,struct.pack('<8I',*range(1,9)))
            s=toggle(m,False);assert s.networkHouseStatus==0,s.networkHouseStatus;restored(m)
            assert logical_out(m)==[queued[0],queued[2]] and s.networkHouseDropped==2
            assert bytes(m.u.mem_read(0xafa338,32))==bytes(32)
            unchanged(m,before)
            assert all(any(at<=address<at+len(data) for at,data in SITES.items()) for address,size in m.codeWrites),m.codeWrites
            return {'native_packets':packets,'packet_houses':[0,1,0],'foreign_events_in_one_packet':3,'native_crc_count_delay_preserved':True,'managed_sites':len(SITES),'leaf_register_fp_checks':len(seen),'foreign_cancelled':2}
        check(f'installed_native_sender_session_{mode}_protocol_{protocol}',main)

for case in ('singleplayer_0','singleplayer_5','unsupported_protocol','context_conflict','site_conflict','frame_site_conflict','plain_site_conflict','invalid_ring'):
    def reject(case=case):
        mode=0 if case=='singleplayer_0' else 5 if case=='singleplayer_5' else 3
        m=setup(mode,3 if case=='unsupported_protocol' else 1)
        if case=='context_conflict':m.u.mem_write(0x649d40,b'\x90')
        if case=='site_conflict':m.u.mem_write(0x64b9d4,b'\x90')
        if case=='frame_site_conflict':m.u.mem_write(0x649d61,b'\x90')
        if case=='plain_site_conflict':m.u.mem_write(0x649dbc,b'\x90')
        s=toggle(m,True)
        if case!='invalid_ring':
            assert s.networkHouseStatus!=1 and not m.codeWrites,(case,s.networkHouseStatus,m.codeWrites)
        else:
            assert s.networkHouseStatus==1;fill(m,[(8,1,0)]);m.w(0xa802d0,0)
            before=logical_out(m);sent,local=send(m)
            assert sent==0 and not local and not m.sent and logical_out(m)==before
        return {'blocked_without_event_send':True}
    check('rejection_'+case,reject)

for protocol in (0,1,2):
    def phase(protocol=protocol):
        m=setup(3,protocol);assert toggle(m,True).networkHouseStatus==1
        fill(m,[(0x16,0,701),(0x16,1,702),(8,0,0)])
        before=logical_out(m)
        def invalidate(first):
            if first==1:m.b(m.enemy+0x1ed,0)
        m.onSend=invalidate;sent,local=send(m);m.onSend=None
        assert sent==(0 if protocol==0 else 1) and len(m.sent)==1 and len(local)==1
        assert m.r(0xa802c8)==2 and [b[0] for b,t in logical_out(m)]==[0x16,8]
        # Next runtime entry cancels the now-invalid foreign remainder before
        # removing the protected sender; the queued local event remains.
        s=n.tick(m);assert s.networkHouseStatus!=1
        assert logical_out(m)==[before[2]] and s.networkHouseDropped==1
        return {'completed_prefix':1,'failed_group_not_consumed':True}
    check(f'callback_target_invalidation_protocol_{protocol}',phase)

for case in ('ai','observer','non_gameplay','invalid_index','wrong_thread'):
    def foreign(case=case):
        m=setup();assert toggle(m,True).networkHouseStatus==1
        fill(m,[(8,1,0),(0x16,0,801)])
        if case=='ai':m.b(m.enemy+0x1ed,0)
        elif case=='observer':m.w(0xac1198,m.enemy)
        elif case=='non_gameplay':m.b(0xa802d4,0x1c)
        elif case=='invalid_index':m.b(0xa802d6,31)
        else:m.current_thread=888
        sent,local=send(m)
        if case=='wrong_thread':
            assert sent==0 and not local and not m.sent and m.r(0xa802c8)==2
        else:
            assert sent==1 and len(local)==1 and local[0][0]==0x16 and local[0][2]==0
            assert n.state(m).networkHouseDropped==1
        return {'no_foreign_execution':True}
    check('foreign_filter_'+case,foreign)

for case in ('paused','heartbeat_expired','stopped','changed_world'):
    def readiness(case=case):
        m=setup();assert toggle(m,True).networkHouseStatus==1
        fill(m,[(8,1,0),(0x16,0,901)]);before=logical_out(m)
        if case=='paused':m.b(0xa8e9a0,0)
        elif case=='changed_world':m.w(0xa8b230,m.alloc(0x2000))
        else:
            s=n.state(m)
            if case=='heartbeat_expired':s.heartbeat=(m.clock-4000)&0xffffffff
            else:s.stop=1
            n.save(m,s)
        sent,local=send(m)
        if case=='changed_world':
            assert sent==0 and not local and not m.sent and logical_out(m)==before
        else:
            assert sent==1 and len(local)==1 and local[0][0]==0x16 and local[0][2]==0
            assert n.state(m).networkHouseDropped==1 and m.r(0xa802c8)==0
        return {'foreign_never_rewritten_as_local':True,'same_world_foreign_cancelled':case!='changed_world'}
    check('readiness_'+case,readiness)

def invalid_cached_house():
    m=setup();assert toggle(m,True).networkHouseStatus==1
    fill(m,[(8,1,0),(0x16,0,1001)],126);queued=logical_out(m)
    queue=bytes(m.u.mem_read(0xa802c8,12));failed=guarded_reads(m)
    page=m.house&~4095;m.u.mem_unmap(page,4096)
    sent,local=send(m)
    assert sent==0 and not local and not m.sent
    assert logical_out(m)==queued and bytes(m.u.mem_read(0xa802c8,12))==queue
    assert n.state(m).networkHouseStatus==-3
    assert any(page<=source<page+4096 for source,count in failed),failed
    installed(m)
    return {'cached_house_page_unmapped':True,'safe_read_failures':len(failed),'queue_unchanged':True,'gate_retained':True}
check('fault_boundary_cached_house_unmapped',invalid_cached_house)

for case in ('invalid_ring','changed_world'):
    def stop_deferred(case=case):
        m=setup();assert toggle(m,True).networkHouseStatus==1
        fill(m,[(8,1,0),(0x16,0,1101)],126);queued=logical_out(m)
        calls=mapping_lifetime(m);open_count=len(m.mapping_names)
        if case=='invalid_ring':m.w(0xa802d0,127)
        else:m.w(0xa8b230,m.alloc(0x2000))
        s=n.state(m);s.stop=1;n.save(m,s)
        for _ in range(2):
            s=n.tick(m)
            assert s.stop==1 and s.networkHouseStatus==-3,(s.stop,s.networkHouseStatus)
            assert logical_out(m)==queued
            assert ('UnmapViewOfFile',m.SH) not in calls and ('CloseHandle',1) not in calls,calls
            # Since HF24, a missing optional frame-notification mapping is
            # probed again. This fixture returns0 for that mapping; no new
            # Assist/Runtime mapping may replace the retained connection.
            assert all(name.startswith('Local\\RA2Frame_v1_') for name in m.mapping_names[open_count:]),m.mapping_names[open_count:]
            installed(m)
        if case=='invalid_ring':m.w(0xa802d0,0)
        else:
            # A new world's queue may become empty through the game itself.
            # The old adapter must not infer ownership or edit it to get here.
            m.w(0xa802c8,0);m.w(0xa802d0,126)
        s=n.tick(m)
        assert s.stop==2 and s.networkHouseStatus==0,(s.stop,s.networkHouseStatus)
        restored(m)
        assert ('UnmapViewOfFile',m.SH) in calls and ('CloseHandle',1) in calls,calls
        if case=='invalid_ring':
            assert logical_out(m)==[queued[1]] and s.networkHouseDropped==1
        else:assert m.r(0xa802c8)==0 and s.networkHouseDropped==0
        return {'deferred_stop_ticks':2,'mapping_and_hooks_retained_until_safe':True,'final_stop_ack':2,
                'foreign_cancelled':1 if case=='invalid_ring' else 0}
    check('fault_boundary_stop_deferred_'+case,stop_deferred)

if a.pdb:
    def abort_without_world():
        m=setup();assert toggle(m,True).networkHouseStatus==1
        fill(m,[(8,1,0)]);queued=logical_out(m)
        entry=pdb_public(m,a.pdb,b'_AssistNetworkSenderAbort');writes=[]
        m.u.mem_unmap(m.house&~4095,4096)
        m.u.mem_unmap(m.SH,(C.sizeof(m.Shared)+4095)&~4095)
        m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,size,value,_:writes.append((at,size)))
        m.run(entry)
        lo=m.dll.OPTIONAL_HEADER.ImageBase;hi=lo+m.dll.OPTIONAL_HEADER.SizeOfImage
        assert writes and all(lo<=at and at+size<=hi for at,size in writes),writes
        assert logical_out(m)==queued
        return {'direct_abort_rva':hex(entry-lo),'shared_and_cached_house_unmapped':True,
                'only_dll_local_writes':len(writes),'windows_seh_dispatch_tested':False}
    check('fault_boundary_abort_body_invalid_world_and_shared',abort_without_world)

Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
    'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
    'pdb_sha256':hashlib.sha256(Path(a.pdb).read_bytes()).hexdigest() if a.pdb else None,
    'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']!='PASS' for row in rows))
