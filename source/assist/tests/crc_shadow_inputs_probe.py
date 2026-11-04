"""Offline native CRC input-substitution proof; no installable game hook.

The real game-memory MapIsClear byte stays changed. Only the emulated BL load
at64DCCA is substituted. This does not prove field instrumentation coverage,
display-only semantics, a complete Reveal or any multiplayer compatibility.
"""
import hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_EIP

fixture=Path(__file__).with_name('network_state_capture.py')
prefix=fixture.read_text().split('\nrows=[crc_case(c)',1)[0]
ns={'__name__':'crc_input_fixture','__file__':str(fixture)}
exec(compile(prefix,str(fixture),'exec'),ns)
Base=ns['Probe'];S=ns['S'];house=S+4*0x2000;rows=[]

def execute(case,physical=None,original=None):
    observations=[]
    class Probe(Base):
        def __init__(self):
            super().__init__();self.loads=0;self.calls=0
            assert bytes(self.u.mem_read(0x64dcca,6))==bytes.fromhex('8a9941020000')
            self.u.hook_add(UC_HOOK_CODE,self.enter,begin=0x64dab0,end=0x64dab0)
            self.u.hook_add(UC_HOOK_CODE,self.read_input,begin=0x64dcca,end=0x64dcca)
        def enter(self,u,at,size,_):
            self.calls+=1
            if physical is not None:self.b(house+0x241,physical)
        def read_input(self,u,at,size,_):
            self.loads+=1
            if original is not None:
                prior=u.reg_read(UC_X86_REG_EBX)
                u.reg_write(UC_X86_REG_EBX,(prior&0xffffff00)|original)
                u.reg_write(UC_X86_REG_EIP,0x64dcd0)
        def run(self,*args,**kw):
            result=super().run(*args,**kw)
            if args[0]==0x64dab0:
                observations.append({'crc_calls':self.calls,'House_input_reads':self.loads,
                    'physical_MapIsClear_after':self.u.mem_read(house+0x241,1)[0],
                    'rng_indices_after':[self.r(S+5*0x2000+0x218+4),self.r(S+5*0x2000+0x218+8)]})
            return result
    ns['Probe']=Probe;row=ns['crc_case'](case);row.update(observations[-1]);row['physical_override']=physical;row['shadow_original']=original
    assert row['crc_calls']==row['House_input_reads']==1
    if physical is not None:assert row['physical_MapIsClear_after']==physical
    rows.append(row);return row['native_crc']

baseline=execute('baseline');modified=execute('map_clear');assert baseline!=modified
assert execute('baseline',1,0)==baseline
for case in ['x_step','direction','random','order','count']:
    unmodified=execute(case);assert unmodified!=baseline
    assert execute(case,1,0)==unmodified
assert execute('map_clear',1,1)==modified # Legitimate reveal must update the shadow too.
report={'scope':__doc__,'executable_sha256':hashlib.sha256(Path(ns['a'].game).read_bytes()).hexdigest() if ns['a'].game else None,
    'field':'House+0x241 MapIsClear','native_input':'64DCCA ->64DCD0','installable_hook':False,
    'production_enabled':False,'game_tested':False,'multiplayer_tested':False,
    'logger_mocked':True,'limitations':['No complete world tick or Reveal executed','Natural-writer coverage not proved','No platform integrity or multiplayer validation','Native logger is mocked as in the reused baseline; RNG counts are relative to that same fixture'],
    'results':rows}
Path(ns['a'].output).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(rows),'native CRC input cases; changed live byte retained, normal motion/direction/RNG/order/count still detected; research only')
