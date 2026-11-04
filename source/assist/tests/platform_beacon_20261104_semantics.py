"""Interpret the 2026-11-04 captured Beacon function; no private bytes in output.

Requires a separately supplied private Probe capture and GNU objdump. The
small interpreter rejects unsupported instructions and unmodelled addresses;
all five external platform callees are explicit fixtures. This is neither
Windows execution nor a real-game/multiplayer acceptance test.
"""
from pathlib import Path
import argparse, json, re, struct, subprocess, hashlib

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--capture',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
ROOT=args.capture.resolve()
args.output.parent.mkdir(parents=True,exist_ok=True)
report=json.loads((ROOT/'platform-signatures.json').read_text())
sig=next(s for s in report['signatures'] if s['address']=='0x431410')
session=json.loads((ROOT/'session.json').read_text())
assert session['file_sha256'].lower()=='ebb8b7bd07236c9ba7cbb2ad01bdca82b99faf2fccf08d79f7f60f59c4d77707'
assert set(bytes.fromhex(sig['snapshot_valid']))=={1} and set(bytes.fromhex(sig['snapshot_stable']))=={1}
assert sig['snapshot_actual']==sig['entry_later'][:16]
assert sig['target_region']['state']==0x1000 and sig['target_region']['type']==0x1000000 and sig['target_region']['protection']==0x20
BASE=int(sig['target_region']['allocation_base'],16); START=int(sig['rel32_target'],16)
MODULE_BYTES=0xC3A000; BODY_BYTES=0x127
body=(ROOT/sig['target_file']).read_bytes()[:BODY_BYTES]
valid=(ROOT/(sig['target_file']+'.valid')).read_bytes()[:BODY_BYTES]
assert len(body)==BODY_BYTES and all(valid)
assert START-BASE==0x153CD0
modules=json.loads((ROOT/'platform-capture.json').read_text())['modules']
assert any(m['name'].upper()=='MRGA1.DLL' and int(m['base'],16)==BASE and m['size']==MODULE_BYTES for m in modules)
normalized=bytearray(body)
for off in (4,63,99,105,279):
    value=struct.unpack_from('<I',body,off)[0]
    assert BASE<=value<BASE+MODULE_BYTES
    struct.pack_into('<I',normalized,off,value-BASE)
assert hashlib.sha256(normalized).hexdigest()=='e358b23e0021705cce6de0e00b10b08e3dbc8f9eca156f13cb0fe8f87ed0392c'
raw=subprocess.run(['objdump','-D','-b','binary','-m','i386','-M','intel','--no-show-raw-insn',
    '--adjust-vma='+hex(START),'--stop-address='+hex(START+BODY_BYTES),str(ROOT/sig['target_file'])],
    check=True,capture_output=True,text=True).stdout
instructions={}
for line in raw.splitlines():
    m=re.match(r'\s*([0-9a-f]+):\s*(\w+)\s*(.*)',line)
    if m and START<=int(m[1],16)<START+BODY_BYTES:
        instructions[int(m[1],16)]=(m[2],m[3].strip())
addresses=list(instructions)
following={a:addresses[i+1] if i+1<len(addresses) else START+BODY_BYTES for i,a in enumerate(addresses)}
assert len(instructions)==97 and instructions[START+0x124]==('ret','0x4')
assert set(x[0] for x in instructions.values()) <= {'push','pop','mov','lea','call','test','cmp','add','jne','je','ja','jge','jmp','ret'}

class Machine:
    def __init__(self):
        self.mem={};self.reg={r:0 for r in ('eax','ebx','ecx','edx','esi','edi','ebp','esp')}
        self.flags={'Z':False,'S':False,'O':False,'C':False};self.calls=[];self.steps=0
        self.guard=0;self.erase=None;self.deleted=[];self.freed=[]
    def mapped(self,a,n=1):
        return BASE<=a and a+n<=BASE+MODULE_BYTES or 0x20000000<=a and a+n<=0x20010000 or 0x30000000<=a and a+n<=0x30010000
    def load(self,a,n=4):
        assert self.mapped(a,n),('unmodelled read',hex(a),n)
        return sum(self.mem.get(a+i,0)<<(8*i) for i in range(n))
    def store(self,a,value,n=4):
        assert self.mapped(a,n),('unmodelled write',hex(a),n)
        for i in range(n):self.mem[a+i]=(value>>(8*i))&255
    def address(self,expression):
        expression=expression.strip()
        if expression.startswith('ds:'):expression=expression[3:]
        if expression.startswith('['):expression=expression[1:-1]
        assert re.fullmatch(r'[a-z0-9x+*]+',expression),expression
        value=0
        for term in expression.split('+'):
            product=1
            for factor in term.split('*'):product*=self.reg[factor] if factor in self.reg else int(factor,0)
            value+=product
        return value&0xffffffff
    def value(self,operand):
        if operand in self.reg:return self.reg[operand]
        if operand=='al':return self.reg['eax']&255
        if operand.startswith('ds:'):return self.load(self.address(operand))
        if operand.startswith('DWORD PTR '):return self.load(self.address(operand[10:]))
        if operand.startswith('BYTE PTR '):return self.load(self.address(operand[9:]),1)
        return int(operand,0)&0xffffffff
    def assign(self,operand,value):
        value&=0xffffffff
        if operand in self.reg:self.reg[operand]=value;return
        if operand.startswith('ds:'):self.store(self.address(operand),value);return
        if operand.startswith('DWORD PTR '):self.store(self.address(operand[10:]),value);return
        if operand.startswith('BYTE PTR '):self.store(self.address(operand[9:]),value,1);return
        raise AssertionError(('unsupported destination',operand))
    def push(self,value):
        self.reg['esp']=(self.reg['esp']-4)&0xffffffff;self.store(self.reg['esp'],value)
    def pop(self):
        value=self.load(self.reg['esp']);self.reg['esp']+=4;return value
    def subtract(self,a,b):
        result=(a-b)&0xffffffff
        self.flags={'Z':result==0,'S':bool(result&0x80000000),'O':bool((a^b)&(a^result)&0x80000000),'C':a<b}
    def stub(self,at):
        rva=at-BASE;self.calls.append(rva)
        if rva==0x1AAB20:self.reg['eax']=self.guard
        elif rva==0x1549E0:pass  # cdecl manager cleanup/prepare; explicit no-op fixture.
        elif rva==0x1545C0:self.deleted.append(self.load(self.reg['esp']+4))
        elif rva==0x4B404E:
            assert self.load(self.reg['esp']+8)==0x114
            self.freed.append(self.load(self.reg['esp']+4))
        elif rva==0x1552F0:
            assert self.reg['ecx']==BASE+0x7DE1F8
            self.erase=(self.load(self.reg['esp']+8),self.load(self.reg['esp']+12))
        else:raise AssertionError(('unmodelled call',hex(rva)))
        pc=self.pop()
        if rva==0x1552F0:self.reg['esp']+=12
        return pc
    def run(self,index,guard=0,state_flag=0,entries=None,count=8,array=True,slot=True):
        self.reg.update(ebx=0x11111111,esi=0x22222222,edi=0x33333333,ebp=0x44444444,esp=0x3000f000,ecx=0x89c3b0)
        before={r:self.reg[r] for r in ('ebx','esi','edi','ebp','esp')}
        self.guard=guard
        self.store(BASE+0xB6F4E8,0x20002000 if state_flag else 0)
        if state_flag:self.store(0x20002000+0x1716,state_flag,1)
        self.store(BASE+0x66F128,0x20000000);self.store(0x20000000+16,count)
        self.store(0x20000000+4,0x20001000 if array else 0)
        for i in range(8):self.store(0x20001000+4*i,1 if slot else 0)
        entries=entries or []
        self.store(BASE+0x7DE1F8,0x20005000);self.store(BASE+0x7DE1FC,0x20005000+8*len(entries))
        for i,owner in enumerate(entries):
            node=0x20006000+0x200*i if owner is not None else 0
            self.store(0x20005000+8*i,i+1);self.store(0x20005004+8*i,node)
            if node:self.store(node+0x110,owner)
        self.push(index&0xffffffff);self.push(0xDEAD0000);pc=START
        while pc!=0xDEAD0000:
            assert self.steps<3000,('instruction bound',hex(pc));self.steps+=1
            assert pc in instructions,('unmodelled code',hex(pc))
            op,operands=instructions[pc];next_pc=following[pc];args=operands.split(',') if operands else []
            if op=='push':self.push(self.value(args[0]))
            elif op=='pop':self.assign(args[0],self.pop())
            elif op=='mov':self.assign(args[0],self.value(args[1]))
            elif op=='lea':self.assign(args[0],self.address(args[1]))
            elif op=='cmp':self.subtract(self.value(args[0]),self.value(args[1]))
            elif op=='test':
                result=self.value(args[0])&self.value(args[1]);self.flags={'Z':result==0,'S':bool(result&0x80000000),'O':False,'C':False}
            elif op=='add':
                a,b=self.value(args[0]),self.value(args[1]);result=(a+b)&0xffffffff
                self.assign(args[0],result);self.flags={'Z':result==0,'S':bool(result&0x80000000),'O':bool((~(a^b))&(a^result)&0x80000000),'C':a+b>0xffffffff}
            elif op=='call':self.push(next_pc);next_pc=self.stub(self.value(args[0]))
            elif op=='ret':next_pc=self.pop();self.reg['esp']+=self.value(args[0]) if args else 0
            elif op=='jmp':next_pc=self.value(args[0])
            else:
                take={'je':self.flags['Z'],'jne':not self.flags['Z'],'ja':not self.flags['C'] and not self.flags['Z'],'jge':self.flags['S']==self.flags['O']}[op]
                if take:next_pc=self.value(args[0])
            pc=next_pc
        assert before=={r:self.reg[r] for r in before},('callee-saved/stack',before,self.reg)
        return {'steps':self.steps,'callee_rvas':[hex(x) for x in self.calls],'destroy_count':len(self.deleted),'free_count':len(self.freed),'erase':self.erase}

rows=[]
cases=[('owner_minus1',-1,{},0,False),('owner_zero',0,{},0,True),('owner_seven',7,{},0,True),('owner_eight',8,{},0,False),
    ('platform_guard',0,{'guard':1},0,False),('platform_state',0,{'state_flag':1},0,False),
    ('player_count_zero',0,{'count':0},0,False),('player_array_null',0,{'array':False},0,False),('player_slot_null',0,{'slot':False},0,False),
    ('mixed_owner_zero',0,{'entries':[0,7,0]},2,True),('mixed_owner_seven',7,{'entries':[0,7,0]},1,True),
    ('mixed_null_and_nonowner',7,{'entries':[None,0,None]},0,True)]
for name,index,kwargs,destroy,erase in cases:
    m=Machine();row=m.run(index,**kwargs)
    assert row['destroy_count']==destroy,(name,row)
    assert (m.erase is not None)==erase,(name,row)
    if name=='mixed_owner_zero':assert m.load(0x20005000)==2 and m.load(0x20005004)==0x20006200 and m.erase==(0x20005008,0x20005018)
    if name=='mixed_owner_seven':assert m.load(0x20005000)==1 and m.load(0x20005008)==3 and m.erase==(0x20005010,0x20005018)
    row.pop('erase');rows.append({'name':name,'pass':True,**row})
result={'pass':True,'scope':'bounded instruction interpreter over GNU objdump decoding of the actual captured 295-byte x86 function; five platform callees explicit fixtures; not Windows or real platform execution',
    'main_image_sha256':session['file_sha256'].lower(),'module':'MRGA1.DLL','module_image_bytes':MODULE_BYTES,'target_rva':'0x153CD0',
    'instruction_count':len(instructions),'body_bytes':BODY_BYTES,'body_normalized_sha256':hashlib.sha256(normalized).hexdigest(),
    'callee_saved_and_ret4_verified':True,'cases':rows,'private_bytes_published':False}
args.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='cases'},indent=2));print('PASS',len(rows),'captured instruction semantics cases')
